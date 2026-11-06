#include "Wheel.h"
#include "RuntimeSupport.h"
#include "Outfits.h"
#include "CombatPresets.h"
#include "CombatCycles.h"
#include "CombatCycleModel.h"
#include "CombatPresetModel.h"
#include "Settings.h"
#include "InputGate.h"
#include "InputDispatchChain.h"
#include "OpenPolicy.h"
#include "InputBindings.h"
#include "QuickSlots.h"
#include "InventoryPages.h"
#include "InventoryOrder.h"
#include "TimeControl.h"
#include "TimePolicy.h"
#include "QueuedTask.h"
#include "GameSession.h"
#include "ActorRuntime.h"
#include "FormRuntime.h"
#include "ControllerSticks.h"
#include "ActionPolicy.h"
#include "AlchemyCategories.h"
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
        // Native menu callbacks, input and player-update callbacks can overlap.
        // Serialize opening/closing with time synchronization. Recursive entry is
        // required because a time conflict cancels from inside SyncWheelTime.
        std::recursive_mutex lifecycleMutex;
        View view;
        std::atomic<FormMode> openedForm{FormMode::Human};
        InventoryPages inventoryPages;
        std::string inventoryFontGlyphs,functionFontGlyphs; // viewMutex held when publishing.
        std::vector<Item> functionItems;
        bool inventoryLoaded=false,inventoryScope=false;
        std::atomic<std::uint64_t> directoryRevision{0};
        std::atomic<bool> directoryTaskQueued{false},detailTaskQueued{false};
        std::atomic<std::uint64_t> inventoryChanges{0};
        std::uint64_t inventoryStamp=0; // viewMutex held.
        std::chrono::steady_clock::time_point nextInventoryRefresh{};
        std::optional<ItemKey> hoverKey;
        std::chrono::steady_clock::time_point hoverStarted;
        std::uint64_t hoverSerial=0; // Input pump only; a reopened wheel starts a new dwell.
        int favoritePage=0,functionPage=0,combatPage=0,cyclePage=0,cycleMemberPage=0;
        int FunctionPage(FaceLight::Section section){return section==FaceLight::Section::Outfits?functionPage:section==FaceLight::Section::Combat?combatPage:section==FaceLight::Section::Cycles?cyclePage:section==FaceLight::Section::CycleMembers?cycleMemberPage:0;}
        WheelOpeningMemory openingMemory; // viewMutex held for all accesses.
        void RememberFunctionPage(){if(view.functionSection==FaceLight::Section::Outfits)functionPage=view.page;else if(view.functionSection==FaceLight::Section::Combat)combatPage=view.page;else if(view.functionSection==FaceLight::Section::Cycles)cyclePage=view.page;else if(view.functionSection==FaceLight::Section::CycleMembers)cycleMemberPage=view.page;}
        FaceLight::Section functionCategory=FaceLight::Section::Outfits; // Remember the top-level type, not a child list.
        bool shiftHeld[2]{};
        bool keyboardHeld[256]{};
        ControllerChordState padChords;
        float padX=0, padY=0;
        float nameRepeatAt[256]{};
        GameSession gameSession;
        std::atomic<bool> sessionProbeQueued{false};
        std::atomic<std::uint64_t> epoch{0};
        std::atomic<bool> closing{false};
        std::atomic<bool> openQueued{false};
        std::atomic<std::uint64_t> openSerial{0};
        struct PendingOpen {
            bool functions;
            RE::INPUT_DEVICE device;
            std::uint32_t code;
            std::uint64_t generation, serial;
            std::chrono::steady_clock::time_point deadline;
        };
        std::mutex openMutex;
        std::optional<PendingOpen> pendingOpen;
        REL::Relocation<void (*)(RE::PlayerCharacter*, float)> previousPlayerUpdate;
        std::atomic<bool> wheelInstalled{false};
        bool playerUpdateInstalled=false;
        std::atomic<bool> updateLogged{false};
        std::atomic<float> viewportWidth{1280}, viewportHeight{900};
        InputGate inputGate;
        ControllerPageButtons controllerPages;
        ControllerPageButtons controllerTriggers{280};
        MovementStickGate movementStick;
        LookStickGate lookStick;
        std::mutex pauseMutex;
        bool pauseRequested=false;
        using Dispatch = void(RE::BSTEventSource<RE::InputEvent*>*, RE::InputEvent**);
        REL::Relocation<Dispatch> previousDispatch;
        std::atomic<std::uint64_t> dispatchCount{0};
        struct PendingAction {
            Item item;
            bool left;
            std::uint64_t generation;
            std::chrono::steady_clock::time_point deadline;
            bool retained=false;
            std::uint64_t serial=0;
            bool keepPaused=false;
            FormMode formMode=CurrentFormMode();
        };
        std::mutex actionMutex;
        std::optional<PendingAction> pendingAction;
        std::atomic<bool> actionTaskQueued{false};
        std::uint64_t outfitGeneration=0; // SKSE task thread only.
        std::atomic<bool> retainedAction{false};
        std::atomic<bool> retainedKeepsPause{false};
        std::atomic<std::uint64_t> playerUpdates{0};
        bool retainedSubmitted=false; // lifecycleMutex held.
        std::chrono::steady_clock::time_point retainedAt;
        std::uint64_t retainedUpdate=0;

        bool HasPendingAction() { std::lock_guard lock(actionMutex); return pendingAction.has_value(); }

        bool Focused() {
            DWORD process = 0;
            GetWindowThreadProcessId(GetForegroundWindow(), &process);
            return process == GetCurrentProcessId();
        }
        const char* BlockingMenu() {
            auto ui = RE::UI::GetSingleton();
            if (!ui) return "UI unavailable";
            return BlockingWheelMenu([&](const char* name){return ui->IsMenuOpen(name);});
        }
        bool BlockedMenu() { return BlockingMenu() != nullptr; }
        bool ValidPlayer() {
            auto player = RE::PlayerCharacter::GetSingleton();
            auto menus = RE::MenuControls::GetSingleton();
            const auto mode=CurrentFormMode();
            return menus && (!IsOpen() || mode==openedForm.load()) &&
                PlayerEligible(gameSession.Active(), player != nullptr, player && player->Get3D(),
                    ActorRuntime::IsDead(player), mode==FormMode::UnsupportedBeast);
        }
        bool EntryControlsEnabled(RE::INPUT_DEVICE device,std::uint32_t code) {
            auto controls=RE::ControlMap::GetSingleton();
            if(!controls)return false;
            if(device!=RE::INPUT_DEVICE::kKeyboard && device!=RE::INPUT_DEVICE::kMouse && device!=RE::INPUT_DEVICE::kGamepad)return false;
            const auto context=controls->controlMap[RE::UserEvents::INPUT_CONTEXT_ID::kGameplay];
            if(!context)return true; // A custom wheel key need not have a native mapping.
            const auto enabled=controls->GetRuntimeData().enabledControls.underlying();
            for(const auto& mapping:context->deviceMappings[static_cast<unsigned>(device)])
                if(mapping.inputKey==code && !EntryControlGroupEnabled(mapping.userEventGroupFlag.underlying(),enabled))return false;
            return true;
        }
        const char* OpenBlockReason() {
            auto ui = RE::UI::GetSingleton();
            auto controls = RE::ControlMap::GetSingleton();
            if (!Config().enabled) return "disabled";
            if(auto camera=RE::PlayerCamera::GetSingleton();camera && camera->IsInFreeCameraMode())return "free camera";
            if (!RendererReady()) return "renderer not ready";
            if (closing) return "wheel still closing";
            if(Outfits::Busy() || Combat::Busy())return "preset changing";
            if (HasPendingAction()) return "item action pending";
            if (!gameSession.Active()) return "game not active";
            auto player = RE::PlayerCharacter::GetSingleton();
            if (!player) return "player unavailable";
            if (!player->Get3D()) return "player 3D unavailable";
            if (ActorRuntime::IsDead(player)) return "player dead";
            auto menus = RE::MenuControls::GetSingleton();
            if (!menus) return "menu controls unavailable";
            if (CurrentFormMode()==FormMode::UnsupportedBeast) return "unsupported beast form: yielding to vanilla";
            if (CurrentFormMode()==FormMode::VampireLord && !VampireSpellList()) return "vampire spell list unavailable: yielding to vanilla";
            if (!Focused()) return "game not foreground";
            if (!ui) return "UI unavailable";
            if (const auto menu = BlockingMenu()) return menu;
            if (ui->GameIsPaused()) return "another menu pauses game";
            if (!controls) return "control map unavailable";
            if (!controls->IsMenuControlsEnabled()) return "menu controls disabled";
            if (controls->GetRuntimeData().textEntryCount > 0) return "text input active";
            return nullptr;
        }
        void RequestPause(bool pause) {
            std::lock_guard lock(pauseMutex);
            if(pauseRequested==pause)return;
            if(auto queue=RE::UIMessageQueue::GetSingleton()) {
                pauseRequested=pause;
                queue->AddMessage(pauseMenuName,pause?RE::UI_MESSAGE_TYPE::kShow:RE::UI_MESSAGE_TYPE::kHide,nullptr);
            }
        }
        bool textInputClaimed=false; // lifecycleMutex; balance only our own claim.
        void ReleaseTextInput() {
            if(textInputClaimed) {
                textInputClaimed=false;
                if(auto controls=RE::ControlMap::GetSingleton())controls->AllowTextInput(false);
            }
            SimpleIME::End();
        }
        void SyncTextInput() {
            const auto current=Snapshot();
            const bool naming=current.open && (current.outfitDialog==1 || current.outfitDialog==2);
            if(!naming){ReleaseTextInput();return;}
            // Let the click/Enter that opened either naming dialog finish before IME focus changes.
            if((GetAsyncKeyState(VK_LBUTTON)|GetAsyncKeyState(VK_RBUTTON)|GetAsyncKeyState(VK_MBUTTON)|GetAsyncKeyState(VK_RETURN))&0x8000)return;
            if(!textInputClaimed && SimpleIME::Available())if(auto controls=RE::ControlMap::GetSingleton()) {
                SimpleIME::Begin();textInputClaimed=true;controls->AllowTextInput(true);
            }
        }
        void SyncWheelTime() {
            std::lock_guard lifecycle(lifecycleMutex);
            SyncTextInput();
            bool open,settings;int dialog;
            {std::lock_guard lock(viewMutex);open=view.open;settings=view.settingsOpen;dialog=view.outfitDialog;}
            if(!open){TimeControl::EndSession();RequestPause(false);return;}
            if(!RendererReady() || !Focused() || !ValidPlayer() || BlockedMenu()){Cancel();return;}
            const auto config=Config();
            const bool pause=PauseForWheel(config.timeMode,settings,dialog) && (!retainedAction || retainedKeepsPause);
            if(!TimeControl::Update(!pause && config.timeMode==1,config.slowPercent)) {
                SKSE::log::warn("Wheel canceled: time state could not be safely maintained");Cancel();return;
            }
            RequestPause(pause);
        }
        void FailedTask(std::uint64_t generation,std::uint64_t serial,const char* task,const char* error,bool outfit=false) {
            SKSE::log::error("Wheel {} task failed: {}",task,error);
            std::lock_guard lifecycle(lifecycleMutex);
            if(generation!=epoch.load())return;
            if(outfit){Outfits::CancelJob();Combat::CancelJob();}
            // A failure belonging to an earlier wheel cannot close a newer one.
            if(serial==openSerial.load()) {
                if(outfit){std::lock_guard lock(actionMutex);pendingAction.reset();}
                Cancel();
            }
        }
        void PublishPage(ItemPage page) { // viewMutex held
            view.items=std::move(page.items);view.itemOffset=page.offset;view.totalItems=page.total;
        }
        std::vector<Item> unsortedItems;
        std::optional<InventorySelectionKey> sortingSelection;
        void CancelSortDrag(){view.sortDrag.Cancel();view.dragItem.reset();} // viewMutex held
        void FilterItems() { // viewMutex held.
            view.sortingSelectedSlot=-1;
            if(!view.sortingEdit){sortingSelection.reset();CancelSortDrag();}
            if(view.functions)PublishPage(SlicePage(functionItems,0,functionItems.size(),view.page));
            else {
                view.visibleCategories=CategoryVisibility(inventoryPages.visible,Config().hideEmptyCategories);
                const auto category=view.visibleCategories.Select(view.category);
                if(category!=view.category){view.category=category;view.page=favoritePage=0;}
                PublishPage(inventoryPages.Page(view.category,view.page));
                static constexpr const char* labels[]={"sortDefault","sortAsc","sortDesc","sortEquipped","sortCustom","sortType","sortSlot","sortStatDesc","sortStatAsc","sortWeight","sortValue"};
                const auto mode=Ordering::Current(view.inventoryWide,view.category);
                const char* label=mode==Ordering::Mode::Type?
                    (view.category==Category::Weapons?"sortWeaponType":view.category==Category::Armor?"sortArmorType":"sortSchool"):
                    labels[static_cast<int>(mode)];
                if(mode==Ordering::Mode::StatDescending || mode==Ordering::Mode::StatAscending) {
                    const bool ascending=mode==Ordering::Mode::StatAscending;
                    label=view.category==Category::Weapons?(ascending?"sortDamageAsc":"sortDamageDesc"):
                        view.category==Category::Armor?(ascending?"sortArmorAsc":"sortArmorDesc"):(ascending?"sortCostAsc":"sortCostDesc");
                }
                view.sortingLabel=Tr(Config(),label);
                if(view.sortingEdit && sortingSelection) {
                    bool exists=false;
                    const auto begin=inventoryPages.boundaries[static_cast<int>(view.category)];
                    const auto end=inventoryPages.boundaries[static_cast<int>(view.category)+1];
                    for(auto i=begin;i<end;++i)if(SelectionKey(inventoryPages.items[i])==*sortingSelection) {
                        exists=true;const int slot=static_cast<int>(i-begin)-view.page*slots;
                        if(slot>=0 && slot<slots)view.sortingSelectedSlot=slot;
                        break;
                    }
                    if(!exists)sortingSelection.reset();
                }
            }
        }
        void PublishDirectory(std::vector<Item> items,std::string fontGlyphs,bool scope,bool preserveSelection=false) { // viewMutex held
            CancelSortDrag(); // A refreshed catalog invalidates a pending drop.
            const int oldSlot=WheelSlot(view.x,view.y,Style(Config()).layout),oldIndex=PageItemIndex(view,oldSlot);
            const auto oldKey=oldSlot>=0 && oldIndex>=0 && oldIndex<int(view.items.size())?
                std::optional<InventorySelectionKey>{SelectionKey(view.items[oldIndex])}:std::optional<InventorySelectionKey>{};
            ++directoryRevision; // Invalidates details for the replaced catalog.
            unsortedItems=items;
            if(!view.vampireRestricted)Ordering::Apply(items,scope);
            inventoryPages.Set(std::move(items));inventoryLoaded=true;inventoryScope=scope;view.inventoryLoading=false;
            inventoryFontGlyphs=std::move(fontGlyphs);
            view.inventoryGlyphs=inventoryFontGlyphs+functionFontGlyphs;
            view.inventoryWide=scope;
            if(!view.functions) {
                FilterItems();
                const int newIndex=PageItemIndex(view,oldSlot);
                const auto newKey=oldSlot>=0 && newIndex>=0 && newIndex<int(view.items.size())?
                    std::optional<InventorySelectionKey>{SelectionKey(view.items[newIndex])}:std::optional<InventorySelectionKey>{};
                if(preserveSelection && oldSlot>=0 && oldKey!=newKey)
                    view.x=view.y=0; // Reset for a different row, not a plain stack's native anchor migration.
            }
        }
        void ResortInventory() { // viewMutex held; selection is repositioned by the caller.
            auto items=unsortedItems;Ordering::Apply(items,view.inventoryWide);
            inventoryPages.Set(std::move(items));++directoryRevision;FilterItems();
        }
        void RequestDirectory(bool refresh);
        void ChangeSorting(bool edit) {
            std::lock_guard lifecycle(lifecycleMutex);
            std::unique_lock lock(viewMutex);
            if(!view.open || view.functions || view.vampireRestricted || view.settingsOpen || view.outfitDialog ||
                view.inventoryLoading || !inventoryLoaded || retainedAction)return;
            CancelSortDrag();
            if(edit) {
                view.sortingEdit=!view.sortingEdit;
                if(view.sortingEdit)Ordering::SetCustom(view.inventoryWide,view.category,inventoryPages.items);
            }else {view.sortingEdit=false;CancelSortDrag();Ordering::Cycle(view.inventoryWide,view.category);}
            sortingSelection.reset();
            if(!edit)view.page=0;
            ResortInventory();
            if(!edit && Ordering::Numeric(Ordering::Current(view.inventoryWide,view.category))) {
                view.inventoryLoading=true;
                lock.unlock();RequestDirectory(true); // Engine stats belong on the game-thread directory task.
            }
        }
        void SelectSortedItem() {
            std::lock_guard lock(viewMutex);
            if(!view.sortingEdit || view.functions || view.vampireRestricted || view.inventoryLoading)return;
            const int slot=WheelSlot(view.x,view.y,Style(Config()).layout),local=PageItemIndex(view,slot);
            if(slot>=0 && local>=0 && local<int(view.items.size())) {
                sortingSelection=SelectionKey(view.items[local]);FilterItems();
            }
        }
        void MoveSortedItem(int direction,bool followPointer=false) {
            std::lock_guard lock(viewMutex);
            if(!view.sortingEdit || view.functions || view.vampireRestricted || !inventoryLoaded || view.inventoryLoading)return;
            if(!sortingSelection)return;
            const auto begin=inventoryPages.boundaries[static_cast<int>(view.category)];
            const auto end=inventoryPages.boundaries[static_cast<int>(view.category)+1];
            const auto selectedIt=std::find_if(inventoryPages.items.begin()+begin,inventoryPages.items.begin()+end,
                [&](const Item& item){return SelectionKey(item)==*sortingSelection;});
            if(selectedIt==inventoryPages.items.begin()+end){sortingSelection.reset();FilterItems();return;}
            const auto at=static_cast<std::size_t>(selectedIt-inventoryPages.items.begin());
            if((direction<0 && at==begin) || (direction>0 && at+1>=end))return;
            const auto selected=inventoryPages.items[at];
            auto neighbor=static_cast<std::ptrdiff_t>(at)+direction;
            bool moved=false;
            while(neighbor>=static_cast<std::ptrdiff_t>(begin) && neighbor<static_cast<std::ptrdiff_t>(end)) {
                if(Ordering::MoveItem(view.inventoryWide,view.category,selected,inventoryPages.items[neighbor])){moved=true;break;}
                neighbor+=direction;
            }
            if(!moved)return;
            ResortInventory();
            const auto found=std::find_if(inventoryPages.items.begin()+begin,inventoryPages.items.begin()+end,
                [&](const Item& item){return SelectionKey(item)==SelectionKey(selected);});
            if(found==inventoryPages.items.begin()+end)return;
            const auto index=static_cast<std::size_t>(found-inventoryPages.items.begin())-begin;
            view.page=static_cast<int>(index/slots);FilterItems();
            const auto point=ShapePoint(Style(Config()).layout,.75f,float(index%slots)*2*std::numbers::pi_v<float>/slots);
            if(followPointer){view.x=point.x;view.y=point.y;padX=view.x;padY=view.y;}
        }
        void BeginSortDrag() {
            std::lock_guard lock(viewMutex);
            CancelSortDrag();
            if(!view.sortingEdit || view.inventoryLoading || view.functions || view.vampireRestricted || retainedAction)return;
            const int slot=WheelSlot(view.x,view.y,Style(Config()).layout),local=PageItemIndex(view,slot);
            if(slot<0 || local<0 || local>=int(view.items.size()))return;
            sortingSelection=SelectionKey(view.items[local]);view.dragItem=view.items[local];
            view.sortDrag.Begin(view.x,view.y);FilterItems();
        }
        void EndSortDrag() {
            std::lock_guard lifecycle(lifecycleMutex);
            // ValidPlayer calls IsOpen(), which acquires viewMutex. Validate
            // before taking the view lock; lifecycleMutex still guards the session.
            const bool allowed=Focused() && ValidPlayer() && CurrentFormMode()==FormMode::Human;
            bool click=false;
            {
                std::lock_guard lock(viewMutex);
                if(!view.sortDrag.held)return;
                const bool active=view.sortDrag.active;
                const auto selected=view.dragItem;
                CancelSortDrag();
                if(!view.open || !view.sortingEdit || view.functions || view.vampireRestricted || view.settingsOpen ||
                    view.outfitDialog || view.inventoryLoading || !selected || !sortingSelection ||
                    !allowed)return;
                const int slot=WheelSlot(view.x,view.y,Style(Config()).layout),local=PageItemIndex(view,slot);
                if(slot<0 || local<0 || local>=int(view.items.size()))return;
                if(!active)click=SelectionKey(view.items[local])==*sortingSelection;
                else {
                    const auto begin=inventoryPages.boundaries[static_cast<int>(view.category)];
                    const auto end=inventoryPages.boundaries[static_cast<int>(view.category)+1];
                    const auto source=std::find_if(inventoryPages.items.begin()+begin,inventoryPages.items.begin()+end,
                        [&](const Item& item){return SelectionKey(item)==*sortingSelection;});
                    if(source==inventoryPages.items.begin()+end)return;
                    const auto target=view.items[local];
                    if(Ordering::InsertItem(view.inventoryWide,view.category,*source,target))ResortInventory();
                }
            }
            if(click)MoveSortedItem(-1,true);
        }
        void RequestDirectory(bool refresh=false) {
            // Clear a previous wheel/page even when an older request is still queued.
            // The input pump retries after that stale task releases the queue flag.
            if(!refresh){std::lock_guard lock(viewMutex);view.inventoryLoading=true;if(!view.functions){view.visibleCategories={0};PublishPage({});}}
            const bool scope=Config().allInventory;
            if(directoryTaskQueued.exchange(true))return;
            const auto generation=epoch.load(),serial=openSerial.load(),revision=directoryRevision.load();
            const auto stamp=inventoryChanges.load();
            auto failed=[generation,serial](const char* error){FailedTask(generation,serial,"directory",error);};
            ScheduleQueuedTask(directoryTaskQueued,[&] {
                if(auto tasks=SKSE::GetTaskInterface())tasks->AddTask([generation,serial,revision,scope,stamp,refresh,failed] {
                    RunQueuedTask(directoryTaskQueued,[&] {
                        if(generation==epoch.load() && serial==openSerial.load() && revision==directoryRevision.load() &&
                            IsOpen() && Focused() && ValidPlayer() && !BlockedMenu()) {
                            auto items=CollectInventory(scope);
                            auto fontGlyphs=CatalogFontGlyphs(items);
                            std::lock_guard lock(viewMutex);
                            if(generation==epoch.load() && serial==openSerial.load() && revision==directoryRevision.load() && view.open) {
                                PublishDirectory(std::move(items),std::move(fontGlyphs),scope,refresh);inventoryStamp=stamp;
                            }
                        }
                    },failed);
                });else directoryTaskQueued=false;
            },failed);
        }
        void PumpLiveInventory() {
            const auto now=std::chrono::steady_clock::now();
            bool refresh=false;
            {std::lock_guard lock(viewMutex);
                if(view.open && !view.functions && !view.settingsOpen && !view.outfitDialog && inventoryLoaded &&
                    (Config().timeMode!=0 || Config().keepOpen) && !retainedAction &&
                    inventoryStamp!=inventoryChanges.load() && now>=nextInventoryRefresh && !directoryTaskQueued) {
                    nextInventoryRefresh=now+std::chrono::milliseconds(250);refresh=true;
                }
            }
            if(refresh)RequestDirectory(true);
        }
        void CompleteRetainedAction(const PendingAction& action) {
            std::lock_guard lifecycle(lifecycleMutex);
            if(action.retained && retainedAction && action.serial==openSerial.load() && IsOpen()) {
                if(action.keepPaused) {
                    // EquipObject applies immediately; a paused player cannot
                    // supply the update frames used by the live/FaceLight path.
                    retainedAction=false;retainedKeepsPause=false;retainedSubmitted=false;
                    bool loaded;{std::lock_guard lock(viewMutex);loaded=inventoryLoaded;}
                    if(loaded)RequestDirectory(true);
                }else {
                    retainedSubmitted=true;retainedAt=std::chrono::steady_clock::now();retainedUpdate=playerUpdates.load();
                }
            }
        }
        void RefreshFunctions() {
            const auto mode=CurrentFormMode();if(mode!=FormMode::Human && mode!=FormMode::Werewolf)return;
            const auto current=Snapshot();const auto section=current.functionSection;
            if(mode==FormMode::Werewolf && section!=FaceLight::Section::Combat && section!=FaceLight::Section::Cycles && section!=FaceLight::Section::CycleMembers)return;
            auto entries=section==FaceLight::Section::Outfits?Outfits::Entries():section==FaceLight::Section::Combat?Combat::Entries():
                section==FaceLight::Section::Cycles || section==FaceLight::Section::CycleMembers?Cycles::Entries(section==FaceLight::Section::CycleMembers?current.cycleGroup:0):FaceLight::Entries(section);
            if(mode==FormMode::Werewolf)std::erase_if(entries,[](const Item& item){return item.action!=ActionKind::Combat && item.action!=ActionKind::SaveCombat && item.action!=ActionKind::Cycle && item.action!=ActionKind::CycleMember && item.action!=ActionKind::CycleCapture && item.action!=ActionKind::FunctionBack;});
            Cycles::AnnotateMembership(entries);
            auto fontGlyphs=CatalogFontGlyphs(entries,Outfits::Names()+Combat::Names()+Cycles::Names()+FaceLight::Names());
            std::lock_guard lock(viewMutex);
            functionFontGlyphs=std::move(fontGlyphs);view.inventoryGlyphs=inventoryFontGlyphs+functionFontGlyphs;
            functionItems=std::move(entries);FilterItems();
        }
        void SwitchWheel() {
            const auto mode=CurrentFormMode();if(mode!=FormMode::Human && mode!=FormMode::Werewolf)return;
            bool functions,needsDirectory=false;
            {std::lock_guard lock(viewMutex);
                if(view.functions)RememberFunctionPage();else favoritePage=view.page;
                functionCategory=FaceLight::VisibleSection(functionCategory,view.faceLightAvailable);
                if(mode==FormMode::Werewolf && functionCategory!=FaceLight::Section::Combat && functionCategory!=FaceLight::Section::Cycles)functionCategory=FaceLight::Section::Combat;
                view.sortingEdit=false;CancelSortDrag();functions=view.functions=!view.functions;openingMemory.Observe(functions,true);view.functionSection=functionCategory;
                view.page=functions?FunctionPage(functionCategory):favoritePage;view.x=view.y=0;
                if(!functions) {
                    needsDirectory=!inventoryLoaded || inventoryScope!=Config().allInventory;
                    if(!needsDirectory)FilterItems();
                }
            }
            if(functions)RefreshFunctions();
            else if(needsDirectory)RequestDirectory();
        }
        void Open(bool functions) {
            std::lock_guard lifecycle(lifecycleMutex);
            openedForm=CurrentFormMode();
            {std::lock_guard lock(viewMutex);
                functions=openingMemory.Resolve(functions,Config().rememberLastWheel,openedForm==FormMode::Human);
                if(openedForm==FormMode::Werewolf)functions=true;
            }
            if(openedForm==FormMode::Human)FaceLight::Capture(); // Before pause.
            const bool scope=Config().allInventory;
            const auto stamp=inventoryChanges.load();
            auto items=functions?std::vector<Item>{}:CollectInventory(scope);
            auto fontGlyphs=CatalogFontGlyphs(items);
            {
                std::lock_guard lock(viewMutex);
                view.faceLightAvailable=FaceLight::Available();
                view.vampireRestricted=openedForm==FormMode::VampireLord;
                functionCategory=FaceLight::VisibleSection(functionCategory,view.faceLightAvailable);
                if(openedForm==FormMode::Werewolf && functionCategory!=FaceLight::Section::Combat && functionCategory!=FaceLight::Section::Cycles)functionCategory=FaceLight::Section::Combat;
                ++directoryRevision;inventoryLoaded=false;inventoryPages.Set({});functionItems.clear();
                view.visibleCategories={0};
                inventoryStamp=stamp;nextInventoryRefresh=std::chrono::steady_clock::now();
                PublishPage({});
                inventoryFontGlyphs.clear();functionFontGlyphs.clear();
                view.inventoryGlyphs.clear();view.inventoryWide=scope;view.inventoryLoading=false;
                openingMemory.Observe(functions,openedForm==FormMode::Human);
                view.open = true;  view.sortingEdit=false;CancelSortDrag(); view.animateClose=false; view.functions=functions;view.functionSection=functionCategory;view.outfitDialog=0;view.cycleDialog=view.cycleMemberDialog=view.cycleCaptureKey=false; view.settingsOpen = view.capturingKey = view.saveError = false;
                view.page = functions?FunctionPage(functionCategory):favoritePage; view.x = view.y = 0;
                padX=padY=0;
                if(!functions)PublishDirectory(std::move(items),std::move(fontGlyphs),scope);
            }
            if(functions)RefreshFunctions();
            TimeControl::BeginSession();SyncWheelTime();
            if(!IsOpen())return;
            RE::UIMessageQueue::GetSingleton()->AddMessage(menuName, RE::UI_MESSAGE_TYPE::kShow, nullptr);
            if(Config().sounds) RE::PlaySound("UIMenuOK");
            SKSE::log::info("Wheel opened: mode={} scope={} entries={} timeMode={} slowPercent={} padMove={} formMode={}",functions?"functions":"inventory",scope?"all":"favorites",inventoryPages.items.size(),Config().timeMode,Config().slowPercent,Config().gamepadMoveWhileOpen,static_cast<int>(openedForm.load()));
        }
        void RequestOpen(bool functions,RE::INPUT_DEVICE device,std::uint32_t code) {
            std::lock_guard lifecycle(lifecycleMutex);
            std::lock_guard lock(openMutex);
            if(openQueued.exchange(true))return;
            pendingOpen=PendingOpen{functions,device,code,epoch.load(),++openSerial,
                std::chrono::steady_clock::now()+std::chrono::seconds(2)};
        }
        void PumpOpen() {
            std::lock_guard lifecycle(lifecycleMutex);
            if(!openQueued)return;
            std::optional<PendingOpen> request;
            {std::lock_guard lock(openMutex);request=std::move(pendingOpen);pendingOpen.reset();}
            if(!request || request->serial!=openSerial.load())return;
            RunQueuedTask(openQueued,[&] {
                const auto reason=!EntryControlsEnabled(request->device,request->code)?"entry control group disabled":OpenBlockReason();
                if(request->generation==epoch.load() && !IsOpen() && !reason &&
                    std::chrono::steady_clock::now()<request->deadline)Open(request->functions);
                else SKSE::log::info("Wheel opening canceled on player update: {}",reason?reason:"state changed or expired");
            },[&](const char* error){FailedTask(request->generation,request->serial,"open",error);});
        }
        void FunctionNavigate(FaceLight::Section section,bool preservePointer=false) {
            {std::lock_guard lock(viewMutex);
                section=FaceLight::VisibleSection(section,view.faceLightAvailable);
                if(CurrentFormMode()==FormMode::Werewolf && section!=FaceLight::Section::Combat && section!=FaceLight::Section::Cycles && section!=FaceLight::Section::CycleMembers)section=FaceLight::Section::Combat;
                RememberFunctionPage();
                functionCategory=section==FaceLight::Section::Followers?FaceLight::Section::Lighting:section==FaceLight::Section::CycleMembers?FaceLight::Section::Cycles:section;
                view.functionSection=section;view.page=FunctionPage(section);
                if(!preservePointer)view.x=view.y=0;
            }
            RefreshFunctions();
        }
        void FunctionBack() {
            const auto section=Snapshot().functionSection;
            FunctionNavigate(CurrentFormMode()==FormMode::Werewolf?FaceLight::Section::Combat:section==FaceLight::Section::Followers?FaceLight::Section::Lighting:section==FaceLight::Section::CycleMembers?FaceLight::Section::Cycles:FaceLight::Section::Outfits);
        }
        void ChangeCategory(int direction) {
            const auto current=Snapshot();
            if(current.functions) {
                const auto section=FaceLight::ChangeType(current.functionSection,direction,current.faceLightAvailable);
                if(section!=current.functionSection)FunctionNavigate(section,true);
                return;
            }
            std::lock_guard lock(viewMutex);
            if(!inventoryLoaded || view.inventoryLoading)return;
            const auto category=CategoryVisibility(inventoryPages.visible,Config().hideEmptyCategories).Move(view.category,direction);
            if(category==view.category)return; // One type (or no items) preserves its page.
            view.sortingEdit=false;CancelSortDrag();view.category = category;
            view.page = 0; // Keep the pointer on the same sector when changing type.
            FilterItems();
        }
        void ChangePage(int direction,const char* source=nullptr) {
            std::lock_guard lock(viewMutex);
            const auto previous=view.page;
            const auto pages=PageCount(ItemCount(view));
            const auto next=Wrap(previous+direction,pages);
            if(source)SKSE::log::info("Controller page: key={} wheel={} category={} section={} items={} pages={} previous={} next={}",
                source,view.functions?"functions":"favorites",static_cast<int>(view.category),static_cast<int>(view.functionSection),
                ItemCount(view),pages,previous+1,next+1);
            if(next==previous)return;
            view.page = next;
            if(view.sortDrag.held)view.sortDrag.Page();
            else view.x = view.y = 0;
            FilterItems();
        }
        void OutfitDialog(int mode,std::uint32_t id,const std::string& name,bool combat=false) {
            if(retainedAction)return;
            {std::lock_guard lock(viewMutex);view.outfitDialog=mode;view.combatDialog=combat;view.cycleDialog=view.cycleMemberDialog=view.cycleCaptureKey=false;view.outfitId=id;view.outfitName.Set(name);view.x=view.y=0;}
            SyncWheelTime();
        }
        void EndOutfitDialog() {
            if(Snapshot().outfitDialog==1 || Snapshot().outfitDialog==2)TextBridge::Reset();
            {std::lock_guard lock(viewMutex);view.outfitDialog=0;view.combatDialog=view.cycleDialog=view.cycleMemberDialog=view.cycleCaptureKey=false;view.x=view.y=0;}
            SyncWheelTime();RefreshFunctions();
        }
        void CycleDialog(int mode,std::uint32_t group,const std::string& name,std::uint32_t member=0) {
            OutfitDialog(mode,group,name);const auto binding=Cycles::Binding(group);
            std::lock_guard lock(viewMutex);view.cycleDialog=true;view.cycleMemberDialog=member!=0;
            if(view.cycleGroup!=group)cycleMemberPage=0;
            view.cycleGroup=group;view.cycleMember=member;view.cycleBinding=binding;
        }
        void QueueCycleCapture(std::uint32_t group,std::uint32_t preset=0) {
            const auto current=Snapshot();if(!current.open)return;
            Item item;item.action=ActionKind::CycleCapture;item.actionId=group;item.actionIndex=preset;
            const bool paused=current.config.timeMode==0;
            {std::lock_guard lock(actionMutex);pendingAction=PendingAction{item,false,epoch.load(),std::chrono::steady_clock::now()+std::chrono::seconds(2),true,openSerial.load(),paused};}
            retainedAction=true;retainedKeepsPause=paused;retainedSubmitted=false;SyncWheelTime();
        }
        void Activate(bool left) {
            if(!ValidPlayer()) {Cancel();return;}
            std::lock_guard lifecycle(lifecycleMutex);
            if(HasPendingAction() || retainedAction || Outfits::Busy() || Combat::Busy())return;
            const auto current = Snapshot();
            if(current.sortingEdit){
                if(current.sortDrag.held)return;
                const int target=WheelSlot(current.x,current.y,Style(current.config).layout);
                const int local=PageItemIndex(current,target);
                if(target>=0 && local>=0 && local<static_cast<int>(current.items.size())) {
                    SelectSortedItem();MoveSortedItem(left?1:-1,true);
                }
                return;
            }
            const int slot = WheelSlot(current.x, current.y,Style(current.config).layout);
            const int index = PageItemIndex(current,slot);
            if(current.inventoryLoading && !current.functions)return;
            if (slot < 0 || index < 0 || index >= static_cast<int>(current.items.size())) {
                if(current.functions)SKSE::log::info("Function click ignored: neutral/empty slot={} x={:.3f} y={:.3f}",slot,current.x,current.y);
                return;
            }
            const auto selected = current.items[index];
            if(!CurrentActionAllowed(selected))return;
            if(current.functions)SKSE::log::info("Function click: action={} preset={} usable={} equipped={} right={}",static_cast<int>(selected.action),selected.actionId,selected.usable,selected.equipped,left);
            if(selected.action==ActionKind::FunctionBack){FunctionBack();return;}
            if(selected.action==ActionKind::FaceLightMenu || selected.action==ActionKind::FaceLightFollowers) {
                if(selected.usable)FunctionNavigate(selected.action==ActionKind::FaceLightMenu?FaceLight::Section::Lighting:FaceLight::Section::Followers);
                else RE::SendHUDMessage::ShowHUDMessage(selected.detail.c_str());
                return;
            }
            if(selected.action==ActionKind::SaveOutfit){OutfitDialog(1,0,Tr(Config(),"outfitDefault"));return;}
            if(selected.action==ActionKind::SaveCombat){OutfitDialog(1,0,Tr(Config(),"combatDefault"),true);return;}
            if(selected.action==ActionKind::CycleCapture){QueueCycleCapture(selected.actionId);return;}
            if(selected.action==ActionKind::ImportOutfits){Outfits::Import();RefreshFunctions();return;}
            if(selected.action==ActionKind::Outfit && left){OutfitDialog(3,selected.actionId,selected.name);return;}
            if(selected.action==ActionKind::Combat && left){OutfitDialog(3,selected.actionId,selected.name,true);return;}
            if((selected.action==ActionKind::Cycle || selected.action==ActionKind::CycleMember) && left) {
                CycleDialog(3,selected.actionId,selected.name,selected.action==ActionKind::CycleMember?selected.actionIndex:0);return;
            }
            if (!selected.usable) {
                RE::SendHUDMessage::ShowHUDMessage((selected.action==ActionKind::Cycle?Tr(Config(),"cycleEmpty"):selected.action==ActionKind::FaceLightCommand || selected.action==ActionKind::Combat || selected.action==ActionKind::CycleMember?selected.detail:Tr(Config(),selected.action==ActionKind::Outfit?"outfitMissing":"unsupported")).c_str());
                return;
            }
            // Outfit changes close the overlay so the player can observe the full transition.
            const bool consumable=selected.action==ActionKind::Favorite && ConsumableCategory(selected.category);
            const bool keep=selected.action!=ActionKind::Outfit && selected.action!=ActionKind::Combat && selected.action!=ActionKind::Cycle && selected.action!=ActionKind::CycleMember && KeepWheelAfterUse(current.config.keepOpen,consumable,current.config.keepOpenConsumables &&
                (selected.category==Category::Potions || selected.category==Category::Food),selected.book || selected.scriptedInstrument);
            const bool keepPaused=KeepPausedForUse(keep,selected.action==ActionKind::Favorite,consumable,current.config.timeMode);
            {
                std::lock_guard lock(actionMutex);
                pendingAction = PendingAction{selected, left, epoch.load(), std::chrono::steady_clock::now() + std::chrono::seconds(2),keep,openSerial.load(),keepPaused};
            }
            if(keep) {
                retainedKeepsPause=keepPaused;retainedAction=true;retainedSubmitted=false;SyncWheelTime();
            }else Cancel(true);
        }
        void BindQuickSlot(int slot) {
            if(CurrentFormMode()!=FormMode::Human)return;
            const auto current=Snapshot();
            if(current.sortingEdit)return;
            const int radial=WheelSlot(current.x,current.y,Style(current.config).layout),index=PageItemIndex(current,radial);
            if(current.settingsOpen || current.outfitDialog || radial<0 || index<0 ||
                index>=static_cast<int>(current.items.size()) || current.inventoryLoading || !ValidQuickSlot(slot))return;
            const auto selected=current.items[index];
            const bool combat=selected.action==ActionKind::Combat;
            if(current.functions && !combat)return;
            if(!selected.favorited){RE::SendHUDMessage::ShowHUDMessage(Tr(Config(),"quickSlotNeedsFavorite").c_str());return;}
            const auto generation=epoch.load(),serial=openSerial.load();
            if(auto tasks=SKSE::GetTaskInterface())tasks->AddTask([selected,slot,generation,serial,combat] {
                std::lock_guard lifecycle(lifecycleMutex);
                const auto now=Snapshot();
                if(generation!=epoch.load() || serial!=openSerial.load() || !now.open || now.functions!=combat ||
                    now.settingsOpen || now.outfitDialog || !Focused() || !ValidPlayer() || BlockedMenu() ||
                    HasPendingAction() || Outfits::Busy() || Combat::Busy())return;
                int next=-1;
                if(combat) {
                    if(now.functionSection!=FaceLight::Section::Combat || !Combat::BindQuickSlot(selected.actionId,slot,&next))return;
                    if(next>=0) {
                        std::lock_guard lock(viewMutex);
                        for(auto& item:inventoryPages.items)if(item.quickSlot==slot)SetQuickSlotSnapshot(item,-1);
                    }
                    RE::SendHUDMessage::ShowHUDMessage(Tr(Config(),next<0?"combatQuickUnbound":"combatQuickBound").c_str());
                    RefreshFunctions();return;
                }
                if(!BindFavoriteQuickSlot(selected,slot,&next))return;
                Combat::ClearQuickSlot(slot);
                std::lock_guard lock(viewMutex);
                if(generation!=epoch.load() || serial!=openSerial.load() || !view.open || view.functions)return;
                // Preserve expensive item-info snapshots and cursor/category state.
                for(auto* catalog:{&inventoryPages.items,&unsortedItems})for(auto& item:*catalog) {
                    SetQuickSlotSnapshot(item,ReassignedQuickSlot(item.quickSlot,item.key==selected.key,slot,next));
                }
                FilterItems();
            });
        }
        bool TryCombatQuickSlot(int slot,bool wheelEntrance,int modifiers) {
            if(CurrentFormMode()!=FormMode::Human)return false;
            std::lock_guard lifecycle(lifecycleMutex);
            if(!ValidQuickSlot(slot) || !Combat::CombatQuickSlotMayRun(IsOpen(),closing,openQueued,
                OpenBlockReason()!=nullptr,modifiers,wheelEntrance))return false;
            const auto id=Combat::PresetForQuickSlot(slot);if(!id)return false;
            // A later SkyUI/vanilla binding owns its native event. Detect only
            // on a fresh shortcut press, not by scanning inventory every frame.
            if(RouteQuickSlot(id,FavoriteQuickSlotOccupied(slot))==QuickSlotRoute::Native) {
                const auto generation=epoch.load();
                if(auto tasks=SKSE::GetTaskInterface())tasks->AddTask([slot,id,generation] {
                    std::lock_guard lifecycle(lifecycleMutex);
                    if(generation==epoch.load() && FavoriteQuickSlotOccupied(slot))Combat::ClearQuickSlot(slot,id);
                });
                SKSE::log::info("Combat quick slot yielded to later native binding: slot={} preset={}",slot+1,id);
                return false;
            }
            Item item;item.action=ActionKind::Combat;item.actionId=id;
            {
                std::lock_guard lock(actionMutex);
                pendingAction=PendingAction{item,false,epoch.load(),std::chrono::steady_clock::now()+std::chrono::seconds(2),false,openSerial.load(),false};
            }
            SKSE::log::info("Combat quick slot activated: slot={} preset={}",slot+1,id);
            return true;
        }
        void PumpDetails() {
            const auto serial=openSerial.load();
            if(hoverSerial!=serial){hoverSerial=serial;hoverKey.reset();}
            const auto current=Snapshot();
            const int radial=WheelSlot(current.x,current.y,Style(current.config).layout),index=PageItemIndex(current,radial);
            if(!current.open || current.functions || current.settingsOpen || current.outfitDialog || current.inventoryLoading ||
                radial<0 || index<0 || index>=static_cast<int>(current.items.size())){hoverKey.reset();return;}
            const auto& item=current.items[index];
            const auto now=std::chrono::steady_clock::now();
            const auto tick=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());
            if(!hoverKey || *hoverKey!=item.key){hoverKey=item.key;hoverStarted=now;return;}
            if(item.infoReady && (current.config.timeMode==0 || tick-item.infoReadAt<500))return;
            if(now-hoverStarted<std::chrono::milliseconds(80) || detailTaskQueued.exchange(true))return;
            const auto generation=epoch.load(),revision=directoryRevision.load();
            std::size_t catalogIndex;
            {std::lock_guard lock(viewMutex);catalogIndex=inventoryPages.Index(current.category,current.page,radial);}
            auto failed=[generation,serial](const char* error){FailedTask(generation,serial,"detail",error);};
            ScheduleQueuedTask(detailTaskQueued,[&] {
                if(auto tasks=SKSE::GetTaskInterface())tasks->AddTask([selected=item,catalogIndex,generation,serial,revision,failed] {
                    RunQueuedTask(detailTaskQueued,[&] {
                        auto stillSelected=[&] {
                            const auto v=Snapshot();const int slot=WheelSlot(v.x,v.y,Style(v.config).layout),i=PageItemIndex(v,slot);
                            return generation==epoch.load() && serial==openSerial.load() && revision==directoryRevision.load() &&
                                v.open && !v.functions && !v.settingsOpen && !v.outfitDialog && slot>=0 &&
                                i>=0 && i<static_cast<int>(v.items.size()) && v.items[i].key==selected.key;
                        };
                        if(stillSelected() && Focused() && ValidPlayer() && !BlockedMenu()) {
                            const auto started=std::chrono::steady_clock::now();
                            ItemInfo info;const bool valid=ReadItemInfo(selected,info);
                            const auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
                            if(elapsed>8)SKSE::log::info("Inventory detail: form={:08X} total_ms={:.2f}",selected.key.form,elapsed);
                            std::lock_guard lock(viewMutex);
                            if(generation==epoch.load() && serial==openSerial.load() && revision==directoryRevision.load() &&
                                view.open && catalogIndex<inventoryPages.items.size() && inventoryPages.items[catalogIndex].key==selected.key) {
                                auto& cached=inventoryPages.items[catalogIndex];cached.info=std::move(info);cached.infoReady=true;
                                cached.infoReadAt=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                    std::chrono::steady_clock::now().time_since_epoch()).count());
                                cached.detail=valid?std::string{}:Tr(Config(),"changed");
                                if(!view.functions)for(auto& visible:view.items)if(visible.key==cached.key)visible=cached;
                            }
                        }
                    },failed);
                });else detailTaskQueued=false;
            },failed);
        }
        std::optional<PendingAction> TakeAction(ActionExecutor executor) {
            std::lock_guard lifecycle(lifecycleMutex);
            std::lock_guard lock(actionMutex);
            if(!pendingAction || ExecutorFor(pendingAction->item.action==ActionKind::FaceLightCommand)!=executor)return {};
            auto ui=RE::UI::GetSingleton();
            const auto guard=ui?ui->GetMenu(pauseMenuName):nullptr;
            const bool onlyWheelPause=ui && OnlyWheelPause(ui->IsMenuOpen(pauseMenuName),
                guard && guard->PausesGame(),ui->numPausesGame);
            const auto decision=DecideRetainedActionOn(executor,
                ExecutorFor(pendingAction->item.action==ActionKind::FaceLightCommand),
                pendingAction->generation,epoch.load(),
                ui && ValidPlayer() && Focused() && !BlockedMenu() && pendingAction->formMode==CurrentFormMode() &&
                    CurrentActionAllowed(pendingAction->item),closing || (!pendingAction->retained && TimeControl::Active()),
                ui && (ui->IsMenuOpen(menuName) || ui->IsMenuOpen(pauseMenuName)),ui && ui->GameIsPaused(),
                std::chrono::steady_clock::now()>=pendingAction->deadline,
                pendingAction->retained,pendingAction->serial==openSerial.load(),IsOpen(),pendingAction->keepPaused,onlyWheelPause);
            if(decision==ActionDecision::Wait)return {};
            auto submit=decision==ActionDecision::Submit?std::move(pendingAction):std::optional<PendingAction>{};
            if(decision==ActionDecision::Discard) {
                SKSE::log::info("Pending action canceled: state changed or close timed out");
                retainedAction=false;retainedKeepsPause=false;retainedSubmitted=false;
            }
            pendingAction.reset();return submit;
        }
        void PumpAction() {
            bool taskAction;
            {std::lock_guard lock(actionMutex);taskAction=pendingAction &&
                ExecutorFor(pendingAction->item.action==ActionKind::FaceLightCommand)==ActionExecutor::TaskQueue;}
            if ((!taskAction && !Outfits::Busy() && !Combat::Busy()) || actionTaskQueued.exchange(true)) return;
            const auto generation=epoch.load(),serial=openSerial.load();
            auto failed=[generation,serial](const char* error){FailedTask(generation,serial,"action",error,true);};
            // Favorites and outfit jobs retain their existing SKSE task path.
            ScheduleQueuedTask(actionTaskQueued,[&] {
                if(auto tasks=SKSE::GetTaskInterface())tasks->AddTask([generation,failed] {
                    RunQueuedTask(actionTaskQueued,[&] {
                        std::lock_guard lifecycle(lifecycleMutex);
                        if(generation!=epoch.load())return;
                        auto submit=TakeAction(ActionExecutor::TaskQueue);
                        if (submit) {
                            if(submit->item.action==ActionKind::Outfit){outfitGeneration=submit->generation;Outfits::Apply(submit->item.actionId);}
                            else if(submit->item.action==ActionKind::Combat){outfitGeneration=submit->generation;Combat::Apply(submit->item.actionId);}
                            else if(submit->item.action==ActionKind::Cycle || submit->item.action==ActionKind::CycleMember){outfitGeneration=submit->generation;Cycles::Apply(submit->item.actionId,submit->item.actionIndex);}
                            else if(submit->item.action==ActionKind::CycleCapture){Cycles::Append(submit->item.actionId,submit->item.actionIndex);RefreshFunctions();}
                            else UseFavorite(submit->item, submit->left);
                            CompleteRetainedAction(*submit);
                        }
                        if(!submit && Outfits::Busy()) {
                            auto ui=RE::UI::GetSingleton();
                            Outfits::Tick(CurrentFormMode()==FormMode::Human && outfitGeneration==epoch.load() && ValidPlayer() && Focused() && !BlockedMenu() && ui && !ui->GameIsPaused());
                        }
                        if(!submit && Combat::Busy()) {
                            auto ui=RE::UI::GetSingleton();
                            const auto mode=CurrentFormMode();
                            Combat::Tick((mode==FormMode::Human || mode==FormMode::Werewolf) && outfitGeneration==epoch.load() && ValidPlayer() && Focused() && !BlockedMenu() && ui && !ui->GameIsPaused());
                        }
                    },failed);
                });else actionTaskQueued=false;
            },failed);
        }
        void PumpRetainedAction() { // Only player-update may refresh the Face Lighting API.
            std::lock_guard lifecycle(lifecycleMutex);
            if(!retainedAction || !retainedSubmitted || !IsOpen())return;
            const double elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-retainedAt).count();
            if(!RetainedActionSettled(HasPendingAction(),Outfits::Busy() || Combat::Busy(),elapsed,playerUpdates.load()-retainedUpdate))return;
            retainedAction=false;retainedKeepsPause=false;retainedSubmitted=false;
            if(CurrentFormMode()==FormMode::Human)FaceLight::Capture();
            const auto current=Snapshot();
            if(current.functions)RefreshFunctions();
            // Coalesce one post-action inventory refresh, also in pause mode.
            // PublishDirectory retains aim unless the hovered instance vanished.
            bool loaded;{std::lock_guard lock(viewMutex);loaded=inventoryLoaded;}
            if(loaded)RequestDirectory(true);
        }
        void PlayerUpdate(RE::PlayerCharacter* player,float delta) {
            // Face Lighting establishes its accepted thread in the previous hook.
            // Do not call its API from input, Present or an SKSE task callback.
            previousPlayerUpdate(player,delta);
            std::lock_guard lifecycle(lifecycleMutex);
            ++playerUpdates;
            if(!updateLogged.exchange(true))SKSE::log::info("Wheel player-update callback active: thread={}",GetCurrentThreadId());
            if(auto submit=TakeAction(ActionExecutor::PlayerUpdate)) {
                FaceLight::Execute(submit->item);CompleteRetainedAction(*submit);
            }
            PumpRetainedAction();
            if(IsOpen())SyncWheelTime();
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
        bool StartupMenuOpen(RE::UI* ui) {
            return !ui || ui->IsMenuOpen(RE::MainMenu::MENU_NAME) || ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME) ||
                ui->IsMenuOpen(RE::Console::MENU_NAME) || ui->IsMenuOpen(RE::RaceSexMenu::MENU_NAME);
        }
        void ProbeInitialSession() {
            // Input dispatch runs even on quiet frames. Only the missing initial
            // session case schedules work; never inspect actor/cell state here.
            if(!wheelInstalled || !gameSession.AwaitingWorld() || StartupMenuOpen(RE::UI::GetSingleton()))return;
            auto tasks=SKSE::GetTaskInterface();
            if(!tasks || sessionProbeQueued.exchange(true))return;
            const auto generation=epoch.load();
            auto failed=[](const char* error){SKSE::log::error("Initial game-session probe failed: {}",error);};
            ScheduleQueuedTask(sessionProbeQueued,[&] {
                tasks->AddTask([generation,failed] {
                    RunQueuedTask(sessionProbeQueued,[&] {
                        std::lock_guard lifecycle(lifecycleMutex);
                        if(!gameSession.AwaitingWorld() || generation!=epoch.load())return;
                        auto ui=RE::UI::GetSingleton();auto player=RE::PlayerCharacter::GetSingleton();
                        auto cell=player?player->GetParentCell():nullptr;
                        const StartupWorld world{wheelInstalled.load(),ui!=nullptr,StartupMenuOpen(ui),
                            player!=nullptr,player && player->Get3D(),cell && cell->IsAttached(),
                            !player || player->GetPlayerFlags().isLoading};
                        if(!gameSession.CanBootstrap(world,generation,epoch.load()))return;
                        SKSE::log::info("Game session recovered: source=attached-player-world startup=implicit-v1 cell={:08X} thread={}",
                            cell->GetFormID(),GetCurrentThreadId());
                        // Installs the player update hook too, outside every
                        // plugin's DataLoaded callback, just like a normal start.
                        SetGameActive(true);
                    },failed);
                });
            },failed);
        }
        std::uint32_t FavoritesScanCode(RE::INPUT_DEVICE device=RE::INPUT_DEVICE::kKeyboard) {
            auto controls = RE::ControlMap::GetSingleton();
            return controls ? controls->GetMappedKey("Favorites", device, RE::UserEvents::INPUT_CONTEXT_ID::kGameplay) : 0xFFFFFFFF;
        }
        int HeldModifiers() {
            return ((keyboardHeld[42]||keyboardHeld[54])?1:0) |
                ((keyboardHeld[29]||keyboardHeld[157])?2:0) | ((keyboardHeld[56]||keyboardHeld[184])?4:0);
        }
        int EffectiveKeyboardKey(const Settings& config) {
            return ResolveFavoritesKey(config.hotkey,FavoritesScanCode());
        }
        int EffectiveGamepadKey(const Settings& config) {
            if(config.gamepadHotkey>=0)return config.gamepadHotkey;
            const auto mapped=FavoritesScanCode(RE::INPUT_DEVICE::kGamepad);
            if(mapped==0xFF || mapped==0xFFFFFFFF)return -1;
            const auto key=SKSE::InputMap::GamepadMaskToKeycode(mapped);
            return key>=266 && key<=281?static_cast<int>(key):-1;
        }
        std::uint16_t NativeGamepadModifiers() {
            RE::ControlMap::UserEventMapping mapping{};
            auto controls=RE::ControlMap::GetSingleton();
            return controls && controls->GetMappingFromEventName("Favorites",RE::UserEvents::INPUT_CONTEXT_ID::kGameplay,
                RE::INPUT_DEVICE::kGamepad,mapping)?ControllerModifierMask(mapping.modifier):0;
        }
        std::uint16_t OpeningControllerHeld(ControllerSample sample) {
            const auto config=Config();
            return ControllerOpeningHeld(padChords.Mask(),sample,EffectiveGamepadKey(config),
                NativeGamepadModifiers() | ControllerKeyBit(config.gamepadModifier) | ControllerKeyBit(config.gamepadActionModifier));
        }
        bool ReplacedEntrance(const RE::ButtonEvent* button,ControllerSample sample) {
            const auto config=Config();
            if(button->GetDevice()==RE::INPUT_DEVICE::kKeyboard)
                return button->GetIDCode()==FavoritesScanCode() && static_cast<int>(button->GetIDCode())==EffectiveKeyboardKey(config);
            if(button->GetDevice()==RE::INPUT_DEVICE::kGamepad)
                return button->GetIDCode()==FavoritesScanCode(RE::INPUT_DEVICE::kGamepad) &&
                    static_cast<int>(SKSE::InputMap::GamepadMaskToKeycode(button->GetIDCode()))==EffectiveGamepadKey(config) &&
                    !ForeignGamepadChord(OpeningControllerHeld(sample),EffectiveGamepadKey(config),NativeGamepadModifiers(),
                        config.gamepadHotkey<0,config.gamepadModifier,config.gamepadActionModifier) &&
                    (OpeningControllerHeld(sample) & NativeGamepadModifiers())==NativeGamepadModifiers();
            return false;
        }
        Opening ToggleKey(const RE::ButtonEvent* button,ControllerSample sample) {
            const auto config=Config();
            if(button->GetDevice()==RE::INPUT_DEVICE::kKeyboard) {
                const int favorite=EffectiveKeyboardKey(config);
                return KeyboardOpening(button->GetIDCode(),HeldModifiers(),favorite,config.hotkeyModifier,
                    config.actionHotkey<0?favorite:config.actionHotkey,config.actionModifier);
            }
            if(button->GetDevice()==RE::INPUT_DEVICE::kGamepad) {
                const int favorite=EffectiveGamepadKey(config);
                return ResolvedGamepadOpening(SKSE::InputMap::GamepadMaskToKeycode(button->GetIDCode()),favorite,
                    OpeningControllerHeld(sample),NativeGamepadModifiers(),config.gamepadHotkey<0,
                    config.gamepadModifier,config.gamepadActionModifier);
            }
            return Opening::None;
        }

        bool CycleBindingConflict(int key,int modifiers) {
            const auto config=Config();const int favorite=EffectiveKeyboardKey(config);
            return Cycles::EntranceConflict(key,modifiers,favorite,config.hotkeyModifier,
                config.actionHotkey<0?favorite:config.actionHotkey,config.actionModifier,config.switchKey);
        }
        bool TryCycleShortcut(int key,int modifiers,bool entrance) {
            const auto mode=CurrentFormMode();if(mode!=FormMode::Human && mode!=FormMode::Werewolf)return false;
            const auto current=Snapshot();
            if(entrance || current.settingsOpen || current.outfitDialog || closing || openQueued ||
                !Config().enabled || !gameSession.Active() || !Focused() || !ValidPlayer() || BlockedMenu())return false;
            const auto id=Cycles::Match(key,modifiers,current.open);if(!id)return false;
            if(!current.open) {
                auto ui=RE::UI::GetSingleton();if(!ui || ui->GameIsPaused())return false;
                if(const auto reason=OpenBlockReason();reason && std::string_view(reason)!="preset changing" && std::string_view(reason)!="item action pending")return false;
            }
            if(HasPendingAction() || retainedAction || Outfits::Busy() || Combat::Busy())return true;
            if(current.open) {
                const int slot=WheelSlot(current.x,current.y,Style(current.config).layout),index=PageItemIndex(current,slot);
                std::uint32_t preset=0;
                if(slot>=0 && index>=0 && index<int(current.items.size())) {
                    const auto& item=current.items[index];
                    if(item.action==ActionKind::Combat)preset=item.actionId;
                    else if(item.action==ActionKind::CycleMember)preset=item.actionIndex;
                }
                QueueCycleCapture(id,preset);
            }else {
                Item item;item.action=ActionKind::Cycle;item.actionId=id;
                std::lock_guard lock(actionMutex);
                pendingAction=PendingAction{item,false,epoch.load(),std::chrono::steady_clock::now()+std::chrono::seconds(2),false,openSerial.load(),false};
            }
            return true;
        }
        void EnterSettings() {
            if(CurrentFormMode()!=FormMode::Human)return;
            if(retainedAction)return;
            BeginSettings();
            {std::lock_guard lock(viewMutex);
                view.sortingEdit=false;CancelSortDrag();view.settingsOpen=true; view.capturingKey=false; view.settingsTab=0; view.saveError=false;
                view.x=view.y=0;padX=padY=0;}
            SyncWheelTime();
        }
        void LeaveSettings(bool save) {
            if (save && !SaveSettings()) {
                SKSE::log::error("Settings save failed: {}",SettingsDiagnostic());
                std::lock_guard lock(viewMutex);view.saveError=true;return;
            }
            if (save) {
                const auto config=Config();
                SKSE::log::info("Settings saved: {}; language={} theme={} scale={} hotkey={} keepOpen={} padCategory={} padMove={}",
                    SettingsDiagnostic(),config.language,config.theme,config.scale,config.hotkey,config.keepOpen,config.gamepadCategoryButtons,config.gamepadMoveWhileOpen);
            }
            if (!save) RevertSettings();
            bool reload=false;
            {std::lock_guard lock(viewMutex);
                view.settingsOpen=view.capturingKey=view.saveError=false;view.x=view.y=0;
                padX=padY=0;
                if(save && Config().allInventory!=inventoryScope) {
                    inventoryLoaded=false;++directoryRevision;reload=!view.functions;
                }
                if(!reload && inventoryLoaded && !view.functions)FilterItems();
            }
            if(reload)RequestDirectory();
            SyncWheelTime();
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
            case 12: config.hotkeyModifier=Wrap(config.hotkeyModifier+direction,8); break;
            case 13: config.actionHotkey=-1;config.actionModifier=1; break;
            case 14: config.actionModifier=Wrap(config.actionModifier+direction,8); break;
            case 15: config.gamepadHotkey=-1; break;
            case 16: config.gamepadModifier=Wrap((config.gamepadModifier<0?0:config.gamepadModifier-265)+direction,17);config.gamepadModifier=config.gamepadModifier?config.gamepadModifier+265:-1; break;
            case 17: config.gamepadActionModifier=Wrap((config.gamepadActionModifier<0?0:config.gamepadActionModifier-265)+direction,17);config.gamepadActionModifier=config.gamepadActionModifier?config.gamepadActionModifier+265:-1; break;
            case 18: config.allInventory=!config.allInventory; break;
            case 19: config.timeMode=Wrap(config.timeMode+direction,3); break;
            case 20: config.slowPercent+=direction*5; break;
            case 21: config.gamepadCategoryButtons=1-config.gamepadCategoryButtons; break;
            case 22: config.keepOpen=!config.keepOpen; break;
            case 23: config.gamepadMoveWhileOpen=!config.gamepadMoveWhileOpen; break;
            case 24: config.hideEmptyCategories=!config.hideEmptyCategories; break;
            case 25: config.swapMouseHands=!config.swapMouseHands; break;
            case 26: config.rememberLastWheel=!config.rememberLastWheel; break;
            case 27: config.keepOpenConsumables=!config.keepOpenConsumables; break;
            case 28: config.showActionNotifications=!config.showActionNotifications; break;
            }
            EditSettings(config);
            std::lock_guard lock(viewMutex); view.saveError=false;
        }
        void SettingsClick(bool right) {
            const auto current=Snapshot();
            const float x=current.x*224, y=current.y*224;
            if (current.capturingKey) return;
            if(generalTab.Contains(x,y) || controlsTab.Contains(x,y) || gameplayTab.Contains(x,y) || gamepadTab.Contains(x,y)) {
                std::lock_guard lock(viewMutex);view.settingsTab=gameplayTab.Contains(x,y)?2:gamepadTab.Contains(x,y)?3:controlsTab.Contains(x,y)?1:0;return;
            }
            if (applyButton.Contains(x,y) && !right) { LeaveSettings(true); return; }
            if (cancelButton.Contains(x,y) && !right) { LeaveSettings(false); return; }
            if (defaultsButton.Contains(x,y) && !right) { DefaultSettings(); return; }
            for (int slot=0;slot<SettingCount(current.settingsTab);++slot) {
                const int row=SettingRow(current.settingsTab,slot);
                if(!BindingRow(row) && MinusButton(slot).Contains(x,y)) { AdjustSetting(row,-1); return; }
                if(!BindingRow(row) && PlusButton(slot).Contains(x,y)) { AdjustSetting(row,1); return; }
                if(ValueButton(slot).Contains(x,y)) {
                    if(BindingRow(row) && !right) { std::lock_guard lock(viewMutex); view.capturingKey=true;view.captureBinding=row; }
                    else AdjustSetting(row,right?-1:1);
                    return;
                }
            }
        }

        void OutfitAccept() {
            const auto current=Snapshot();bool ok=false;
            if(current.cycleDialog) {
                if(current.outfitDialog==2)ok=Cycles::Rename(current.cycleGroup,current.outfitName.text);
                else if(current.outfitDialog==5)ok=current.cycleMemberDialog?Cycles::Remove(current.cycleGroup,current.cycleMember):Cycles::Clear(current.cycleGroup);
                else {EndOutfitDialog();return;}
                if(ok)EndOutfitDialog();return;
            }
            switch(current.outfitDialog) {
            case 1:ok=current.combatDialog?Combat::Capture(current.outfitName.text):Outfits::Capture(current.outfitName.text);break;
            case 2:ok=current.combatDialog?Combat::Rename(current.outfitId,current.outfitName.text):Outfits::Rename(current.outfitId,current.outfitName.text);break;
            case 4:ok=current.combatDialog?Combat::Capture(current.outfitName.text,current.outfitId):Outfits::Capture(current.outfitName.text,current.outfitId);break;
            case 5:ok=current.combatDialog?Combat::Remove(current.outfitId):Outfits::Remove(current.outfitId);break;
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
            if(current.cycleDialog) {
                if(current.cycleMemberDialog) {
                    if(outfitOverwriteButton.Contains(x,y)){Cycles::Move(current.cycleGroup,current.cycleMember,-1);RefreshFunctions();}
                    else if(outfitDeleteButton.Contains(x,y)){Cycles::Move(current.cycleGroup,current.cycleMember,1);RefreshFunctions();}
                    else if(outfitExportButton.Contains(x,y))CycleDialog(5,current.cycleGroup,current.outfitName.text,current.cycleMember);
                }else {
                    if(outfitNameButton.Contains(x,y))CycleDialog(2,current.cycleGroup,current.outfitName.text);
                    else if(outfitOverwriteButton.Contains(x,y)){std::lock_guard lock(viewMutex);view.cycleCaptureKey=true;}
                    else if(outfitDeleteButton.Contains(x,y))CycleDialog(5,current.cycleGroup,current.outfitName.text);
                    else if(outfitExportButton.Contains(x,y)){EndOutfitDialog();FunctionNavigate(FaceLight::Section::CycleMembers);}
                }
                return;
            }
            if(outfitNameButton.Contains(x,y))OutfitDialog(2,current.outfitId,current.outfitName.text,current.combatDialog);
            else if(outfitOverwriteButton.Contains(x,y))OutfitDialog(4,current.outfitId,current.outfitName.text,current.combatDialog);
            else if(outfitDeleteButton.Contains(x,y))OutfitDialog(5,current.outfitId,current.outfitName.text,current.combatDialog);
            else if(current.combatDialog && combatLeftButton.Contains(x,y)){if(Combat::CycleSlot(current.outfitId,Combat::Slot::Left))RefreshFunctions();}
            else if(current.combatDialog && combatRightButton.Contains(x,y)){if(Combat::CycleSlot(current.outfitId,Combat::Slot::Right))RefreshFunctions();}
            else if(current.combatDialog && combatVoiceButton.Contains(x,y)){if(Combat::CycleSlot(current.outfitId,Combat::Slot::Voice))RefreshFunctions();}
            else if(!current.combatDialog && outfitExportButton.Contains(x,y))Outfits::Export(current.outfitId);
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
            ProbeInitialSession();
            auto ui=RE::UI::GetSingleton();
            bool captureBatch = IsOpen() || closing || openQueued ||
                (ui && (ui->IsMenuOpen(menuName) || ui->IsMenuOpen(pauseMenuName)));
            movementStick.Capture(captureBatch);
            if (IsOpen() && (!Focused() || !ValidPlayer() || BlockedMenu())) Cancel();
            auto bridge=TextBridge::Query();
            const bool simpleIME=SimpleIME::Available();
            bridge.composing=bridge.composing || SimpleIME::Composing();
            bool characterBatch=false;
            // Resolve chords from the whole physical batch before mutating events.
            // The main button may precede its modifier in the same input poll.
            ControllerSample controllerSample;
            if(auto manager=RE::BSInputDeviceManager::GetSingleton()) {
                if(auto keyboard=manager->GetKeyboard())
                    for(unsigned i=0;i<256;++i)keyboardHeld[i]=(keyboard->GetRuntimeData().curState[i]&0x80)!=0;
                auto pad=manager->GetGamepad();
                const bool connected=pad && pad->IsEnabled();
                padChords.Attach(connected?pad:nullptr);
                // Use the native delegate only after checking its runtime type.
                // Other device providers keep the existing event-only path.
                if(connected)if(auto native=skyrim_cast<RE::BSWin32GamepadDevice*>(pad)) {
                    const auto state=native->GetRuntimeData().currentState.gamepad;
                    controllerSample={true,ControllerPacketButtons(state.buttons,state.leftTrigger,state.rightTrigger)};
                }
                if(!connected){movementStick.Reset();lookStick.Reset(AppliedGamepadMoveWhileOpen());controllerPages.Reset();controllerTriggers.Reset();std::lock_guard lock(viewMutex);padX=padY=0;}
            }
            for(auto event=events?*events:nullptr;event;event=event->next) {
                if(auto c=event->AsCharEvent();c && c->keyCode>=32)characterBatch=true;
                if(auto b=event->AsButtonEvent()) {
                    if(b->GetDevice()==RE::INPUT_DEVICE::kKeyboard && b->GetIDCode()<256)keyboardHeld[b->GetIDCode()]=b->IsPressed();
                    if(b->GetDevice()==RE::INPUT_DEVICE::kGamepad) {
                        const auto key=SKSE::InputMap::GamepadMaskToKeycode(b->GetIDCode());
                        const bool wasHeld=padChords.Held(static_cast<int>(key));
                        padChords.Observe(key,b->IsPressed());
                        if(wasHeld!=padChords.Held(static_cast<int>(key))) {
                            const auto config=Config();
                            if(static_cast<int>(key)==config.gamepadModifier || static_cast<int>(key)==config.gamepadActionModifier ||
                                (NativeGamepadModifiers() & ControllerKeyBit(static_cast<int>(key))))
                                SKSE::log::info("Controller modifier: raw={} key={} pressed={} nativeDown={} duration={} state=events-v1",
                                    b->GetIDCode(),key,b->IsPressed(),b->IsDown(),b->HeldDuration());
                        }
                    }
                }
                if(auto stick=event->AsThumbstickEvent();stick && IsOpen() &&
                    ControllerSelectionStick(AppliedGamepadMoveWhileOpen(),stick->IsLeft(),stick->IsRight())) {
                    std::lock_guard lock(viewMutex);padX=stick->xValue;padY=stick->yValue;
                    if(std::hypot(padX,padY)>.2f) {
                        view.gamepad=true;
                        if(!view.settingsOpen && !view.outfitDialog) {
                            AimStick(view.x,view.y,padX,padY);
                            ClampWheelPointer(view.x,view.y,Style(Config()).layout);
                        }
                    }
                }
            }
            shiftHeld[0]=keyboardHeld[42];shiftHeld[1]=keyboardHeld[54];
            InputDispatchChain<RE::InputEvent> dispatch(events?*events:nullptr);
            for (auto event = events ? *events : nullptr; event; event = event->next) {
                bool suppress=false;
                if (auto button = event->AsButtonEvent()) {
                    const auto code = button->GetIDCode();
                    const auto identity = (static_cast<std::uint64_t>(button->GetDevice()) << 32) | code;
                    const bool pressed = button->IsPressed();
                    const bool gamepad=button->GetDevice()==RE::INPUT_DEVICE::kGamepad;
                    const auto pad=gamepad?SKSE::InputMap::GamepadMaskToKeycode(code):0u;
                    const bool pageDown=gamepad && controllerPages.Observe(pad,pressed,button->IsDown());
                    const bool triggerDown=gamepad && controllerTriggers.Observe(pad,pressed,button->IsDown());
                    // Some controller event sources omit the native zero-duration
                    // down or emit release with zero duration. Do not retain the
                    // UI's swallowed state after an observed physical release.
                    const bool up = gamepad?!pressed:button->IsUp();
                    const bool down = gamepad && IsOpen() && (pad==266 || pad==267 || pad==280 || pad==281)?
                        (pad>=280?triggerDown:pageDown):button->IsDown();
                    bool nameRepeat=false;
                    if(button->GetDevice()==RE::INPUT_DEVICE::kKeyboard && code<256) {
                        if(up)nameRepeatAt[code]=0;
                        else if(down)nameRepeatAt[code]=.4f;
                        else if(pressed && nameRepeatAt[code]>0 && button->GetRuntimeData().heldDownSecs>=nameRepeatAt[code]) {
                            nameRepeat=true;nameRepeatAt[code]=button->GetRuntimeData().heldDownSecs+.05f;
                        }
                    }
                    const auto opening=ToggleKey(button,controllerSample);
                    const bool toggle = opening!=Opening::None;
                    // Native contextual actions can disable a physical entrance
                    // through custom control groups without disabling all menus.
                    // Yield its whole cycle unchanged while idle; open/closing
                    // wheels still own captured buttons until physical release.
                    const bool yieldEntry=!captureBatch && !inputGate.Swallowed(identity) &&
                        (toggle || ReplacedEntrance(button,controllerSample)) && !EntryControlsEnabled(button->GetDevice(),code);
                    if(down && (button->GetDevice()==RE::INPUT_DEVICE::kGamepad || button->GetDevice()==RE::INPUT_DEVICE::kKeyboard || button->GetDevice()==RE::INPUT_DEVICE::kMouse)) {
                        std::lock_guard lock(viewMutex);view.gamepad=pad!=0;
                    }
                    if (down && button->GetDevice() == RE::INPUT_DEVICE::kKeyboard && (code == 16 || toggle)) {
                        const auto reason = yieldEntry?"entry control group disabled":OpenBlockReason();
                        const auto player = RE::PlayerCharacter::GetSingleton();
                        const auto race = player ? player->GetRace() : nullptr;
                        SKSE::log::info("Favorites key: scan={} mapped={} event='{}' match={} open={} blocked='{}' race={:08X} racePlayable={}",
                            code, FavoritesScanCode(), button->GetUserEvent().c_str(), toggle, IsOpen(), reason ? reason : "none",
                            race ? race->GetFormID() : 0, race && race->GetPlayable());
                    }
                    if(down && pad && (toggle || pad==266 || static_cast<int>(pad)==EffectiveGamepadKey(Config()))) {
                        const auto reason=yieldEntry?"entry control group disabled":OpenBlockReason();
                        const auto config=Config();const int favorite=EffectiveGamepadKey(config);
                        const auto openingMask=OpeningControllerHeld(controllerSample);
                        auto held=[&](int key){return key<0 || (key!=favorite && (openingMask & ControllerKeyBit(key)));};
                        SKSE::log::info("Controller entrance: raw={} key={} mapped={} effective={} mode={} open={} blocked='{}' favoriteModifier={} favoriteHeld={} actionModifier={} actionHeld={} heldMask={:04X} nativeRequired={:04X} followNative={} engineAvailable={} engineMask={:04X} openingMask={:04X} event='{}' state=events+engine-v2 binding=native-chord-v1",
                            code,pad,FavoritesScanCode(RE::INPUT_DEVICE::kGamepad),favorite,
                            opening==Opening::Actions?"actions":opening==Opening::Favorites?"favorites":"none",IsOpen(),reason?reason:"none",
                            config.gamepadModifier,held(config.gamepadModifier),config.gamepadActionModifier,held(config.gamepadActionModifier),padChords.Mask(),NativeGamepadModifiers(),config.gamepadHotkey<0,
                            controllerSample.available,controllerSample.held,openingMask,button->GetUserEvent().c_str());
                    }
                    bool consume = captureBatch || inputGate.Swallowed(identity);
                    const auto current=Snapshot();
                    if(pageDown && current.open)SKSE::log::info("Controller page input: raw={} key={} nativeDown={} swallowed={} settings={} dialog={}",
                        code,pad,button->IsDown(),inputGate.Swallowed(identity),current.settingsOpen,current.outfitDialog);
                    const bool naming=current.open && (current.outfitDialog==1 || current.outfitDialog==2);
                    if(naming && nameRepeat && (code==14 || code==211 || code==203 || code==205) && !bridge.composing)NameKey(code,false);
                    const bool bridgeKey=naming && ((bridge.available && bridge.enabled) || simpleIME) && button->GetDevice()==RE::INPUT_DEVICE::kKeyboard &&
                        (code==bridge.hotkey || code==29 || code==157 || code==42 || code==54 || ((bridge.active || simpleIME) && code==57 && (GetAsyncKeyState(VK_CONTROL)&0x8000)));
                    if(up && current.sortDrag.held && button->GetDevice()==RE::INPUT_DEVICE::kMouse && code==0) {
                        EndSortDrag();consume=true; // Release is owned even though the press is swallowed by InputGate.
                    } else if(down && current.sortDrag.held &&
                        ((button->GetDevice()==RE::INPUT_DEVICE::kMouse && code==1) ||
                        (button->GetDevice()==RE::INPUT_DEVICE::kKeyboard && code==1))) {
                        std::lock_guard lock(viewMutex);CancelSortDrag();consume=true;
                    } else if(yieldEntry) {
                        // Leave the event for the contextual action's handler.
                    } else if(down && openQueued && button->GetDevice()==RE::INPUT_DEVICE::kKeyboard && (code==1 || code==15)) {
                        Cancel();
                    } else if(bridgeKey) {
                        // Keep toggle/modifier values intact for Text Bridge regardless of hook order.
                        consume=false;
                    } else if(down && button->GetDevice()==RE::INPUT_DEVICE::kKeyboard && EntryControlsEnabled(button->GetDevice(),code) &&
                        !current.sortingEdit && !(current.open && !current.functions && !current.vampireRestricted &&
                            !current.settingsOpen && !current.outfitDialog && !toggle &&
                            code!=static_cast<unsigned>(Config().switchKey) && SortingShortcut(code,HeldModifiers())>=0) && TryCycleShortcut(static_cast<int>(code),HeldModifiers(),toggle)) {
                        consume=true;
                    } else if(down && !current.open && button->GetDevice()==RE::INPUT_DEVICE::kKeyboard && HeldModifiers()==0 &&
                        ValidQuickSlot(QuickSlotFromKey(code,button->GetUserEvent().c_str())) && EntryControlsEnabled(button->GetDevice(),code) &&
                        TryCombatQuickSlot(QuickSlotFromKey(code,button->GetUserEvent().c_str()),toggle,HeldModifiers())) {
                        consume=true;
                    } else if(down && current.open && current.outfitDialog && !inputGate.Swallowed(identity)) {
                        if(button->GetDevice()==RE::INPUT_DEVICE::kKeyboard) {
                            if(current.cycleCaptureKey) {
                                if(code==1){std::lock_guard lock(viewMutex);view.cycleCaptureKey=false;}
                                else if(!ModifierBit(code)) {
                                    const int key=(code==14 || code==211)?-1:static_cast<int>(code),modifiers=key<0?0:HeldModifiers();
                                    if(CycleBindingConflict(key,modifiers))RE::SendHUDMessage::ShowHUDMessage(Tr(Config(),"cycleConflict").c_str());
                                    else if(Cycles::Bind(current.cycleGroup,key,modifiers)) {
                                        const auto binding=Cycles::Binding(current.cycleGroup);
                                        {std::lock_guard lock(viewMutex);view.cycleCaptureKey=false;view.cycleBinding=binding;}
                                        RefreshFunctions();
                                    }
                                }
                            }else if(naming && bridge.composing) { /* IME owns candidate editing/confirmation. */ }
                            else if(code==1 || code==15)EndOutfitDialog();
                            else if(code==28)OutfitAccept();
                            else if(naming)NameKey(code,!simpleIME && !bridge.active && !characterBatch);
                        } else if(button->GetDevice()==RE::INPUT_DEVICE::kMouse && code==0)OutfitClick();
                        else if(pad==277)EndOutfitDialog();
                        else if(pad==276)OutfitClick();
                        else if(pad==279 && !naming)OutfitAccept();
                    } else if (down && current.open && current.settingsOpen && !inputGate.Swallowed(identity)) {
                        if(current.capturingKey) {
                            const bool cancel=(button->GetDevice()==RE::INPUT_DEVICE::kKeyboard && code==1) || pad==277;
                            const bool capture=current.captureBinding==15?(pad>=266 && pad<=281):
                                (button->GetDevice()==RE::INPUT_DEVICE::kKeyboard && code<255 && !ModifierBit(code));
                            if(cancel || capture) {
                                if(!cancel) {
                                    auto config=Config();
                                    if(current.captureBinding==15)config.gamepadHotkey=static_cast<int>(pad);
                                    else if(current.captureBinding==11)config.switchKey=static_cast<int>(code);
                                    else if(current.captureBinding==13){config.actionHotkey=static_cast<int>(code);config.actionModifier=HeldModifiers();}
                                    else {config.hotkey=static_cast<int>(code);config.hotkeyModifier=HeldModifiers();}
                                    EditSettings(config);
                                }
                                std::lock_guard lock(viewMutex);view.capturingKey=false;
                            }
                        } else if(button->GetDevice()==RE::INPUT_DEVICE::kKeyboard) {
                            if(code==1 || code==15) LeaveSettings(false);
                        } else if(button->GetDevice()==RE::INPUT_DEVICE::kMouse && code<2) SettingsClick(code==1);
                        else if(pad==276 || pad==278)SettingsClick(pad==278);
                        else if(pad==277)LeaveSettings(false);
                        else if(pad==274 || pad==275){std::lock_guard lock(viewMutex);view.settingsTab=NextSettingsTab(view.settingsTab,pad==275?1:-1);}
                    } else if (down && current.open && !inputGate.Swallowed(identity) &&
                        ((button->GetDevice()==RE::INPUT_DEVICE::kKeyboard && code==60) || pad==270)) {
                        EnterSettings();
                    } else if(down && current.open && !current.functions && !current.vampireRestricted && !toggle &&
                        !inputGate.Swallowed(identity) &&
                        (pad==272 || pad==273 || (button->GetDevice()==RE::INPUT_DEVICE::kKeyboard &&
                            code!=static_cast<unsigned>(Config().switchKey) && SortingShortcut(code,HeldModifiers())>=0))) {
                        ChangeSorting(pad?pad==273:SortingShortcut(code,HeldModifiers())==1);
                    } else if(down && current.open && !inputGate.Swallowed(identity) &&
                        ((button->GetDevice()==RE::INPUT_DEVICE::kKeyboard && code==static_cast<unsigned>(Config().switchKey)) || pad==279)) {
                        SwitchWheel();
                    } else if (down && !inputGate.Swallowed(identity) && toggle && (!pad || !current.open)) {
                        if (IsOpen() || openQueued) { Cancel(true); consume = captureBatch = true; }
                        else if (closing || HasPendingAction() || Outfits::Busy() || Combat::Busy()) { consume = true;if(Outfits::Busy() || Combat::Busy())RE::SendHUDMessage::ShowHUDMessage(Tr(Config(),Combat::Busy()?"combatWorking":"outfitWorking").c_str()); }
                        else if (!OpenBlockReason()) { RequestOpen(opening==Opening::Actions,button->GetDevice(),code); consume = captureBatch = true; }
                    } else if (down && IsOpen() && !inputGate.Swallowed(identity)) {
                        if (button->GetDevice() == RE::INPUT_DEVICE::kKeyboard) {
                            const int quickSlot=QuickSlotFromKey(code,button->GetUserEvent().c_str());
                            if(HeldModifiers()==0 && quickSlot>=0)BindQuickSlot(quickSlot);
                            else switch (code) {
                            case 1: case 15: if(current.functions && (current.functionSection==FaceLight::Section::Followers || current.functionSection==FaceLight::Section::CycleMembers))FunctionBack();else Cancel(true); break; // Esc / Tab
                            case 30: case 203: ChangeCategory(-1); break; // A / left
                            case 32: case 205: ChangeCategory(1); break; // D / right
                            case 17: case 200: ChangePage(-1); break; // W / up
                            case 31: case 208: ChangePage(1); break; // S / down
                            case 28: Activate(false); break;
                            default: break;
                            }
                        } else if (button->GetDevice() == RE::INPUT_DEVICE::kMouse) {
                            if (code == 0 || code == 1) {
                                if(current.sortingEdit){if(code==0)BeginSortDrag();else Activate(true);}
                                else Activate(MouseUsesLeftHand(code,current.config.swapMouseHands,current.functions));
                            }
                            else if (code == 8) ChangePage(-1);
                            else if (code == 9) ChangePage(1);
                        } else if(pad) {
                            switch(ControllerCommand(pad,current.config.gamepadCategoryButtons)) {
                            case PadWheelCommand::UseRight:Activate(false);break;
                            case PadWheelCommand::UseLeft:Activate(true);break;
                            case PadWheelCommand::Back:if(current.functions && (current.functionSection==FaceLight::Section::Followers || current.functionSection==FaceLight::Section::CycleMembers))FunctionBack();else Cancel(true);break;
                            case PadWheelCommand::PreviousCategory:ChangeCategory(-1);break;
                            case PadWheelCommand::NextCategory:ChangeCategory(1);break;
                            case PadWheelCommand::PreviousPage:ChangePage(-1,"up");break;
                            case PadWheelCommand::NextPage:ChangePage(1,"down");break;
                            default:break;
                            }
                        }
                    } else if(down && !toggle && ReplacedEntrance(button,controllerSample) && !OpenBlockReason()) {
                        // A configured modifier is required for the replaced vanilla entrance.
                        // This only owns the mapped entrance, never number / Numpad quick slots.
                        consume=true;
                    }
                    const bool wheelImpulse = button->GetDevice() == RE::INPUT_DEVICE::kMouse && code >= 8;
                    // Identify the physical gameplay binding, including remaps.
                    // A wheel entrance takes priority even if bound to movement.
                    auto controls=RE::ControlMap::GetSingleton();
                    const auto device=button->GetDevice();
                    const bool movement=!toggle && !ReplacedEntrance(button,controllerSample) && controls &&
                        (device==RE::INPUT_DEVICE::kKeyboard || device==RE::INPUT_DEVICE::kMouse || device==RE::INPUT_DEVICE::kGamepad) &&
                        IsMovementEvent(controls->GetUserEventName(code,button->GetDevice()));
                    const auto decision = inputGate.Filter(identity, pressed, up,
                        movement?captureBatch:consume, wheelImpulse, movement);
                    if(down && button->GetDevice()==RE::INPUT_DEVICE::kKeyboard &&
                        ((code>=2 && code<=11) || code==82 || (code>=79 && code<=81)))
                        SKSE::log::info("Numeric shortcut input: scan={} event='{}' wheel={} queued={} closing={} filter={}",
                            code,button->GetUserEvent().c_str(),current.open,openQueued.load(),closing.load(),
                            decision==InputGate::Result::Pass?"pass":decision==InputGate::Result::Release?"release":
                            decision==InputGate::Result::Resume?"resume":"suppress");
                    if(down && (pad==274 || pad==275 || pad==280 || pad==281) && consume)
                        SKSE::log::info("Controller wheel input: raw={} key={} event='{}' filter={}",code,pad,
                            button->GetUserEvent().c_str(),decision==InputGate::Result::Suppress?"removed":
                            decision==InputGate::Result::Release?"release":"pass");
                    if(decision==InputGate::Result::Resume) {
                        // A held UI movement key needs a fresh down when gameplay
                        // resumes, rather than waiting for a physical re-press.
                        button->GetRuntimeData().heldDownSecs=0;
                    } else if (decision != InputGate::Result::Pass) {
                        // Release captured gameplay actions once; keep wheel
                        // clicks/entrances suppressed through physical release.
                        auto& data = button->GetRuntimeData();
                        data.value = 0;
                        data.heldDownSecs = decision == InputGate::Result::Release ? std::max(.001f, data.heldDownSecs) : 0;
                        suppress=decision==InputGate::Result::Suppress;
                    }
                } else if (auto mouse = event->AsMouseMoveEvent(); mouse && captureBatch) {
                    if (IsOpen()) {
                        std::lock_guard lock(viewMutex);
                        if(mouse->mouseInputX || mouse->mouseInputY){view.gamepad=false;padX=padY=0;}
                        const auto config=Config();
                        const float width=viewportWidth.load(),height=viewportHeight.load();
                        if(view.settingsOpen || view.outfitDialog) {
                            const float scale=LayoutScale(width,height,config.scale);
                            const float sensitivity=config.sensitivity/(224*scale);
                            view.x=std::clamp(view.x+mouse->mouseInputX*sensitivity,-width/(448*scale),width/(448*scale));
                            view.y=std::clamp(view.y+mouse->mouseInputY*sensitivity,-height/(448*scale),height/(448*scale));
                        } else {
                            MoveWheelPointer(view.x,view.y,mouse->mouseInputX*config.sensitivity/250.f,
                                mouse->mouseInputY*config.sensitivity/250.f,Style(config).layout);
                            view.sortDrag.Move(view.x,view.y);
                        }
                    }
                    mouse->mouseInputX = mouse->mouseInputY = 0;
                } else if(auto character=event->AsCharEvent();character && captureBatch) {
                    const auto current=Snapshot();
                    if(current.outfitDialog!=1 && current.outfitDialog!=2)character->keyCode=0;
                } else if (auto stick = event->AsThumbstickEvent(); stick) {
                    const auto config=Config();
                    const bool split=AppliedGamepadMoveWhileOpen();
                    if(stick->IsLeft()) {
                        bool open,modal;
                        {std::lock_guard lock(viewMutex);open=view.open;modal=view.settingsOpen || view.outfitDialog;}
                        if(split && captureBatch && config.timeMode!=0)
                            modal=modal || !Focused() || !ValidPlayer() || BlockedMenu();
                        const auto mode=ControllerLeftStick(split,captureBatch,
                            open,modal,config.timeMode,closing || openQueued);
                        movementStick.Capture(mode==LeftStickMode::HeldDirection);
                        if(mode==LeftStickMode::Block)stick->xValue=stick->yValue=0;
                        movementStick.Filter(stick->xValue,stick->yValue);
                    } else if(stick->IsRight()) {
                        lookStick.Filter(stick->xValue,stick->yValue,captureBatch,split);
                    } else if(captureBatch)stick->xValue=stick->yValue=0;
                }
                dispatch.Append(event,!suppress);
            }
            // Keep genuine gameplay releases and allowed movement/IME events.
            // Restore original queue links after every chained hook has returned;
            // injected character events are consumed during that same dispatch.
            dispatch.Finish();
            previousDispatch(source, dispatch.Events());
            dispatch.Restore();
            if (!Focused()) {
                inputGate.Reset();movementStick.Reset();lookStick.Reset(AppliedGamepadMoveWhileOpen());controllerPages.Reset();controllerTriggers.Reset();shiftHeld[0]=shiftHeld[1]=false;
                std::fill_n(keyboardHeld,256,false);padChords.Reset();
                std::lock_guard lock(viewMutex);padX=padY=0;
            }
            if(IsOpen())SyncWheelTime();
            PumpAction();
            {const auto current=Snapshot();if(current.open && !current.functions && !current.settingsOpen && !current.outfitDialog) {
                bool needed,refresh;
                {std::lock_guard lock(viewMutex);
                    needed=DirectoryRequestNeeded(inventoryLoaded,view.inventoryLoading,directoryTaskQueued.load());
                    refresh=inventoryLoaded;
                }
                if(needed)RequestDirectory(refresh);
            }}
            PumpLiveInventory();
            PumpDetails();
        }

        // SimpleIME delivers committed text through the native UI message queue.
        // CommonLib exposes GFxEvent but not the Scaleform character payload.
        struct ScaleformCharacter : RE::GFxEvent {
            std::uint32_t code;
            std::uint8_t keyboard;
        };
        static_assert(sizeof(ScaleformCharacter)==0x0C);
        bool ReceiveTextMessage(RE::UIMessage& message) {
            if(message.type!=RE::UI_MESSAGE_TYPE::kScaleformEvent || !message.data)return false;
            const auto data=static_cast<RE::BSUIScaleformData*>(message.data);
            const auto event=data->scaleformEvent;
            if(!event || (event->type.get()!=RE::GFxEvent::EventType::kCharEvent &&
                static_cast<std::uint32_t>(event->type.get())!=0x80000001u))return false;
            std::lock_guard lock(viewMutex);
            if(!view.open || (view.outfitDialog!=1 && view.outfitDialog!=2))return false;
            const auto character=static_cast<const ScaleformCharacter*>(event);
            // Zero is SimpleIME's channel probe, not input.
            if(character->code)view.outfitName.Insert(character->code);
            return true;
        }

        class WheelMenu final : public RE::IMenu {
        public:
            WheelMenu() {
                depthPriority = 3;
                // This is an input-gated overlay. Native menu mode resets held
                // gameplay movement; input ownership is handled in InputHook.
                menuFlags.set(Flag::kDisablePauseMenu,Flag::kRequiresUpdate,Flag::kCustomRendering);
            }
            static RE::IMenu* Create() { return new WheelMenu; }
            RE::UI_MESSAGE_RESULTS ProcessMessage(RE::UIMessage& message) override {
                std::lock_guard lifecycle(lifecycleMutex);
                if(ReceiveTextMessage(message))return RE::UI_MESSAGE_RESULTS::kHandled;
                if (message.type == RE::UI_MESSAGE_TYPE::kHide || message.type == RE::UI_MESSAGE_TYPE::kForceHide) {
                    ReleaseTextInput();
                    const auto old=Snapshot();
                    if(old.outfitDialog==1 || old.outfitDialog==2)TextBridge::Reset();
                    TimeControl::EndSession();RequestPause(false);
                    retainedAction=false;retainedKeepsPause=false;retainedSubmitted=false;
                    RevertSettings();
                    std::lock_guard lock(viewMutex);
                    if(view.open) view.animateClose=false;
                    view.open = false;view.sortingEdit=false;CancelSortDrag();view.outfitDialog=0;view.cycleDialog=view.cycleMemberDialog=view.cycleCaptureKey=false; view.settingsOpen=view.capturingKey=false; closing = false;
                    return RE::UI_MESSAGE_RESULTS::kHandled;
                }
                if (message.type == RE::UI_MESSAGE_TYPE::kShow) {
                    if(IsOpen())SyncWheelTime();
                    else if(auto q=RE::UIMessageQueue::GetSingleton())q->AddMessage(menuName,RE::UI_MESSAGE_TYPE::kHide,nullptr);
                    return RE::UI_MESSAGE_RESULTS::kHandled;
                }
                return RE::UI_MESSAGE_RESULTS::kPassOn;
            }
            void AdvanceMovie(float, std::uint32_t) override {SyncWheelTime();}
            void PostDisplay() override {RenderTextInputLayer();}
        };

        class MenuObserver final : public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
            RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* event,RE::BSTEventSource<RE::MenuOpenCloseEvent>*)override {
                if(event && event->opening && event->menuName!=menuName && event->menuName!=pauseMenuName && IsOpen()) {
                    auto ui=RE::UI::GetSingleton();auto menu=ui?ui->GetMenu(event->menuName.c_str()):nullptr;
                    if(BlockedMenu() || (menu && (menu->PausesGame() || menu->Modal() || menu->ApplicationMenu())))Cancel();
                }
                return RE::BSEventNotifyControl::kContinue;
            }
        };

        // A separate fixed-flag menu lets the engine balance its pause counters.
        // Never toggle kPausesGame on an existing menu or modify UI pause counts.
        class PauseGuardMenu final : public RE::IMenu {
        public:
            PauseGuardMenu(){depthPriority=2;menuFlags.set(Flag::kPausesGame,Flag::kDisablePauseMenu);}
            static RE::IMenu* Create(){return new PauseGuardMenu;}
            RE::UI_MESSAGE_RESULTS ProcessMessage(RE::UIMessage& message) override {
                std::lock_guard lifecycle(lifecycleMutex);
                if(ReceiveTextMessage(message))return RE::UI_MESSAGE_RESULTS::kHandled;
                if(message.type==RE::UI_MESSAGE_TYPE::kShow) {
                    bool wanted;{std::lock_guard lock(pauseMutex);wanted=pauseRequested;}
                    if(!wanted)if(auto q=RE::UIMessageQueue::GetSingleton())q->AddMessage(pauseMenuName,RE::UI_MESSAGE_TYPE::kHide,nullptr);
                }else if(message.type==RE::UI_MESSAGE_TYPE::kHide || message.type==RE::UI_MESSAGE_TYPE::kForceHide) {
                    bool wanted;{std::lock_guard lock(pauseMutex);wanted=pauseRequested;}
                    if(wanted && IsOpen()) {
                        if(message.type==RE::UI_MESSAGE_TYPE::kForceHide)Cancel();
                        else if(auto q=RE::UIMessageQueue::GetSingleton())q->AddMessage(pauseMenuName,RE::UI_MESSAGE_TYPE::kShow,nullptr);
                    }
                }
                return message.type==RE::UI_MESSAGE_TYPE::kScaleformEvent?RE::UI_MESSAGE_RESULTS::kPassOn:RE::UI_MESSAGE_RESULTS::kHandled;
            }
            void AdvanceMovie(float,std::uint32_t)override{}
            void PostDisplay()override{}
        };

        // Event callbacks only invalidate a stamp; scans are coalesced by the input pump.
        class InventoryObserver final : public RE::BSTEventSink<RE::TESContainerChangedEvent>,public RE::BSTEventSink<RE::TESEquipEvent> {
            RE::BSEventNotifyControl ProcessEvent(const RE::TESContainerChangedEvent* event,RE::BSTEventSource<RE::TESContainerChangedEvent>*)override {
                if(event && (event->oldContainer==0x14 || event->newContainer==0x14))++inventoryChanges;
                return RE::BSEventNotifyControl::kContinue;
            }
            RE::BSEventNotifyControl ProcessEvent(const RE::TESEquipEvent* event,RE::BSTEventSource<RE::TESEquipEvent>*)override {
                if(event && event->actor.get()==RE::PlayerCharacter::GetSingleton())++inventoryChanges;
                return RE::BSEventNotifyControl::kContinue;
            }
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
                    const bool naming=view.outfitDialog==1 || view.outfitDialog==2;
                    // With SimpleIME, let native characters reach its Scaleform filter.
                    // Reading them here leaks pinyin; clearing them breaks English with IME off.
                    if(naming && SimpleIME::Available())continue;
                    if(naming)view.outfitName.Insert(c->keyCode);
                    c->keyCode=0;
                }
                return RE::BSEventNotifyControl::kContinue;
            }
        };
    }

    View Snapshot() { std::lock_guard lock(viewMutex); auto copy=view; copy.config=Config();copy.appliedGamepadMove=AppliedGamepadMoveWhileOpen(); return copy; }
    void AdvanceGamepadPointer(float elapsed) {
        std::lock_guard lock(viewMutex);
        if(!view.open || !view.gamepad || (!view.settingsOpen && !view.outfitDialog))return;
        const auto config=Config();
        const float scale=LayoutScale(viewportWidth,viewportHeight,config.scale);
        const float speed=2.4f*config.sensitivity*std::clamp(elapsed,0.f,.05f);
        view.x=std::clamp(view.x+StickAxis(padX)*speed,-viewportWidth.load()/(448*scale),viewportWidth.load()/(448*scale));
        view.y=std::clamp(view.y-StickAxis(padY)*speed,-viewportHeight.load()/(448*scale),viewportHeight.load()/(448*scale));
    }
    void SetViewport(float width,float height) { if(width>0 && height>0) {viewportWidth=width; viewportHeight=height;} }
    bool IsOpen() { std::lock_guard lock(viewMutex); return view.open; }
    void Cancel(bool effects) {
        std::lock_guard lifecycle(lifecycleMutex);
        retainedAction=false;retainedKeepsPause=false;retainedSubmitted=false;
        {std::lock_guard lock(openMutex);++openSerial;openQueued=false;pendingOpen.reset();}
        const auto current=Snapshot();
        ReleaseTextInput();
        if(current.outfitDialog==1 || current.outfitDialog==2)TextBridge::Reset();
        RevertSettings();
        TimeControl::EndSession();RequestPause(false);
        bool wasOpen;
        { std::lock_guard lock(viewMutex); wasOpen = view.open;if(view.functions)RememberFunctionPage();else favoritePage=view.page; view.open = false;view.sortingEdit=false;CancelSortDrag();view.sortingSelectedSlot=-1;sortingSelection.reset();view.outfitDialog=0;view.combatDialog=view.cycleDialog=view.cycleMemberDialog=view.cycleCaptureKey=false; view.animateClose=effects; view.settingsOpen=view.capturingKey=false; }
        if (wasOpen) {
            if(effects && Config().sounds) RE::PlaySound("UIMenuCancel");
            closing = true;
            if (auto queue = RE::UIMessageQueue::GetSingleton()) queue->AddMessage(menuName, RE::UI_MESSAGE_TYPE::kHide, nullptr);
        }
    }
    void SetGameActive(bool active) {
        std::lock_guard lifecycle(lifecycleMutex);
        ++epoch;
        Outfits::CancelJob();
        Combat::CancelJob();
        FaceLight::Clear();
        { std::lock_guard lock(actionMutex); pendingAction.reset(); }
        if(active)InstallPlayerUpdate();
        gameSession.Notify(active);
        Cancel();
        favoritePage=functionPage=combatPage=cyclePage=cycleMemberPage=0;shiftHeld[0]=shiftHeld[1]=false;
        inputGate.Reset();movementStick.Reset();lookStick.Reset(AppliedGamepadMoveWhileOpen());controllerPages.Reset();controllerTriggers.Reset();std::fill_n(keyboardHeld,256,false);padChords.Reset();
        { std::lock_guard lock(viewMutex);openingMemory.Reset();++directoryRevision;inventoryLoaded=false;inventoryPages.Set({});functionItems.clear();
            view.items.clear();view.visibleCategories={0};view.totalItems=0;view.inventoryLoading=false;padX=padY=0; }
        SKSE::log::info("Game active={}", active);
        if (active) {
            const auto config=Config();const auto favorite=EffectiveKeyboardKey(config);
            SKSE::log::info("Input state after load: callbacks={} favoritesScan={} override={} effectiveFavorite={} favoriteModifiers={} effectiveAction={} actionModifiers={} gamepadMain={} gamepadModifier={} gamepadActionModifier={} controllerState=events+engine-v2",
                dispatchCount.load(),FavoritesScanCode(),config.hotkey,favorite,config.hotkeyModifier,
                config.actionHotkey<0?favorite:config.actionHotkey,config.actionModifier,
                EffectiveGamepadKey(config),config.gamepadModifier,config.gamepadActionModifier);
            if(config.hotkey<0 && favorite!=16)SKSE::log::info("Wheel follows the game's remapped Favorites key (scan={}); set Controls/Hotkey=16 to use physical Q",favorite);
        }
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
        ui->Register(menuName, WheelMenu::Create);
        ui->Register(pauseMenuName,PauseGuardMenu::Create);
        static MenuObserver menuObserver;
        ui->AddEventSink<RE::MenuOpenCloseEvent>(&menuObserver);
        if(auto events=RE::ScriptEventSourceHolder::GetSingleton()) {
            static InventoryObserver observer;
            events->AddEventSink<RE::TESContainerChangedEvent>(&observer);
            events->AddEventSink<RE::TESEquipEvent>(&observer);
        }
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

