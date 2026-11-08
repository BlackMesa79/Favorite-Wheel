#include "UIResources.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string_view>
#include <cmath>
#include <Windows.h>
#include <initializer_list>
#include <memory>
#include <mutex>

namespace Wheel {
    namespace {
        const std::unordered_map<std::string,std::string> english{
            {"swapMouseHands","Swap mouse hands"},
            {"swapMouseHandsHelp","On: LMB left hand, RMB right hand. Presets keep LMB apply / RMB manage."},
            {"compactUseSwapped","LMB USE / LEFT HAND  ·  RMB RIGHT HAND"},
            {"cycleTitle","COMBAT CYCLES"},{"cycleTab","CYCLES"},{"cycleEntries","CYCLE ENTRIES"},{"cycleDefault","Cycle group"},
            {"cycleManage","MANAGE CYCLE GROUP"},{"cycleMemberManage","MANAGE CYCLE ENTRY"},{"cycleCapture","ADD CURRENT COMBAT PRESET"},
            {"cycleUse","LMB NEXT SET · RMB MANAGE"},{"cycleMemberHint","LMB APPLY · RMB REORDER / REMOVE"},
            {"cycleMemberHelp","Change the order or remove this entry. The combat preset itself is kept."},
            {"cycleStorage","Save your game to retain groups, bindings and cycle positions. Entries use combat presets with slot modes."},
            {"cycleFull","This cycle group is full."},{"cycleDuplicate","This preset is already in the cycle group."},
            {"cycleAdded","Added to cycle"},{"cycleEmpty","This cycle group is empty."},{"cycleUnavailable","No usable preset in this cycle group."},
            {"cycleInvalidKey","Use a main key with optional Shift/Ctrl/Alt. Bare 1-8 and menu keys are reserved."},
            {"cycleConflict","This combination conflicts with a wheel binding or another cycle group."},{"cycleBound","Cycle shortcut updated."},
            {"cycleBind","BIND SHORTCUT"},{"cycleClear","CLEAR GROUP"},{"cycleEdit","EDIT ENTRIES"},{"cycleRemove","REMOVE FROM GROUP"},
            {"cycleMoveUp","MOVE EARLIER"},{"cycleMoveDown","MOVE LATER"},{"cycleCaptureKey","PRESS A COMBINATION..."},
            {"cycleCaptureHint","Press the main key with optional Shift/Ctrl/Alt. Esc cancels; Backspace/Delete disables the shortcut. Left/right modifiers are equivalent."},
            {"cycleConfirmClear","Clear this group? Its combat presets will be kept."},{"cycleConfirmRemove","Remove this entry from the group? Its combat preset will be kept."},
            {"cycleSets","sets"},{"cycleQuickHint","CYCLE SHORTCUT: ADD CURRENT COMBAT PRESET / HOVERED PRESET"},
            {"combatTitle","COMBAT PRESETS"},{"combatTab","COMBAT"},{"combatSave","SAVE COMBAT PRESET"},{"combatDefault","Combat preset"},
            {"combatManage","MANAGE COMBAT PRESET"},{"combatConfirmOverwrite","Replace this combat preset with your current hands and Voice?"},
            {"combatOverwrite","REPLACE WITH CURRENT EQUIPMENT"},
            {"combatStorageHint","Presets are saved with your game. Slots can be set, ignored, or unequipped; Voice accepts shouts and powers."},
            {"combatUse","LMB APPLY · RMB MANAGE"},{"combatLeft","Left hand"},{"combatRight","Right hand"},{"combatVoice","Voice"},{"combatBoth","Two-handed"},
            {"combatIgnore","Ignore"},{"combatUnequip","Unequip"},{"combatSet","Set"},{"combatSlotUnavailable","No saved item: Set is skipped. Replace the preset to capture equipment again."},
            {"combatEmptyHand","Empty"},{"combatMissing","Item missing"},{"combatChanged","Item changed"},
            {"combatAmbiguous","Ambiguous item instance"},{"combatNotEnough","Not enough matching copies"},
            {"combatSpellMissing","Spell unavailable or not learned"},{"combatUnsupported","Current equipment or Voice cannot be captured/restored."},
            {"combatUnavailable","UNAVAILABLE"},{"combatCaptureFailed","Unable to capture the current combat preset."},
            {"combatLimit","Combat preset limit reached (100)."},{"combatSaved","Combat preset saved. Save your game to retain it."},
            {"combatApplied","Combat preset equipped."},{"combatWorking","Applying combat preset..."},
            {"combatBlocked","Current equipment or game state blocks this preset."},
            {"combatInterrupted","Combat preset interrupted. Check your equipment and Voice."},
            {"combatPartial","Combat preset could not finish. Check your equipment, Voice and the log."},
            {"infoWeight","Weight"},{"infoValue","Value"},{"infoDamage","Damage"},{"infoBaseDamage","Base damage"},
            {"infoArmor","Armor"},{"infoMagicka","Magicka"},{"infoPerSecond","/s"},
            {"infoEffects","BASE EFFECTS"},{"infoEnchantment","BASE ENCHANTMENT EFFECTS"},
            {"infoMagnitude","Magnitude"},{"infoSeconds","s"},{"infoArea","Area"},{"infoFeet","ft"},
            {"infoUnnamedEffect","Unnamed effect"},{"infoMoreEffects","more effects"},
            {"lightUse","LMB TOGGLE PREFERENCE / OPEN LIST"},
            {"lightNavigation","SWITCH WHEEL · ESC BACK · F2 SETTINGS"},
            {"lightUnavailableShort","API UNAVAILABLE"},
            {"lightWrongThread","Face Lighting rejected the calling thread. See FavoriteWheel.log."},
            {"languageAuto","System"},

            {"functionTitle","FUNCTION WHEEL"},
            {"functionCount","ACTIONS"},
            {"functionUse","LMB USE / OPEN · OUTFIT RMB MANAGE"},
            {"lightMenu","FACE LIGHTING"},
            {"lightTitle","FACE LIGHTING"},
            {"lightPlayer","PLAYER LIGHT"},
            {"lightTarget","TARGET LIGHT"},
            {"lightFollowers","FOLLOWER LIGHTS"},
            {"lightFollowerGroup","FOLLOWER GROUP"},
            {"lightBack","BACK"},
            {"lightOpen","CLICK TO OPEN"},
            {"lightOn","PREFERENCE: ON"},
            {"lightOff","PREFERENCE: OFF"},
            {"lightPeople","ACTORS"},
            {"lightUnavailable","Face Lighting API unavailable. Check the installed version."},
            {"lightNotReady","Face Lighting is not ready. Resume gameplay and reopen."},
            {"lightNoTarget","Aim at an NPC before opening the wheel."},
            {"lightUnnamed","Unnamed actor"},
            {"lightNoFollowers","NO FOLLOWERS"},
            {"lightGroupOff","SOURCE GROUP OFF"},
            {"lightUnloaded","UNLOADED"},
            {"lightUnsafe","UNSAFE NOW"},
            {"lightSneak","HIDDEN: SNEAK"},
            {"lightView","HIDDEN: VIEW"},
            {"lightAmbient","HIDDEN: AMBIENT"},
            {"lightDialogueHidden","HIDDEN: DIALOGUE"},
            {"lightDialogue","DIALOGUE SOURCE"},
            {"lightModel","MISSING NODE"},
            {"lightWaiting","NOT ALLOCATED"},
            {"lightPreferenceHint","Toggle preference; runtime rules still apply."},
            {"lightActorSave","Save the game to keep personal preferences"},
            {"lightGroupHint","Change the source group; keep individual preferences."},
            {"lightNotFollower","No longer a teammate. Reopen the wheel."},
            {"lightSubmitted","Light preference submitted; scene rules still apply."},
            {"lightNoChange","Light preference already matches."},
            {"lightStale","State changed. Reopen the wheel and choose again."},
            {"lightTargetInvalid","Target invalid or unloaded; no light preference changed."},
            {"lightSourceDisabled","Actor source group is off. Enable that source first."},
            {"lightListFull","Face Lighting list is full; preference unchanged."},
            {"lightSaveFailed","Could not save Face Lighting settings. Check the log."},
            {"lightBusyPreview","Close the Face Lighting configuration preview first."},
            {"lightBlocked","The current game state blocks light changes."},
            {"lightError","Face Lighting operation failed. Check the log."},

            {"outfitWorking","Changing outfit, please wait..."},{"outfitInterrupted","Outfit interrupted by a game state change. Check your equipment."},
            {"controlsBindHint","Modifiers match exactly. Right-click / X resets a main key."},{"bindingConflict","Matching shortcuts: actions take priority."},
            {"keyboardControlsHelp","Configure independent opening keys and modifiers for both wheels."},{"controlsCaptureHint","Click a main key to bind; right-click / X restores its default."},{"appearanceHint","Changes preview immediately. Apply saves; cancel restores settings."},{"padSelect","A SELECT / USE  ·  X LEFT HAND"},
            {"quickSlotHint","HOVER + 1-8 BIND / UNBIND QUICK SLOT"},
            {"settingsGameplay","GAMEPLAY"},{"gameplaySettingsHelp","Choose the item source and wheel time behavior."},
            {"timeMode","Time behavior"},{"timePause","Pause"},{"timeSlow","Slow time"},{"timeNormal","Normal speed"},
            {"slowTimePercent","Slow-time speed"},{"timeSettingsHint","Settings and outfit dialogs always pause. Apply saves your choices."},
            {"slowTimeHelp","Slow time scales the existing game speed without lowering it below 50%. Existing slower effects are preserved. The world keeps moving and the player can still take damage. UI animations keep their normal speed."},
            {"timeSettingsPhysicsHint","Slow time: 50-100% (physics guard). Settings pause; Apply saves."},
            {"inventoryScope","Item source"},{"scopeFavorites","Favorites only"},{"scopeAll","All inventory"},
            {"settingsKeyboard","KEYBOARD"},{"settingsController","CONTROLLER"},
            {"consumableRetentionHelp","Potions and food use a separate switch. Poisons and presets close the wheel; consumption briefly releases pause."},{"keepOpenConsumables","Keep open after potions / food"},{"keepOpen","Keep open after equip"},{"gamepadCategoryButtons","Category buttons"},
            {"gamepadMoveWhileOpen","Left stick movement"},
            {"kbMoveWhileOpen","KB+Mouse Move While Open"},{"kbCategoryPrevKey","Category Prev Key (Z)"},{"kbCategoryNextKey","Category Next Key (C)"},
            {"kbMoveNavHint","Scroll: page  ·  Middle: category  ·  Z: previous  ·  C: next"},
            {"hideEmptyCategories","Hide empty categories"},
            {"gamepadMoveApplyHint","Stick controls change only after Apply; Cancel keeps the current scheme."},
            {"gamepadMoveHelp","On: left stick moves in Slow/Normal; right stick selects or moves dialog pointers. Pause and dialogs stop movement. Sprint buttons remain wheel controls."},
            {"padSettingsHelpRight","Right stick moves pointer; A selects; X resets; B cancels; LB/RB tabs."},
            {"padNavigationMoving","LS MOVE  ·  RS SELECT  ·  Y WHEEL  ·  B CLOSE  ·  START SETTINGS"},
            {"padNavigationRight","RS SELECT  ·  Y WHEEL  ·  B CLOSE  ·  START SETTINGS"},
            {"controllerControlsHelp","Left stick moves pointer; A selects; X resets; B cancels; LB/RB tabs."},
            {"controllerCaptureHint","Main key: select to bind; X resets. Modifiers: cycle with - / +."},
            {"controllerSchemeHelp","Choose LB/RB or LT/RT to change categories. The other pair equips left/right; A/X also use/manage. D-Pad Up/Down changes item pages."},
            {"keepOpenHelp","Equipment, spells, powers, shouts and other actions stay open. Potions, food and outfit presets close the wheel. Pause briefly releases for equipment changes, then resumes. Slow/normal speed is preserved."},
            {"padUseTriggers","RT / A USE  ·  LT / X LEFT / MANAGE"},
            {"padUseBumpers","RB / A USE  ·  LB / X LEFT / MANAGE"},
            {"inventoryEmpty","NO ITEMS IN THIS CATEGORY"},{"inventoryLoading","LOADING ITEMS"},{"inventoryCount","ITEMS"},
            {"allQuickSlotHint","HOVER FAVORITE + 1-8 BIND / UNBIND"},
            {"quickSlotNeedsFavorite","Favorite this item in the inventory before assigning a quick slot."},
            {"combatQuickBound","Combat quick slot assigned. The previous item/preset binding is cleared. Save your game."},
            {"combatQuickUnbound","Combat quick slot removed. Save your game to keep this change."},
            {"settingsGeneral","APPEARANCE"},{"settingsControls","CONTROLS"},{"favoriteModifier","Favorites modifier"},{"actionHotkey","Actions key"},{"actionModifier","Actions modifier"},{"gamepadHotkey","Controller favorites key"},{"gamepadModifier","Controller favorites modifier"},{"gamepadActionModifier","Controller actions modifier"},{"modifierNone","None"},{"followFavorite","Follow favorites key"},{"captureChord","Hold modifiers and press a key; Esc cancels."},{"capturePad","Press a controller button; B / Esc cancels."},{"padUse","A USE / RIGHT HAND  ·  X LEFT HAND / MANAGE"},{"padNavigation","Y SWITCH WHEEL  ·  B BACK / CLOSE  ·  START SETTINGS"},{"padSettingsHelp","Left stick moves pointer; A selects; X resets; B cancels; LB/RB tabs."},
            {"wheelBindHint","Click to bind; right-click to reset (Favorites: game / switch: R)"},{"switchWheelKey","Switch wheel key"},
            {"outfitWheelTitle","FUNCTIONS · OUTFITS"},
            {"outfitTitle","OUTFIT PRESETS"},
            {"outfitCategory","OUTFITS"},
            {"outfitCount","PRESETS"},
            {"outfitSave","SAVE CURRENT"},
            {"outfitImport","IMPORT OUTFITS"},
            {"outfitDefault","New outfit"},
            {"outfitManage","MANAGE OUTFIT"},
            {"outfitNameHelp","Type a name · Ctrl+A clear · Ctrl+V paste · Enter confirm"},
            {"outfitNameEditHelp","Ctrl+A select all · Ctrl+V paste · Enter confirm"},
            {"outfitManageHelp","Click name to rename. Overwrite and delete require confirmation."},
            {"outfitNameEmpty","Enter a name"},
            {"outfitOverwrite","REPLACE WITH CURRENT"},
            {"outfitDelete","DELETE PRESET"},
            {"outfitExport","EXPORT PRESET"},
            {"outfitConfirmOverwrite","Replace this preset with your current outfit?"},
            {"outfitConfirmDelete","Delete this preset? Inventory items remain untouched."},
            {"outfitStorageHint","Presets are saved with your game. Weapons and shields are excluded."},
            {"outfitBack","BACK"},
            {"outfitConfirm","CONFIRM"},
            {"outfitUnavailable","MISSING / AMBIGUOUS"},
            {"outfitSelect","LMB USE · RMB MANAGE"},
            {"outfitNavigation","W / S / SCROLL PAGE"},
            {"outfitUse","LMB EQUIP / REMOVE · RMB MANAGE"},
            {"dualWheelHint","SWITCH WHEEL · ESC CLOSE · F2 SETTINGS"},
            {"sortSave","Order is per character and category; save your game."},
            {"sortEditing","ARRANGE"},
            {"sortButton","Sort"},
            {"sortArrange","Arrange"},
            {"sortEarlier","Move earlier"},
            {"sortLater","Move later"},
            {"sortDone","Done"},
            {"sortDefault","Default"},
            {"sortAsc","Name A-Z"},
            {"sortDesc","Name Z-A"},
            {"sortEquipped","Equipped first"},
            {"sortCustom","Custom"},
            {"sortType","Type"},
            {"sortWeaponType","Weapon type"},
            {"sortArmorType","Armor class"},
            {"sortSchool","Magic school"},
            {"sortSlot","Equipment slot"},
            {"rememberLastWheel","Remember last wheel"},
            {"rememberLastWheelHelp","Normal entrance remembers the last wheel this session. The actions shortcut stays direct. Loading a save resets the remembered wheel."},
            {"sortStatDesc","Stat: high to low"},
            {"sortStatAsc","Stat: low to high"},
            {"sortDamageDesc","Damage: high to low"},
            {"sortDamageAsc","Damage: low to high"},
            {"sortArmorDesc","Armor: high to low"},
            {"sortArmorAsc","Armor: low to high"},
            {"sortCostDesc","Magicka cost: high to low"},
            {"sortCostAsc","Magicka cost: low to high"},
            {"sortWeight","Weight: low to high"},
            {"sortValue","Value: high to low"},
            {"sortDragging","Release: move to target | Scroll: page | RMB / Esc: cancel"},
            {"sortKeys","T: Cycle sort | Shift+T: Arrange | F2: Settings"},
            {"sortPad","LS click: Sort | RS click: Arrange | Save game to keep order"},
            {"sortMove","Click: earlier / later | Hold LMB + drag: move | Shift+T: Done"},
            {"sortMovePad","A earlier | X later | RS click done"},
            {"vampireUse","SELECT SKILL · USE ITS NATIVE EQUIP SLOT"},
            {"vampireRestricted","VAMPIRE LORD · SKILLS ONLY"},
            {"outfitEmpty","No eligible armor is worn. Weapons and shields are excluded."},
            {"outfitLimit","The limit is 100 presets."},
            {"outfitSaveInvalid","Outfit not saved: equipment data failed validation. See FavoriteWheel.log."},
            {"outfitSaved","Outfit recorded. Save your game to keep it."},
            {"outfitNotPortable","Dynamic items or enchantments cannot be exported across characters."},
            {"outfitExported","Exported to FavoriteWheel/Outfits/Exports."},
            {"outfitFileError","Could not write outfit file. See the log."},
            {"outfitNoImport","No .fwo preset files in the Imports folder."},
            {"outfitImported","Outfits imported. Save your game to keep them."},
            {"outfitImportFailed","Import canceled: invalid file, missing items or ambiguous instances. See log."},
            {"outfitMissing","Missing or ambiguous outfit items; no equipment changed."},
            {"outfitItemMissing","Not in inventory"},
            {"outfitItemChanged","Item instance changed"},
            {"outfitItemAmbiguous","Ambiguous item instance"},
            {"outfitItemDuplicate","Duplicate outfit entry"},
            {"outfitCaptureMismatch","Cannot identify the worn outfit reliably; preset not saved. See log."},
            {"outfitBlocked","Outfit canceled: protected equipment or a shield slot conflict."},
            {"outfitChanged","Equipment changed; outfit operation stopped. Reopen the wheel."},
            {"outfitPartial","Outfit change was incomplete. Check your equipment and log."},
            {"outfitRemoved","Armor, clothing and jewelry removed. Weapons and shields retained."},
            {"outfitApplied","Outfit equipped."},
            {"outfitEditTitle","Edit Outfit"},
            {"outfitEditDraft","Preset pieces"},
            {"outfitEditInventory","Inventory apparel"},
            {"outfitEditAll","All"},
            {"outfitEditHead","Head"},
            {"outfitEditBody","Body"},
            {"outfitEditHands","Hands"},
            {"outfitEditFeet","Feet"},
            {"outfitEditJewelry","Jewelry"},
            {"outfitEditOther","Other"},
            {"outfitEditRemove","Remove"},
            {"outfitEditAdd","Add →"},
            {"outfitEditSave","Save (F5 / Y)"},
            {"outfitEditWorn","Worn"},
            {"outfitEditReplaced","Replaced:"},
            {"outfitEditCountError","Save requires 1–64 valid pieces."},
            {"outfitEditDuplicate","This instance is already in the draft."},
            {"outfitEditLoadError","Could not load outfit. Cancel and reopen."},
            {"outfitEditSaveError","Save failed or preset changed. Cancel and reopen."},
            {"outfitEditLoading","Loading…"},
            {"outfitEditSaving","Saving…"},
            {"outfitEditEmpty","No pieces"},
            {"outfitEditKeyHelp","↑↓ Select · ←→ Column · Enter Edit · F5 Save · Esc Cancel"},
            {"outfitEditPadHelp","D-pad Select / Column · A Edit · Y Save · B Cancel"},
            {"outfitEditMouseHelp","Click select · Double-click edit · Scroll list · Q/E or LB/RB filter"},
            {"frameworkApply","Apply & Save"},
            {"frameworkDraftHelp","Switching pages keeps your edits. Apply and Cancel affect all pages; Restore Defaults affects this page only."},
            {"frameworkPending","You have unapplied changes."},
            {"frameworkHelp","Changes stay in this draft until Apply. Cancel reloads current settings."},{"frameworkCloseWheel","Close the wheel before editing settings here."},{"frameworkSaved","Settings applied and saved."},{"frameworkSaveFailed","Save failed or settings are being edited elsewhere. Cancel to reload; see the log if saving still fails."},{"frameworkConflict","Settings changed elsewhere. Cancel to reload before applying."},
            {"title","F A V O R I T E S"},{"weapons","WEAPONS"},{"armor","APPAREL"},{"potions","POTIONS"},{"poisons","POISONS"},{"food","FOOD"},{"magic","MAGIC"},{"spells","SPELLS"},{"shouts","SHOUTS"},{"powers","POWERS"},{"other","OTHER"},{"books","BOOKS"},
            {"equipped","EQUIPPED"},{"equippedLeft","EQUIPPED · LEFT HAND"},{"equippedRight","EQUIPPED · RIGHT HAND"},
            {"equippedDual","EQUIPPED · BOTH HANDS"},{"equippedTwoHanded","EQUIPPED · TWO-HANDED"},
            {"select","CLICK TO SELECT"},{"inventory","USE IN INVENTORY"},{"empty","NO FAVORITES"},{"move","MOVE TO SELECT"},{"count","FAVORITES"},
            {"categoriesHint","A / D  CATEGORY     W / S OR SCROLL  PAGE"},{"useHint","LMB USE / RIGHT HAND    RMB LEFT HAND    ESC CLOSE"},
            {"compactNavigation","A / D CATEGORY  ·  W / S / SCROLL PAGE"},{"compactUse","LMB USE / RIGHT HAND  ·  RMB LEFT HAND"},{"compactClose","ESC CLOSE  ·  F2 SETTINGS"},
            {"wheelSize","Wheel size"},{"positionX","Horizontal position X"},{"positionY","Vertical position Y"},{"overlayOpacity","Background dimming"},{"sounds","Open / close sounds"},{"animations","Open / close animation"},
            {"layoutSettingsHelp","Apply and return to check layout; Esc cancels changes."},{"settings","SETTINGS"},{"settingsTitle","WHEEL SETTINGS"},{"settingsHelp","Changes preview immediately. Apply to save; Esc cancels."},
            {"scale","Interface size"},{"sensitivity","Mouse sensitivity"},{"dim","Background darkness"},{"hints","Control hints"},{"actionNotifications","Equipment and cycle notifications"},{"language","Language"},{"theme","Theme"},{"hotkey","Favorites key"},
            {"on","ON"},{"off","OFF"},{"follow","Follow game"},{"capture","Press a key... Esc cancels"},{"bindHint","Click to bind; right-click to follow game"},
            {"apply","APPLY"},{"cancel","CANCEL"},{"defaults","DEFAULTS"},{"saveError","Could not save settings. Check folder access."},
            {"changed","Item changed. Reopen the wheel."},{"unsupported","Use this item in the inventory for now."}
        };
        std::vector<Language> languages{{"en","English","",english}};
        std::vector<Theme> themes{Theme{}};
        std::string systemLanguage="en";
        std::string resourceLoadDiagnostic;
        struct LanguageCache {std::string requested,active;};
        struct GlyphCache {std::string active;int hotkey,switchKey,actionHotkey;std::string text;};
        std::mutex cacheMutex; // Leaf lock: only exchange immutable snapshots while held.
        std::shared_ptr<const LanguageCache> languageCache;
        std::shared_ptr<const GlyphCache> glyphCache;
        std::vector<std::string> fallbackFonts;
        std::filesystem::path windowsFonts;
        std::string InstalledFont(const char* filename) {
            if(windowsFonts.empty())return {};
            const auto path=windowsFonts/filename;
            std::error_code error;
            if(!std::filesystem::is_regular_file(path,error))return {};
            const auto utf8=path.u8string();return {utf8.begin(),utf8.end()};
        }
        std::string AutomaticFont(const std::string& code) {
            const auto choose=[](std::initializer_list<const char*> names) {
                for(const auto name:names)if(auto path=InstalledFont(name);!path.empty())return path;
                return std::string{};
            };
            if(code=="ja" || code.starts_with("ja-"))return choose({"YuGothM.ttc","meiryo.ttc","msgothic.ttc"});
            if(code=="ko" || code.starts_with("ko-"))return choose({"malgun.ttf","gulim.ttc"});
            if(code=="zh" || code.starts_with("zh-"))return choose({"msyh.ttc","msjh.ttc","simsun.ttc"});
            return choose({"segoeui.ttf","arial.ttf"});
        }
        std::string Trim(std::string value) {
            const auto first=value.find_first_not_of(" \t\r\n");
            return first==std::string::npos ? "" : value.substr(first,value.find_last_not_of(" \t\r\n")-first+1);
        }
        std::string NormalizeLanguage(std::string value) {
            value=Trim(std::move(value));
            if(value.empty() || value.size()>63)return {};
            bool separator=true;
            for(auto& c:value) {
                if(c=='_' || c=='-') {
                    if(separator)return {};
                    c='-';separator=true;
                } else if((c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9')) {
                    if(c>='A' && c<='Z')c=static_cast<char>(c-'A'+'a');
                    separator=false;
                } else return {};
            }
            return separator?std::string{}:value;
        }
        std::string DetectSystemLanguage() {
            wchar_t name[LOCALE_NAME_MAX_LENGTH]{};
            if(!LCIDToLocaleName(MAKELCID(GetUserDefaultUILanguage(),SORT_DEFAULT),name,LOCALE_NAME_MAX_LENGTH,0))return "en";
            std::string code;
            for(const auto c:std::wstring_view(name)) {
                if(c>127)return "en";
                code+=static_cast<char>(c);
            }
            code=NormalizeLanguage(code);
            return code.empty()?"en":code;
        }
        std::unordered_map<std::string,std::string> Read(const std::filesystem::path& file) {
            std::unordered_map<std::string,std::string> values;
            std::error_code error;
            const auto size=std::filesystem::file_size(file,error);
            if(error || size>256*1024)return values;
            std::ifstream input(file,std::ios::binary);
            const std::string data((std::istreambuf_iterator<char>(input)),{});
            if(input.bad() || data.empty() || data.size()>256*1024 || data.find('\0')!=std::string::npos ||
                !MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,data.data(),static_cast<int>(data.size()),nullptr,0))return values;
            std::istringstream stream(data); std::string line;
            while (std::getline(stream,line)) {
                if (line.starts_with("\xEF\xBB\xBF")) line.erase(0,3);
                line=Trim(line);
                if (line.empty() || line[0]==';' || line[0]=='#' || line[0]=='[') continue;
                const auto equal=line.find('=');
                if (equal!=std::string::npos) values[Trim(line.substr(0,equal))]=Trim(line.substr(equal+1));
            }
            return values;
        }
        std::vector<std::filesystem::path> Files(const std::filesystem::path& dir) {
            std::vector<std::filesystem::path> result; std::error_code error;
            for (std::filesystem::directory_iterator it(dir,error),end; !error && it!=end; it.increment(error)) {
                std::error_code statusError;
                if (it->is_regular_file(statusError) && !statusError && it->path().extension()==".ini") result.push_back(it->path());
            }
            std::sort(result.begin(),result.end()); return result;
        }
        std::uint32_t Color(const std::string& value,std::uint32_t fallback) {
            if (value.size()!=8 || value.find_first_not_of("0123456789ABCDEFabcdef")!=std::string::npos) return fallback;
            const auto n=std::stoul(value,nullptr,16);
            return ((n>>24)&255)|((n>>8)&0xFF00)|((n<<8)&0xFF0000)|((n<<24)&0xFF000000);
        }
        float Number(const std::unordered_map<std::string,std::string>& values,const char* key,float fallback,float low,float high) {
            const auto it=values.find(key);if(it==values.end())return fallback;
            try {
                std::size_t consumed=0;const float n=std::stof(it->second,&consumed);
                return consumed==it->second.size() && std::isfinite(n)?std::clamp(n,low,high):fallback;
            } catch(...) {return fallback;}
        }
        template<class T> std::string Cycle(const std::vector<T>& list,const std::string& id,int delta) {
            int index=0;
            for (int i=0;i<static_cast<int>(list.size());++i) if (list[i].id==id) index=i;
            const int count=static_cast<int>(list.size()); return list[(index+delta+count)%count].id;
        }
    }
    void LoadResources(const std::string& root) {
        resourceLoadDiagnostic.clear();
        try {
            std::shared_ptr<const LanguageCache> oldLanguage;
            std::shared_ptr<const GlyphCache> oldGlyphs;
            {
                std::lock_guard lock(cacheMutex);
                oldLanguage=std::move(languageCache);oldGlyphs=std::move(glyphCache);
            }
            languages={{"en","English","",english}}; themes={Theme{}};
            auto loadedLanguages=languages;auto loadedThemes=themes;
            systemLanguage=DetectSystemLanguage(); // Once at startup; rendering never queries Windows language settings.
            wchar_t windows[MAX_PATH]{};const auto length=GetWindowsDirectoryW(windows,MAX_PATH);
            windowsFonts=length && length<MAX_PATH?std::filesystem::path(windows)/L"Fonts":std::filesystem::path{};
            fallbackFonts.clear();
            for(const auto name:{"segoeui.ttf","msyh.ttc","malgun.ttf","YuGothM.ttc","meiryo.ttc","msgothic.ttc","gulim.ttc","msjh.ttc"})
                if(auto path=InstalledFont(name);!path.empty())fallbackFonts.push_back(std::move(path));
            for (const auto& path:Files(std::filesystem::u8path(root)/"Languages")) {
                auto values=Read(path); const auto stem=path.stem().u8string();
                const std::string id(stem.begin(),stem.end());const auto code=NormalizeLanguage(id);
                if(code.empty() || code=="auto" || values.empty())continue;
                const auto name=values.contains("Name")&&!values["Name"].empty()?values["Name"]:id;
                Language entry{id,name,values["Font"],std::move(values)};
                if(entry.font=="auto")entry.font=AutomaticFont(code);
                if(code=="en") {entry.id="en";loadedLanguages[0]=std::move(entry);}
                else if(std::none_of(loadedLanguages.begin(),loadedLanguages.end(),[&](const auto& language){return NormalizeLanguage(language.id)==code;}))
                    loadedLanguages.push_back(std::move(entry));
            }
            for (const auto& path:Files(std::filesystem::u8path(root)/"Themes")) {
                auto values=Read(path); Theme entry;
                const auto stem=path.stem().u8string();entry.id.assign(stem.begin(),stem.end()); entry.name=values.contains("Name")?values["Name"]:entry.id; entry.font=values["Font"];
                const auto pack=values["IconPack"];
                if(!pack.empty() && pack.size()<=64 && pack.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")==std::string::npos) {
                    const auto manifest=(std::filesystem::u8path(root)/"IconPacks"/pack/"icons.ini").generic_u8string();
                    entry.iconPack.assign(manifest.begin(),manifest.end());
                }
                auto style=Trim(values["Style"]);
                std::transform(style.begin(),style.end(),style.begin(),[](unsigned char c){return c>='A' && c<='Z'?c+'a'-'A':c;});
                entry.style=style=="skyrim"?ThemeStyle::Skyrim:style=="classic"?ThemeStyle::ClassicGold:ThemeStyle::Etched;
                auto frame=Trim(values["FrameStyle"]);
                std::transform(frame.begin(),frame.end(),frame.begin(),[](unsigned char c){return c>='A' && c<='Z'?c+'a'-'A':c;});
                entry.frameStyle=frame=="nordic"?FrameStyle::Nordic:FrameStyle::Plain;
                const auto token=[&](const char* key) {
                    auto value=Trim(values[key]);
                    std::transform(value.begin(),value.end(),value.begin(),[](unsigned char c){return c>='A' && c<='Z'?c+'a'-'A':c;});
                    return value;
                };
                const auto shape=token("WheelShape");
                entry.layout.shape=shape=="ellipse"?WheelShape::Ellipse:shape=="polygon"?WheelShape::Polygon:WheelShape::Circle;
                entry.layout.stretchX=Number(values,"WheelStretchX",entry.layout.stretchX,.9f,1.25f);
                entry.layout.stretchY=Number(values,"WheelStretchY",entry.layout.stretchY,.9f,1.2f);
                const auto detail=token("DetailPosition");
                entry.layout.detailPosition=detail=="left"?DetailPosition::Left:detail=="right"?DetailPosition::Right:
                    detail=="top"?DetailPosition::Top:detail=="bottom"?DetailPosition::Bottom:
                    detail=="center"?DetailPosition::Center:DetailPosition::Auto;
                entry.layout.detailGap=Number(values,"DetailGap",entry.layout.detailGap,16.f,80.f);
                // Resource paths belong to the theme package, not arbitrary absolute files.
                const auto texture=std::filesystem::u8path(values["SurfaceTexture"]);
                const bool traversal=std::any_of(texture.begin(),texture.end(),[](const auto& part){return part=="..";});
                if(!texture.empty() && !texture.has_root_path() && !traversal &&
                    values["SurfaceTexture"].find(':')==std::string::npos && texture.extension()==".png") {
                    const auto resolved=(path.parent_path()/texture).lexically_normal().generic_u8string();
                    entry.surfaceTexture.assign(resolved.begin(),resolved.end());
                }
                entry.materialStrength=Number(values,"MaterialStrength",entry.materialStrength,0.f,1.f);
                entry.materialZoom=Number(values,"MaterialZoom",entry.materialZoom,1.f,4.f);
                for (auto [key,field]:{std::pair{"Accent",&entry.accent},{"Text",&entry.text},{"Muted",&entry.muted},
                    {"Sector",&entry.sector},{"Empty",&entry.empty},{"Hover",&entry.hover},{"Panel",&entry.panel},{"Background",&entry.background},{"Border",&entry.border}})
                    if (values.contains(key)) *field=Color(values[key],*field);
                entry.borderWidth=Number(values,"BorderWidth",entry.borderWidth,.5f,2.f);
                entry.cornerRadius=Number(values,"CornerRadius",entry.cornerRadius,0.f,16.f);
                entry.ornament=Number(values,"Ornament",entry.ornament,0.f,1.f);
                entry.relief=Number(values,"Relief",entry.relief,0.f,1.f);
                entry.textShadow=Number(values,"TextShadow",entry.textShadow,0.f,1.f);
                entry.iconScale=Number(values,"IconScale",entry.iconScale,.8f,1.15f);
                entry.titleScale=Number(values,"TitleScale",entry.titleScale,.85f,1.15f);
                entry.labelScale=Number(values,"LabelScale",entry.labelScale,.9f,1.1f);
                entry.hoverDuration=Number(values,"HoverDuration",entry.hoverDuration,0.f,.25f);
                entry.pageDuration=Number(values,"PageDuration",entry.pageDuration,0.f,.25f);
                if (entry.id=="classic") loadedThemes[0]=entry; else loadedThemes.push_back(entry);
            }
            languages.swap(loadedLanguages);themes.swap(loadedThemes);
        } catch(const std::exception& error) {
            resourceLoadDiagnostic=error.what();
            OutputDebugStringA("FavoriteWheel: LoadResources failed; keeping built-in resources: ");
            OutputDebugStringA(error.what());OutputDebugStringA("\n");
        } catch(...) {
            resourceLoadDiagnostic="unknown exception";
            OutputDebugStringA("FavoriteWheel: LoadResources failed; keeping built-in resources (unknown exception).\n");
        }
    }
    const std::string& ResourceLoadDiagnostic(){return resourceLoadDiagnostic;}
    const std::vector<Language>& Languages(){return languages;}
    const std::string& SystemLanguage(){return systemLanguage;}
    std::string ResolveLanguage(const std::string& requested,const std::string& systemLocale) {
        auto code=NormalizeLanguage(requested);
        if(code=="auto")code=NormalizeLanguage(systemLocale);
        while(!code.empty()) {
            for(const auto& language:languages)if(NormalizeLanguage(language.id)==code)return language.id;
            const auto separator=code.rfind('-');
            if(separator==std::string::npos)break;
            code.resize(separator);
        }
        return "en";
    }
    std::string ActiveLanguage(const Settings& config) {
        std::shared_ptr<const LanguageCache> cached;
        {
            std::lock_guard lock(cacheMutex);
            cached=languageCache;
        }
        if(cached && cached->requested==config.language)return cached->active;
        auto updated=std::make_shared<const LanguageCache>(LanguageCache{config.language,ResolveLanguage(config.language,systemLanguage)});
        {
            std::lock_guard lock(cacheMutex);
            cached=std::move(languageCache);languageCache=updated;
        }
        return updated->active;
    }
    std::string LanguageLabel(const Settings& config) {
        const auto active=ActiveLanguage(config);
        std::string name=active;
        for(const auto& language:languages)if(language.id==active){name=language.name;break;}
        return NormalizeLanguage(config.language)=="auto"?Tr(config,"languageAuto")+" ("+name+")":name;
    }
    const std::vector<Theme>& Themes(){return themes;}
    const Theme& Style(const Settings& config) { for (const auto& t:themes) if(t.id==config.theme)return t; return themes[0]; }
    std::string Tr(const Settings& config,const std::string& key) {
        const auto active=ActiveLanguage(config);
        for(const auto& language:languages) if(language.id==active) {
            const auto it=language.text.find(key); if(it!=language.text.end()&&!it->second.empty())return it->second;
        }
        const auto translated=languages[0].text.find(key);
        if(translated!=languages[0].text.end()&&!translated->second.empty())return translated->second;
        const auto it=english.find(key); return it!=english.end()?it->second:key;
    }
    std::string UIGlyphs(const Settings& config) {return *UIGlyphSnapshot(config);}
    std::shared_ptr<const std::string> UIGlyphSnapshot(const Settings& config) {
        const auto active=ActiveLanguage(config);
        std::shared_ptr<const GlyphCache> cached;
        {
            std::lock_guard lock(cacheMutex);
            cached=glyphCache;
        }
        if(cached && cached->active==active && cached->hotkey==config.hotkey &&
            cached->switchKey==config.switchKey && cached->actionHotkey==config.actionHotkey)return {cached,&cached->text};
        std::string text;
        for(const auto& [key,value]:english) text+=Tr(config,key);
        for(const auto& language:languages) text+=language.name;
        for(const auto& theme:themes) text+=theme.name;
        text+=KeyLabel(config);auto switchConfig=config;switchConfig.hotkey=config.switchKey;text+=KeyLabel(switchConfig);
        switchConfig.hotkey=config.actionHotkey;text+=KeyLabel(switchConfig);
        auto updated=std::make_shared<const GlyphCache>(GlyphCache{active,config.hotkey,config.switchKey,config.actionHotkey,std::move(text)});
        {
            std::lock_guard lock(cacheMutex);
            cached=std::move(glyphCache);glyphCache=updated;
        }
        return {updated,&updated->text};
    }
    std::string KeyLabel(const Settings& config) {
        if(config.hotkey<0)return Tr(config,"follow");
        wchar_t name[128]{};
        const auto code=static_cast<unsigned>(config.hotkey);
        const LONG parameter=static_cast<LONG>(((code&127)<<16)|((code&128)?(1<<24):0));
        if(!GetKeyNameTextW(parameter,name,128))return "Key "+std::to_string(code);
        const int size=WideCharToMultiByte(CP_UTF8,0,name,-1,nullptr,0,nullptr,nullptr);
        std::string text(size,'\0');
        WideCharToMultiByte(CP_UTF8,0,name,-1,text.data(),size,nullptr,nullptr);
        text.pop_back();return text;
    }
    std::string ModifierLabel(const Settings& config,int modifier) {
        if(!modifier)return Tr(config,"modifierNone");
        std::string result;
        for(auto [bit,name]:{std::pair{1,"Shift"},std::pair{2,"Ctrl"},std::pair{4,"Alt"}})
            if(modifier&bit){if(!result.empty())result+=" + ";result+=name;}
        return result;
    }
    std::string PadLabel(const Settings& config,int key,bool follow) {
        if(key<0)return Tr(config,follow?"follow":"modifierNone");
        constexpr const char* names[]={"D-Pad Up","D-Pad Down","D-Pad Left","D-Pad Right","Start","Back","LS","RS","LB","RB","A","B","X","Y","LT","RT"};
        return key>=266 && key<=281?names[key-266]:"?";
    }
    std::string FontPath(const Settings& config) {
        const auto active=ActiveLanguage(config);
        for(const auto& language:languages) if(language.id==active&&!language.font.empty())return language.font;
        const auto& theme=Style(config); return theme.font.empty()?config.font:theme.font;
    }
    const std::vector<std::string>& FallbackFontPaths(){return fallbackFonts;}
    std::string CycleLanguage(const std::string& id,int delta){
        const auto code=NormalizeLanguage(id)=="auto"?"auto":NormalizeLanguage(ResolveLanguage(id,systemLanguage));int index=0;
        for(int i=0;i<static_cast<int>(languages.size());++i)if(NormalizeLanguage(languages[i].id)==code)index=i+1;
        const int count=static_cast<int>(languages.size())+1;
        index=((index+delta)%count+count)%count;
        return index==0?"auto":languages[index-1].id;
    }
    std::string CycleTheme(const std::string& id,int delta){return Cycle(themes,id,delta);}
}
