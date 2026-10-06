#include "Wheel.h"
#include "RuntimeSupport.h"
#include "Outfits.h"
#include "Settings.h"
#include "InputGate.h"
#include "OpenPolicy.h"
#include "ActorRuntime.h"
#include "ActionPolicy.h"
#include "UIResources.h"
#include "UILayout.h"
#include "WheelFonts.h"
#include "TextBridgeClient.h"
#include "NameHitMap.h"
#include <chrono>
#include <optional>
#include <Windows.h>

namespace Wheel {
    namespace {
        std::mutex viewMutex;
        View view;
        std::vector<Item> allItems;
        int favoritePage=0,functionPage=0;
        bool favoriteCategoryChosen=false; // Session-local navigation, independent of wheel mode.
        FaceLight::Section functionCategory=FaceLight::Section::Outfits; // Remember the top-level type, not a child list.
        bool shiftHeld[2]{};
        float nameRepeatAt[256]{};
        std::atomic<bool> gameActive{false};
        std::atomic<std::uint64_t> epoch{0};
        std::atomic<bool> closing{false};
        std::atomic<bool> openQueued{false};
        std::atomic<std::uint64_t> openSerial{0};
        struct PendingOpen {
            bool functions;
            std::uint64_t generation, serial;
            std::chrono::steady_clock::time_point deadline;
        };
        std::mutex openMutex;
        std::optional<PendingOpen> pendingOpen;
        REL::Relocation<void (*)(RE::PlayerCharacter*, float)> previousPlayerUpdate;
        bool wheelInstalled=false, playerUpdateInstalled=false;
        std::atomic<bool> updateLogged{false};
        std::atomic<float> viewportWidth{1280}, viewportHeight{900};
        InputGate inputGate;
        using Dispatch = void(RE::BSTEventSource<RE::InputEvent*>*, RE::InputEvent**);
        REL::Relocation<Dispatch> previousDispatch;
        std::atomic<std::uint64_t> dispatchCount{0};
        struct PendingAction {
            Item item;
            bool left;
            std::uint64_t generation;
            std::chrono::steady_clock::time_point deadline;
        };
        std::mutex actionMutex;
        std::optional<PendingAction> pendingAction;
        std::atomic<bool> actionTaskQueued{false};
        std::uint64_t outfitGeneration=0; // SKSE task thread only.

        bool HasPendingAction() { std::lock_guard lock(actionMutex); return pendingAction.has_value(); }

        bool Focused() {
            DWORD process = 0;
            GetWindowThreadProcessId(GetForegroundWindow(), &process);
            return process == GetCurrentProcessId();
        }
        const char* BlockingMenu() {
            auto ui = RE::UI::GetSingleton();
            if (!ui) return "UI unavailable";
            // Includes unpaused inventory/dialogue mods; checking GameIsPaused alone is insufficient.
            for (auto name : {"Main Menu", "Loading Menu", "Console", "Dialogue Menu", "InventoryMenu", "MagicMenu",
                     "ContainerMenu", "BarterMenu", "GiftMenu", "Crafting Menu", "Book Menu", "Journal Menu", "MapMenu",
                     "RaceSex Menu", "MessageBoxMenu", "Lockpicking Menu", "Training Menu", "Sleep/Wait Menu", "FavoritesMenu"})
                if (ui->IsMenuOpen(name)) return name;
            return nullptr;
        }
        bool BlockedMenu() { return BlockingMenu() != nullptr; }
        bool ValidPlayer() {
            auto player = RE::PlayerCharacter::GetSingleton();
            auto menus = RE::MenuControls::GetSingleton();
            return menus && PlayerEligible(gameActive, player != nullptr, player && player->Get3D(),
                ActorRuntime::IsDead(player), menus->InBeastForm());
        }
        const char* OpenBlockReason() {
            auto ui = RE::UI::GetSingleton();
            auto controls = RE::ControlMap::GetSingleton();
            if (!Config().enabled) return "disabled";
            if (!RendererReady()) return "renderer not ready";
            if (closing) return "wheel still closing";
            if(Outfits::Busy())return "outfit changing";
            if (HasPendingAction()) return "item action pending";
            if (!gameActive) return "game not active";
            auto player = RE::PlayerCharacter::GetSingleton();
            if (!player) return "player unavailable";
            if (!player->Get3D()) return "player 3D unavailable";
            if (ActorRuntime::IsDead(player)) return "player dead";
            auto menus = RE::MenuControls::GetSingleton();
            if (!menus) return "menu controls unavailable";
            if (menus->InBeastForm()) return "beast form: yielding to vanilla";
            if (!Focused()) return "game not foreground";
            if (!ui) return "UI unavailable";
            if (const auto menu = BlockingMenu()) return menu;
            if (ui->GameIsPaused()) return "another menu pauses game";
            if (!controls) return "control map unavailable";
            if (!controls->IsMenuControlsEnabled()) return "menu controls disabled";
            if (controls->GetRuntimeData().textEntryCount > 0) return "text input active";
            return nullptr;
        }
        void FilterItems() { // viewMutex held
            if(view.functions)return;
            view.items.clear();
            for (const auto& item : allItems) if (item.category == view.category) view.items.push_back(item);
            view.page = std::clamp(view.page, 0, PageCount(view.items.size()) - 1);
        }
        void RefreshFunctions() {
            const auto section=Snapshot().functionSection;
            auto entries=section==FaceLight::Section::Outfits?Outfits::Entries():FaceLight::Entries(section);
            std::lock_guard lock(viewMutex);
            view.items=std::move(entries);view.page=std::clamp(view.page,0,PageCount(view.items.size())-1);
        }
        void SwitchWheel() {
            bool functions;
            {std::lock_guard lock(viewMutex);
                if(view.functions){if(view.functionSection==FaceLight::Section::Outfits)functionPage=view.page;}else favoritePage=view.page;
                functionCategory=FaceLight::VisibleSection(functionCategory,view.faceLightAvailable);
                functions=view.functions=!view.functions;view.functionSection=functionCategory;
                view.page=functions?(functionCategory==FaceLight::Section::Outfits?functionPage:0):favoritePage;view.x=view.y=0;
                if(!functions){favoriteCategoryChosen=true;FilterItems();}
            }
            if(functions)RefreshFunctions();
        }
        void Open(bool functions) {
            FaceLight::Capture(); // After the previous player-update hook, before pause.
            allItems = CollectFavorites();
            auto names=Outfits::Names()+FaceLight::Names();for(const auto& item:allItems)names+=item.name+ItemInfoGlyphs(item.info);
            {
                std::lock_guard lock(viewMutex);
                view.faceLightAvailable=FaceLight::Available();
                functionCategory=FaceLight::VisibleSection(functionCategory,view.faceLightAvailable);
                view.inventoryGlyphs=std::move(names);view.open = true; view.animateClose=false; view.functions=functions;view.functionSection=functionCategory;view.outfitDialog=0; view.settingsOpen = view.capturingKey = view.saveError = false;
                view.page = functions?(functionCategory==FaceLight::Section::Outfits?functionPage:0):favoritePage; view.x = view.y = 0;
                FilterItems();
                if (!functions && !favoriteCategoryChosen) {
                    // Pick a populated category only on the first favorites opening.
                    // Later openings retain even an empty category (e.g. last potion consumed).
                    if (view.items.empty() && !allItems.empty()) { view.category = allItems.front().category; FilterItems(); }
                    favoriteCategoryChosen=true;
                }
            }
            if(functions)RefreshFunctions();
            RE::UIMessageQueue::GetSingleton()->AddMessage(menuName, RE::UI_MESSAGE_TYPE::kShow, nullptr);
            if(Config().sounds) RE::PlaySound("UIMenuOK");
            SKSE::log::info("Wheel opened: mode={} favorites={}",functions?"functions":"favorites",allItems.size());
        }
        void RequestOpen(bool functions) {
            std::lock_guard lock(openMutex);
            if(openQueued.exchange(true))return;
            pendingOpen=PendingOpen{functions,epoch.load(),++openSerial,
                std::chrono::steady_clock::now()+std::chrono::seconds(2)};
        }
        void PumpOpen() {
            if(!openQueued)return;
            std::optional<PendingOpen> request;
            {std::lock_guard lock(openMutex);request=std::move(pendingOpen);pendingOpen.reset();}
            if(!request || request->serial!=openSerial.load())return;
            const auto reason=OpenBlockReason();
            if(request->generation==epoch.load() && !IsOpen() && !reason &&
                std::chrono::steady_clock::now()<request->deadline)Open(request->functions);
            else SKSE::log::info("Wheel opening canceled on player update: {}",reason?reason:"state changed or expired");
            if(request->serial==openSerial.load())openQueued=false;
        }
        void FunctionNavigate(FaceLight::Section section,bool preservePointer=false) {
            {std::lock_guard lock(viewMutex);
                section=FaceLight::VisibleSection(section,view.faceLightAvailable);
                if(view.functionSection==FaceLight::Section::Outfits)functionPage=view.page;
                functionCategory=section==FaceLight::Section::Outfits?section:FaceLight::Section::Lighting;
                view.functionSection=section;view.page=section==FaceLight::Section::Outfits?functionPage:0;
                if(!preservePointer)view.x=view.y=0;
            }
            RefreshFunctions();
        }
        void FunctionBack() {
            const auto section=Snapshot().functionSection;
            FunctionNavigate(section==FaceLight::Section::Followers?FaceLight::Section::Lighting:FaceLight::Section::Outfits);
        }
        void ChangeCategory(int direction) {
            const auto current=Snapshot();
            if(current.functions) {
                const auto section=FaceLight::ChangeType(current.functionSection,direction,current.faceLightAvailable);
                if(section!=current.functionSection)FunctionNavigate(section,true);
                return;
            }
            std::lock_guard lock(viewMutex);
            favoriteCategoryChosen=true;
            view.category = static_cast<Category>(Wrap(static_cast<int>(view.category) + direction, categoryCount));
            view.page = 0; // Keep the pointer on the same sector when changing type.
            FilterItems();
        }
        void ChangePage(int direction) {
            std::lock_guard lock(viewMutex);
            view.page = Wrap(view.page + direction, PageCount(view.items.size()));
            view.x = view.y = 0;
        }
        void OutfitDialog(int mode,std::uint32_t id,const std::string& name) {
            std::lock_guard lock(viewMutex);view.outfitDialog=mode;view.outfitId=id;view.outfitName.Set(name);view.x=view.y=0;
        }
        void EndOutfitDialog() {
            if(Snapshot().outfitDialog==1 || Snapshot().outfitDialog==2)TextBridge::Reset();
            {std::lock_guard lock(viewMutex);view.outfitDialog=0;view.x=view.y=0;}
            RefreshFunctions();
        }
        void Activate(bool left) {
            const auto current = Snapshot();
            const int slot = WheelSlot(current.x, current.y);
            const int index = current.page * slots + slot;
            if (slot < 0 || index < 0 || index >= static_cast<int>(current.items.size())) {
                if(current.functions)SKSE::log::info("Function click ignored: neutral/empty slot={} x={:.3f} y={:.3f}",slot,current.x,current.y);
                return;
            }
            const auto selected = current.items[index];
            if(current.functions)SKSE::log::info("Function click: action={} preset={} usable={} equipped={} right={}",static_cast<int>(selected.action),selected.actionId,selected.usable,selected.equipped,left);
            if(selected.action==ActionKind::FunctionBack){FunctionBack();return;}
            if(selected.action==ActionKind::FaceLightMenu || selected.action==ActionKind::FaceLightFollowers) {
                if(selected.usable)FunctionNavigate(selected.action==ActionKind::FaceLightMenu?FaceLight::Section::Lighting:FaceLight::Section::Followers);
                else RE::SendHUDMessage::ShowHUDMessage(selected.detail.c_str());
                return;
            }
            if(selected.action==ActionKind::SaveOutfit){OutfitDialog(1,0,Tr(Config(),"outfitDefault"));return;}
            if(selected.action==ActionKind::ImportOutfits){Outfits::Import();RefreshFunctions();return;}
            if(selected.action==ActionKind::Outfit && left){OutfitDialog(3,selected.actionId,selected.name);return;}
            if (!selected.usable) {
                RE::SendHUDMessage::ShowHUDMessage((selected.action==ActionKind::FaceLightCommand?selected.detail:Tr(Config(),selected.action==ActionKind::Outfit?"outfitMissing":"unsupported")).c_str());
                return;
            }
            {
                std::lock_guard lock(actionMutex);
                pendingAction = PendingAction{selected, left, epoch.load(), std::chrono::steady_clock::now() + std::chrono::seconds(2)};
            }
            Cancel(true);
        }
        std::optional<PendingAction> TakeAction(ActionExecutor executor) {
            std::lock_guard lock(actionMutex);
            if(!pendingAction || ExecutorFor(pendingAction->item.action==ActionKind::FaceLightCommand)!=executor)return {};
            auto ui=RE::UI::GetSingleton();
            const auto decision=DecideActionOn(executor,
                ExecutorFor(pendingAction->item.action==ActionKind::FaceLightCommand),
                pendingAction->generation,epoch.load(),
                ui && ValidPlayer() && Focused() && !BlockedMenu(),closing,
                ui && ui->IsMenuOpen(menuName),ui && ui->GameIsPaused(),
                std::chrono::steady_clock::now()>=pendingAction->deadline);
            if(decision==ActionDecision::Wait)return {};
            auto submit=decision==ActionDecision::Submit?std::move(pendingAction):std::optional<PendingAction>{};
            if(decision==ActionDecision::Discard)SKSE::log::info("Pending action canceled: state changed or close timed out");
            pendingAction.reset();return submit;
        }
        void PumpAction() {
            bool taskAction;
            {std::lock_guard lock(actionMutex);taskAction=pendingAction &&
                ExecutorFor(pendingAction->item.action==ActionKind::FaceLightCommand)==ActionExecutor::TaskQueue;}
            if ((!taskAction && !Outfits::Busy()) || actionTaskQueued.exchange(true)) return;
            // Favorites and outfit jobs retain their existing SKSE task path.
            SKSE::GetTaskInterface()->AddTask([] {
                auto submit=TakeAction(ActionExecutor::TaskQueue);
                if (submit) {
                    if(submit->item.action==ActionKind::Outfit){outfitGeneration=submit->generation;Outfits::Apply(submit->item.actionId);}
                    else UseFavorite(submit->item, submit->left);
                }
                if(!submit && Outfits::Busy()) {
                    auto ui=RE::UI::GetSingleton();
                    Outfits::Tick(outfitGeneration==epoch.load() && ValidPlayer() && Focused() && !BlockedMenu() && ui && !ui->GameIsPaused());
                }
                actionTaskQueued = false;
            });
        }
        void PlayerUpdate(RE::PlayerCharacter* player,float delta) {
            // Face Lighting establishes its accepted thread in the previous hook.
            // Do not call its API from input, Present or an SKSE task callback.
            previousPlayerUpdate(player,delta);
            if(!updateLogged.exchange(true))SKSE::log::info("Wheel player-update callback active: thread={}",GetCurrentThreadId());
            if(auto submit=TakeAction(ActionExecutor::PlayerUpdate))FaceLight::Execute(submit->item);
            PumpOpen();
        }
        void InstallPlayerUpdate() {
            if(playerUpdateInstalled || !wheelInstalled)return;
            // NewGame/PostLoadGame is after every plugin's DataLoaded callback;
            // chain outside Face Lighting regardless of DLL load order.
            REL::Relocation<std::uintptr_t> vtable{RE::VTABLE_PlayerCharacter[0]};
            previousPlayerUpdate=vtable.write_vfunc(RuntimeSupport::playerUpdateSlot,PlayerUpdate);
            playerUpdateInstalled=true;
            SKSE::log::info("Wheel player-update hook installed after DataLoaded");
        }
        std::uint32_t FavoritesScanCode() {
            auto controls = RE::ControlMap::GetSingleton();
            return controls ? controls->GetMappedKey("Favorites", RE::INPUT_DEVICE::kKeyboard, RE::UserEvents::INPUT_CONTEXT_ID::kGameplay) : 0xFFFFFFFF;
        }
        bool ToggleKey(const RE::ButtonEvent* button) {
            return button->GetDevice() == RE::INPUT_DEVICE::kKeyboard &&
                FavoritesKeyMatches(Config().hotkey, FavoritesScanCode(), button->GetIDCode(), button->GetUserEvent().c_str());
        }

        void EnterSettings() {
            BeginSettings();
            std::lock_guard lock(viewMutex);
            view.settingsOpen=true; view.capturingKey=false; view.saveError=false;
            view.x=view.y=0;
        }
        void LeaveSettings(bool save) {
            if (save && !SaveSettings()) { std::lock_guard lock(viewMutex); view.saveError=true; return; }
            if (save) {
                const auto config=Config();
                SKSE::log::info("Settings saved: language={} theme={} scale={} hotkey={}",config.language,config.theme,config.scale,config.hotkey);
            }
            if (!save) RevertSettings();
            std::lock_guard lock(viewMutex);
            view.settingsOpen=view.capturingKey=view.saveError=false;
            view.x=view.y=0;
        }
        void AdjustSetting(int row,int direction) {
            auto config=Config();
            switch(row) {
            case 0: config.wheelScale+=direction*.05f; break;
            case 1: config.sensitivity+=direction*.1f; break;
            case 2: config.showHints=!config.showHints; break;
            case 3: config.language=CycleLanguage(config.language,direction); break;
            case 4: config.theme=CycleTheme(config.theme,direction); break;
            case 5: config.hotkey=-1; break;
            case 6: config.positionX+=direction*2; break;
            case 7: config.positionY+=direction*2; break;
            case 8: config.overlayOpacity+=direction*5; break;
            case 9: config.sounds=!config.sounds; break;
            case 10: config.animations=!config.animations; break;
            case 11: config.switchKey=19; break;
            }
            EditSettings(config);
            std::lock_guard lock(viewMutex); view.saveError=false;
        }
        void SettingsClick(bool right) {
            const auto current=Snapshot();
            const float x=current.x*224, y=current.y*224;
            if (current.capturingKey) return;
            if (applyButton.Contains(x,y) && !right) { LeaveSettings(true); return; }
            if (cancelButton.Contains(x,y) && !right) { LeaveSettings(false); return; }
            if (defaultsButton.Contains(x,y) && !right) { DefaultSettings(); return; }
            for (int row=0;row<settingRows;++row) {
                if(row!=5 && row!=11 && MinusButton(row).Contains(x,y)) { AdjustSetting(row,-1); return; }
                if(row!=5 && row!=11 && PlusButton(row).Contains(x,y)) { AdjustSetting(row,1); return; }
                if(ValueButton(row).Contains(x,y)) {
                    if((row==5 || row==11) && !right) { std::lock_guard lock(viewMutex); view.capturingKey=true;view.captureSwitch=row==11; }
                    else AdjustSetting(row,right?-1:1);
                    return;
                }
            }
        }

        void OutfitAccept() {
            const auto current=Snapshot();bool ok=false;
            switch(current.outfitDialog) {
            case 1:ok=Outfits::Capture(current.outfitName.text);break;
            case 2:ok=Outfits::Rename(current.outfitId,current.outfitName.text);break;
            case 4:ok=Outfits::Capture(current.outfitName.text,current.outfitId);break;
            case 5:ok=Outfits::Remove(current.outfitId);break;
            default:EndOutfitDialog();return;
            }
            if(ok)EndOutfitDialog();
        }
        void OutfitClick() {
            const auto current=Snapshot();const float x=current.x*224,y=current.y*224;
            if(current.outfitDialog!=3 && cancelButton.Contains(x,y)){EndOutfitDialog();return;}
            if(applyButton.Contains(x,y)){OutfitAccept();return;}
            if((current.outfitDialog==1 || current.outfitDialog==2) && outfitNameButton.Contains(x,y)) {
                const auto at=HitName(current.outfitName.text,x);
                std::lock_guard lock(viewMutex);view.outfitName.Place(at,(GetAsyncKeyState(VK_SHIFT)&0x8000)!=0);return;
            }
            if(current.outfitDialog!=3)return;
            if(outfitNameButton.Contains(x,y))OutfitDialog(2,current.outfitId,current.outfitName.text);
            else if(outfitOverwriteButton.Contains(x,y))OutfitDialog(4,current.outfitId,current.outfitName.text);
            else if(outfitDeleteButton.Contains(x,y))OutfitDialog(5,current.outfitId,current.outfitName.text);
            else if(outfitExportButton.Contains(x,y))Outfits::Export(current.outfitId);
        }
        void NameKey(std::uint32_t code,bool fallback) {
            std::lock_guard lock(viewMutex);
            auto& editor=view.outfitName;
            const bool ctrl=(GetAsyncKeyState(VK_CONTROL)&0x8000)!=0;
            const bool shift=shiftHeld[0]||shiftHeld[1] || (GetAsyncKeyState(VK_SHIFT)&0x8000);
            if(code==14){editor.Backspace();return;}
            if(code==211){editor.Delete();return;}
            if(code==203 || code==205){editor.Move(code==203?-1:1,shift);return;}
            if(code==199 || code==207){editor.Place(code==199?0:editor.text.size(),shift);return;}
            if(ctrl && code==30){editor.SelectAll();return;}
            if(ctrl && (code==46 || code==45)) {
                const auto selection=editor.Selection();if(selection.empty())return;
                const int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,selection.data(),static_cast<int>(selection.size()),nullptr,0);
                if(count<=0)return;
                auto memory=GlobalAlloc(GMEM_MOVEABLE,(count+1)*sizeof(wchar_t));if(!memory)return;
                auto chars=static_cast<wchar_t*>(GlobalLock(memory));if(!chars){GlobalFree(memory);return;}
                MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,selection.data(),static_cast<int>(selection.size()),chars,count);chars[count]=0;GlobalUnlock(memory);
                bool copied=false;
                if(OpenClipboard(GetForegroundWindow())){if(EmptyClipboard())copied=SetClipboardData(CF_UNICODETEXT,memory)!=nullptr;CloseClipboard();}
                if(!copied)GlobalFree(memory);
                else if(code==45)editor.EraseSelection();
                return;
            }
            if(ctrl && code==47) {
                if(OpenClipboard(nullptr)) {
                    if(auto handle=GetClipboardData(CF_UNICODETEXT))if(auto chars=static_cast<const wchar_t*>(GlobalLock(handle))) {
                        const auto capacity=GlobalSize(handle)/sizeof(wchar_t);
                        std::size_t length=0;while(length<std::min<std::size_t>(capacity,128) && chars[length])++length;
                        editor.InsertUtf16({reinterpret_cast<const char16_t*>(chars),length});
                        GlobalUnlock(handle);
                    }
                    CloseClipboard();
                }
            } else if(fallback && !ctrl && !(GetAsyncKeyState(VK_MENU)&0x8000)) {
                BYTE keys[256]{};GetKeyboardState(keys);
                keys[VK_SHIFT]=(shiftHeld[0]||shiftHeld[1])?0x80:0;
                wchar_t chars[8]{};auto layout=GetKeyboardLayout(0);
                const auto vk=MapVirtualKeyExW(code,MAPVK_VSC_TO_VK_EX,layout);
                const int n=ToUnicodeEx(vk,code,keys,chars,8,4,layout);
                if(n>0)editor.InsertUtf16({reinterpret_cast<const char16_t*>(chars),static_cast<std::size_t>(n)});
            }
        }
        void InputHook(RE::BSTEventSource<RE::InputEvent*>* source, RE::InputEvent** events) {
            if (dispatchCount.fetch_add(1) == 0) SKSE::log::info("Input dispatch active (first callback)");
            bool captureBatch = IsOpen() || closing || openQueued;
            if (IsOpen() && (!Focused() || !ValidPlayer() || BlockedMenu())) Cancel();
            const auto bridge=TextBridge::Query();
            bool characterBatch=false;
            for(auto event=events?*events:nullptr;event;event=event->next)
                if(auto c=event->AsCharEvent();c && c->keyCode>=32)characterBatch=true;
            for (auto event = events ? *events : nullptr; event; event = event->next) {
                if (auto button = event->AsButtonEvent()) {
                    const auto code = button->GetIDCode();
                    const auto identity = (static_cast<std::uint64_t>(button->GetDevice()) << 32) | code;
                    const bool up = button->IsUp();
                    const bool down = button->IsDown();
                    const bool pressed = button->IsPressed();
                    bool nameRepeat=false;
                    if(button->GetDevice()==RE::INPUT_DEVICE::kKeyboard && code<256) {
                        if(up)nameRepeatAt[code]=0;
                        else if(down)nameRepeatAt[code]=.4f;
                        else if(pressed && nameRepeatAt[code]>0 && button->GetRuntimeData().heldDownSecs>=nameRepeatAt[code]) {
                            nameRepeat=true;nameRepeatAt[code]=button->GetRuntimeData().heldDownSecs+.05f;
                        }
                    }
                    if(button->GetDevice()==RE::INPUT_DEVICE::kKeyboard && (code==42 || code==54))shiftHeld[code==54]=pressed;
                    const bool toggle = ToggleKey(button);
                    if (down && button->GetDevice() == RE::INPUT_DEVICE::kKeyboard && (code == 16 || toggle)) {
                        const auto reason = OpenBlockReason();
                        const auto player = RE::PlayerCharacter::GetSingleton();
                        const auto race = player ? player->GetRace() : nullptr;
                        SKSE::log::info("Favorites key: scan={} mapped={} event='{}' match={} open={} blocked='{}' race={:08X} racePlayable={}",
                            code, FavoritesScanCode(), button->GetUserEvent().c_str(), toggle, IsOpen(), reason ? reason : "none",
                            race ? race->GetFormID() : 0, race && race->GetPlayable());
                    }
                    bool consume = captureBatch || inputGate.Swallowed(identity);
                    const auto current=Snapshot();
                    const bool naming=current.open && (current.outfitDialog==1 || current.outfitDialog==2);
                    if(naming && nameRepeat && (code==14 || code==211 || code==203 || code==205) && !bridge.composing)NameKey(code,false);
                    const bool bridgeKey=naming && bridge.available && bridge.enabled && button->GetDevice()==RE::INPUT_DEVICE::kKeyboard &&
                        (code==bridge.hotkey || code==29 || code==157 || code==42 || code==54 || (bridge.active && code==57 && (GetAsyncKeyState(VK_CONTROL)&0x8000)));
                    if(down && openQueued && button->GetDevice()==RE::INPUT_DEVICE::kKeyboard && (code==1 || code==15)) {
                        Cancel();
                    } else if(bridgeKey) {
                        // Keep toggle/modifier values intact for Text Bridge regardless of hook order.
                        consume=false;
                    } else if(down && current.open && current.outfitDialog && !inputGate.Swallowed(identity)) {
                        if(button->GetDevice()==RE::INPUT_DEVICE::kKeyboard) {
                            if(naming && bridge.composing) { /* IME owns candidate editing/confirmation. */ }
                            else if(code==1 || code==15)EndOutfitDialog();
                            else if(code==28)OutfitAccept();
                            else if(naming)NameKey(code,!bridge.active && !characterBatch);
                        } else if(button->GetDevice()==RE::INPUT_DEVICE::kMouse && code==0)OutfitClick();
                    } else if (down && current.open && current.settingsOpen && !inputGate.Swallowed(identity)) {
                        if(button->GetDevice()==RE::INPUT_DEVICE::kKeyboard) {
                            if(current.capturingKey) {
                                if(code!=1) { auto config=Config(); if(current.captureSwitch)config.switchKey=static_cast<int>(code);else config.hotkey=static_cast<int>(code); EditSettings(config); }
                                std::lock_guard lock(viewMutex); view.capturingKey=false;
                            } else if(code==1 || code==15) LeaveSettings(false);
                        } else if(button->GetDevice()==RE::INPUT_DEVICE::kMouse && code<2) SettingsClick(code==1);
                    } else if (down && current.open && !inputGate.Swallowed(identity) &&
                        button->GetDevice()==RE::INPUT_DEVICE::kKeyboard && code==60) {
                        EnterSettings(); // Remains reachable even if Favorites itself is bound to F2.
                    } else if(down && current.open && !inputGate.Swallowed(identity) &&
                        button->GetDevice()==RE::INPUT_DEVICE::kKeyboard && code==static_cast<unsigned>(Config().switchKey)) {
                        SwitchWheel();
                    } else if (down && !inputGate.Swallowed(identity) && toggle) {
                        if (IsOpen() || openQueued) { Cancel(true); consume = captureBatch = true; }
                        else if (closing || HasPendingAction() || Outfits::Busy()) { consume = true;if(Outfits::Busy())RE::SendHUDMessage::ShowHUDMessage(Tr(Config(),"outfitWorking").c_str()); }
                        else if (!OpenBlockReason()) { RequestOpen(shiftHeld[0]||shiftHeld[1]); consume = captureBatch = true; }
                    } else if (down && IsOpen() && !inputGate.Swallowed(identity)) {
                        if (button->GetDevice() == RE::INPUT_DEVICE::kKeyboard) {
                            switch (code) {
                            case 1: case 15: if(current.functions && current.functionSection==FaceLight::Section::Followers)FunctionBack();else Cancel(true); break; // Esc / Tab
                            case 30: case 203: ChangeCategory(-1); break; // A / left
                            case 32: case 205: ChangeCategory(1); break; // D / right
                            case 17: case 200: ChangePage(-1); break; // W / up
                            case 31: case 208: ChangePage(1); break; // S / down
                            case 28: Activate(false); break;
                            default: break;
                            }
                        } else if (button->GetDevice() == RE::INPUT_DEVICE::kMouse) {
                            if (code == 0 || code == 1) Activate(code == 1);
                            else if (code == 8) ChangePage(-1);
                            else if (code == 9) ChangePage(1);
                        }
                    }
                    const bool wheelImpulse = button->GetDevice() == RE::INPUT_DEVICE::kMouse && code >= 8;
                    const auto decision = inputGate.Filter(identity, pressed, up, consume, wheelImpulse);
                    if (decision != InputGate::Result::Pass) {
                        // Deliver one release for controls the engine already saw pressed before opening.
                        // Suppress newly pressed wheel buttons through their physical release after closing.
                        auto& data = button->GetRuntimeData();
                        data.value = 0;
                        data.heldDownSecs = decision == InputGate::Result::Release ? std::max(.001f, data.heldDownSecs) : 0;
                    }
                } else if (auto mouse = event->AsMouseMoveEvent(); mouse && captureBatch) {
                    if (IsOpen()) {
                        std::lock_guard lock(viewMutex);
                        const auto config=Config();
                        const float width=viewportWidth.load(),height=viewportHeight.load();
                        if(view.settingsOpen || view.outfitDialog) {
                            const float scale=LayoutScale(width,height,config.scale);
                            const float sensitivity=config.sensitivity/(224*scale);
                            view.x=std::clamp(view.x+mouse->mouseInputX*sensitivity,-width/(448*scale),width/(448*scale));
                            view.y=std::clamp(view.y+mouse->mouseInputY*sensitivity,-height/(448*scale),height/(448*scale));
                        } else {
                            MovePointer(view.x,view.y,mouse->mouseInputX*config.sensitivity/250.f,
                                mouse->mouseInputY*config.sensitivity/250.f);
                        }
                    }
                    mouse->mouseInputX = mouse->mouseInputY = 0;
                } else if(auto character=event->AsCharEvent();character && captureBatch) {
                    const auto current=Snapshot();
                    if(current.outfitDialog!=1 && current.outfitDialog!=2)character->keyCode=0;
                } else if (auto stick = event->AsThumbstickEvent(); stick && captureBatch) {
                    stick->xValue = stick->yValue = 0;
                }
            }
            previousDispatch(source, events);
            if (!Focused()) {inputGate.Reset();shiftHeld[0]=shiftHeld[1]=false;}
            PumpAction();
        }

        class PauseMenu final : public RE::IMenu {
        public:
            PauseMenu() {
                depthPriority = 3;
                menuFlags.set(Flag::kPausesGame, Flag::kDisablePauseMenu, Flag::kUsesMenuContext);
                inputContext = Context::kMenuMode;
            }
            static RE::IMenu* Create() { return new PauseMenu; }
            RE::UI_MESSAGE_RESULTS ProcessMessage(RE::UIMessage& message) override {
                if (message.type == RE::UI_MESSAGE_TYPE::kHide || message.type == RE::UI_MESSAGE_TYPE::kForceHide) {
                    RevertSettings();
                    std::lock_guard lock(viewMutex);
                    if(view.open) view.animateClose=false;
                    view.open = false;view.outfitDialog=0; view.settingsOpen=view.capturingKey=false; closing = false;
                    return RE::UI_MESSAGE_RESULTS::kHandled;
                }
                if (message.type == RE::UI_MESSAGE_TYPE::kShow) return RE::UI_MESSAGE_RESULTS::kHandled;
                return RE::UI_MESSAGE_RESULTS::kPassOn;
            }
            void AdvanceMovie(float, std::uint32_t) override {}
            void PostDisplay() override {}
        };

        // Receives the final event chain, including characters injected by inner hooks.
        class InputObserver final : public RE::BSTEventSink<RE::InputEvent*> {
            RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* events, RE::BSTEventSource<RE::InputEvent*>*) override {
                static bool warned = false;
                if (dispatchCount == 0 && !warned && events && *events) {
                    warned = true;
                    SKSE::log::warn("Input events reach the engine but FavoriteWheel dispatch hook is bypassed by another hook");
                }
                std::lock_guard lock(viewMutex);
                if(view.open)for(auto event=events?*events:nullptr;event;event=event->next)if(auto c=event->AsCharEvent()) {
                    if(view.outfitDialog==1 || view.outfitDialog==2)view.outfitName.Insert(c->keyCode);
                    c->keyCode=0;
                }
                return RE::BSEventNotifyControl::kContinue;
            }
        };
    }

    View Snapshot() { std::lock_guard lock(viewMutex); auto copy=view; copy.config=Config(); return copy; }
    void SetViewport(float width,float height) { if(width>0 && height>0) {viewportWidth=width; viewportHeight=height;} }
    bool IsOpen() { std::lock_guard lock(viewMutex); return view.open; }
    void Cancel(bool effects) {
        {std::lock_guard lock(openMutex);++openSerial;openQueued=false;pendingOpen.reset();}
        const auto current=Snapshot();
        if(current.outfitDialog==1 || current.outfitDialog==2)TextBridge::Reset();
        RevertSettings();
        bool wasOpen;
        { std::lock_guard lock(viewMutex); wasOpen = view.open;if(view.functions){if(view.functionSection==FaceLight::Section::Outfits)functionPage=view.page;}else favoritePage=view.page; view.open = false;view.outfitDialog=0; view.animateClose=effects; view.settingsOpen=view.capturingKey=false; }
        if (wasOpen) {
            if(effects && Config().sounds) RE::PlaySound("UIMenuCancel");
            closing = true;
            if (auto queue = RE::UIMessageQueue::GetSingleton()) queue->AddMessage(menuName, RE::UI_MESSAGE_TYPE::kHide, nullptr);
        }
    }
    void SetGameActive(bool active) {
        ++epoch;
        Outfits::CancelJob();
        FaceLight::Clear();
        { std::lock_guard lock(actionMutex); pendingAction.reset(); }
        if(active)InstallPlayerUpdate();
        gameActive = active;
        Cancel();
        favoritePage=functionPage=0;shiftHeld[0]=shiftHeld[1]=false;
        allItems.clear();
        { std::lock_guard lock(viewMutex); view.items.clear(); }
        SKSE::log::info("Game active={}", active);
        if (active) SKSE::log::info("Input state after load: callbacks={} favoritesScan={} override={}", dispatchCount.load(), FavoritesScanCode(), Config().hotkey);
    }
    bool InstallWheel() {
        auto ui = RE::UI::GetSingleton();
        if (!ui) return false;
        // Same CommonLib input-dispatch call site used by the user's IME project.
        const auto address = REL::RelocationID(RuntimeSupport::inputDispatchSE, RuntimeSupport::inputDispatchAE).address() + RuntimeSupport::inputDispatchCall;
        if (*reinterpret_cast<const std::uint8_t*>(address) != 0xE8) {
            SKSE::log::error("Input dispatch call signature is unsupported; vanilla favorites retained");
            return false;
        }
        ui->Register(menuName, PauseMenu::Create);
        SKSE::AllocTrampoline(32);
        previousDispatch = SKSE::GetTrampoline().write_call<5>(address, InputHook);
        if (auto input = RE::BSInputDeviceManager::GetSingleton()) {
            static InputObserver observer;
            input->AddEventSink(&observer);
        }
        wheelInstalled=true;
        SKSE::log::info("Input dispatch hook installed: runtime={} id={} callOffset={:X}",
            REL::Module::get().version().string(),REL::Module::IsAE()?RuntimeSupport::inputDispatchAE:RuntimeSupport::inputDispatchSE,
            RuntimeSupport::inputDispatchCall);
        return true;
    }
}
