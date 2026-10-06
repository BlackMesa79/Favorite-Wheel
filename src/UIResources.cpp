#include "UIResources.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cmath>
#include <Windows.h>

namespace Wheel {
    namespace {
        const std::unordered_map<std::string,std::string> english{
            {"infoWeight","Weight"},{"infoValue","Value"},{"infoDamage","Damage"},{"infoBaseDamage","Base damage"},
            {"infoArmor","Armor"},{"infoMagicka","Magicka"},{"infoPerSecond","/s"},
            {"infoEffects","BASE EFFECTS"},{"infoEnchantment","BASE ENCHANTMENT EFFECTS"},
            {"infoMagnitude","Magnitude"},{"infoSeconds","s"},{"infoArea","Area"},{"infoFeet","ft"},
            {"infoUnnamedEffect","Unnamed effect"},{"infoMoreEffects","more effects"},
            {"lightUse","LMB TOGGLE PREFERENCE / OPEN LIST"},
            {"lightNavigation","SWITCH WHEEL · ESC BACK · F2 SETTINGS"},
            {"lightUnavailableShort","API UNAVAILABLE"},

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
            {"wheelBindHint","Click to bind; right-click to reset (Favorites: game / switch: R)"},{"switchWheelKey","Switch wheel key"},
            {"outfitWheelTitle","FUNCTIONS · OUTFITS"},
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
            {"outfitEmpty","No eligible armor is worn. Weapons and shields are excluded."},
            {"outfitLimit","The limit is 100 presets."},
            {"outfitSaved","Outfit recorded. Save your game to keep it."},
            {"outfitNotPortable","Dynamic items or enchantments cannot be exported across characters."},
            {"outfitExported","Exported to FavoriteWheel/Outfits/Exports."},
            {"outfitFileError","Could not write outfit file. See the log."},
            {"outfitNoImport","No .fwo preset files in the Imports folder."},
            {"outfitImported","Outfits imported. Save your game to keep them."},
            {"outfitImportFailed","Import canceled: invalid file, missing items or ambiguous instances. See log."},
            {"outfitMissing","Missing or ambiguous outfit items; no equipment changed."},
            {"outfitBlocked","Outfit canceled: protected equipment or a shield slot conflict."},
            {"outfitChanged","Equipment changed; outfit operation stopped. Reopen the wheel."},
            {"outfitPartial","Outfit change was incomplete. Check your equipment and log."},
            {"outfitRemoved","Armor, clothing and jewelry removed. Weapons and shields retained."},
            {"outfitApplied","Outfit equipped."},
            {"title","F A V O R I T E S"},{"weapons","WEAPONS"},{"armor","APPAREL"},{"potions","POTIONS"},{"food","FOOD"},{"magic","MAGIC"},{"other","OTHER"},
            {"equipped","EQUIPPED"},{"select","CLICK TO SELECT"},{"inventory","USE IN INVENTORY"},{"empty","NO FAVORITES"},{"move","MOVE TO SELECT"},{"count","FAVORITES"},
            {"categoriesHint","A / D  CATEGORY     W / S OR SCROLL  PAGE"},{"useHint","LMB USE / RIGHT HAND    RMB LEFT HAND    ESC CLOSE"},
            {"compactNavigation","A / D CATEGORY  ·  W / S / SCROLL PAGE"},{"compactUse","LMB USE / RIGHT HAND  ·  RMB LEFT HAND"},{"compactClose","ESC CLOSE  ·  F2 SETTINGS"},
            {"wheelSize","Wheel size"},{"positionX","Horizontal position X"},{"positionY","Vertical position Y"},{"overlayOpacity","Background dimming"},{"sounds","Open / close sounds"},{"animations","Open / close animation"},
            {"layoutSettingsHelp","Apply and return to check layout; Esc cancels changes."},{"settings","SETTINGS"},{"settingsTitle","WHEEL SETTINGS"},{"settingsHelp","Changes preview immediately. Apply to save; Esc cancels."},
            {"scale","Interface size"},{"sensitivity","Mouse sensitivity"},{"hints","Control hints"},{"language","Language"},{"theme","Theme"},{"hotkey","Favorites key"},
            {"on","ON"},{"off","OFF"},{"follow","Follow game"},{"capture","Press a key... Esc cancels"},{"bindHint","Click to bind; right-click to follow game"},
            {"apply","APPLY"},{"cancel","CANCEL"},{"defaults","DEFAULTS"},{"saveError","Could not save settings. Check folder access."},
            {"changed","Item changed. Reopen the wheel."},{"unsupported","Use this item in the inventory for now."}
        };
        std::vector<Language> languages{{"en","English","",english}};
        std::vector<Theme> themes{Theme{}};
        std::string Trim(std::string value) {
            const auto first=value.find_first_not_of(" \t\r\n");
            return first==std::string::npos ? "" : value.substr(first,value.find_last_not_of(" \t\r\n")-first+1);
        }
        std::unordered_map<std::string,std::string> Read(const std::filesystem::path& file) {
            std::unordered_map<std::string,std::string> values;
            std::ifstream stream(file,std::ios::binary); std::string line;
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
            for (std::filesystem::directory_iterator it(dir,error),end; !error && it!=end; it.increment(error))
                if (it->is_regular_file() && it->path().extension()==".ini") result.push_back(it->path());
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
        languages={{"en","English","",english}}; themes={Theme{}};
        for (const auto& path:Files(std::filesystem::u8path(root)/"Languages")) {
            auto values=Read(path); const auto id=path.stem().string();
            Language entry{id,values.contains("Name")?values["Name"]:id,values["Font"],std::move(values)};
            if (id=="en") languages[0]=std::move(entry); else languages.push_back(std::move(entry));
        }
        for (const auto& path:Files(std::filesystem::u8path(root)/"Themes")) {
            auto values=Read(path); Theme entry;
            entry.id=path.stem().string(); entry.name=values.contains("Name")?values["Name"]:entry.id; entry.font=values["Font"];
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
            if (entry.id=="classic") themes[0]=entry; else themes.push_back(entry);
        }
    }
    const std::vector<Language>& Languages(){return languages;}
    const std::vector<Theme>& Themes(){return themes;}
    const Theme& Style(const Settings& config) { for (const auto& t:themes) if(t.id==config.theme)return t; return themes[0]; }
    std::string Tr(const Settings& config,const std::string& key) {
        for(const auto& language:languages) if(language.id==config.language) {
            const auto it=language.text.find(key); if(it!=language.text.end()&&!it->second.empty())return it->second;
        }
        const auto it=english.find(key); return it!=english.end()?it->second:key;
    }
    std::string UIGlyphs(const Settings& config) {
        std::string text;
        for(const auto& [key,value]:english) text+=Tr(config,key);
        for(const auto& language:languages) text+=language.name;
        for(const auto& theme:themes) text+=theme.name;
        text+=KeyLabel(config);auto switchConfig=config;switchConfig.hotkey=config.switchKey;text+=KeyLabel(switchConfig);
        return text;
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
    std::string FontPath(const Settings& config) {
        for(const auto& language:languages) if(language.id==config.language&&!language.font.empty())return language.font;
        const auto& theme=Style(config); return theme.font.empty()?config.font:theme.font;
    }
    std::string CycleLanguage(const std::string& id,int delta){return Cycle(languages,id,delta);}
    std::string CycleTheme(const std::string& id,int delta){return Cycle(themes,id,delta);}
}
