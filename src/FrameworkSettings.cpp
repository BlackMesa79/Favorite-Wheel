#include "FrameworkSettings.h"
#include "Settings.h"
#include "UIResources.h"
#include "Wheel.h"
#include <Windows.h>
#include <array>
#include <algorithm>
#include <cfloat>
#include <exception>

namespace Wheel::FrameworkSettings {
    namespace {
        // Runtime C ABI from QTR-Modding/SKSE-Menu-Framework-3-API.
        // Never call our private ImGui implementation with the framework's context.
        struct Vec2 {float x=0,y=0;};
        using RenderCallback=void(__stdcall*)();
        enum class Page { Appearance, Gameplay, Keyboard, Controller };
        struct API {
            void (*add)(const char*,RenderCallback)=nullptr;
            void (*text)(const char*,const char*)=nullptr;
            bool (*check)(const char*,bool*)=nullptr;
            bool (*slider)(const char*,int*,int,int,const char*,int)=nullptr;
            bool (*sliderFloat)(const char*,float*,float,float,const char*,int)=nullptr;
            bool (*button)(const char*,Vec2)=nullptr;
            void (*sameLine)(float,float)=nullptr;
            void (*separatorText)(const char*)=nullptr;
            void (*spacing)()=nullptr;
            void (*pushWrap)(float)=nullptr;
            void (*popWrap)()=nullptr;
            void (*nextWidth)(float)=nullptr;
            void (*alignText)()=nullptr;
            void (*available)(Vec2*)=nullptr;
            float (*fontSize)()=nullptr;
            bool (*beginTable)(const char*,int,int,Vec2,float)=nullptr;
            void (*endTable)()=nullptr;
            void (*nextRow)(int,float)=nullptr;
            bool (*nextColumn)()=nullptr;
            void (*setupColumn)(const char*,int,float,unsigned)=nullptr;
            void (*beginDisabled)(bool)=nullptr;
            void (*endDisabled)()=nullptr;
            bool (*beginCombo)(const char*,const char*,int)=nullptr;
            void (*endCombo)()=nullptr;
            bool (*select)(const char*,bool,int,Vec2)=nullptr;
        } api;
        Settings baseline,draft;
        bool initialized=false;
        std::string status;
        template<class T> bool Bind(HMODULE module,T& target,const char* name) {
            target=reinterpret_cast<T>(GetProcAddress(module,name));return target!=nullptr;
        }
        std::string Label(const char* key) {return Tr(draft,key)+"###fw_"+key;}
        std::string Row(const char* key) {
            api.nextRow(0,api.fontSize()*2.1f);api.nextColumn();api.alignText();
            api.pushWrap(0);const auto text=Tr(draft,key);api.text(text.c_str(),nullptr);api.popWrap();
            api.nextColumn();api.nextWidth(-FLT_MIN);return std::string("###fw_")+key;
        }
        void Wrapped(const std::string& text) {api.pushWrap(0);api.text(text.c_str(),nullptr);api.popWrap();}
        void Text(const char* key){const auto text=Tr(draft,key);api.text(text.c_str(),nullptr);}
        void Check(const char* key,bool& value){api.check(Row(key).c_str(),&value);}
        void Slider(const char* key,int& value,int low,int high){api.slider(Row(key).c_str(),&value,low,high,"%d",0);}
        void Scale(const char* key,float& value,float low,float high){api.sliderFloat(Row(key).c_str(),&value,low,high,"%.2f",0);}
        template<class Name> void Choice(const char* key,int& value,int low,int high,Name name) {
            if(api.beginCombo(Row(key).c_str(),name(value).c_str(),0)) {
                for(int i=low;i<=high;++i) {
                    const auto label=name(i)+"###option_"+std::to_string(i);
                    if(api.select(label.c_str(),i==value,0,{}))value=i;
                }
                api.endCombo();
            }
        }
        void Keyboard(const char* key,int& value,bool follow=true) {
            auto name=[](int code){auto config=draft;config.hotkey=code;return KeyLabel(config);};
            const bool category=std::string_view(key)=="kbCategoryPrevKey" || std::string_view(key)=="kbCategoryNextKey";
            const char* empty=category?"off":std::string_view(key)=="actionHotkey"?"followFavorite":"follow";
            const auto preview=value<0 && (std::string_view(key)=="actionHotkey" || category)?Tr(draft,empty):name(value);
            if(api.beginCombo(Row(key).c_str(),preview.c_str(),0)) {
                if(follow && api.select(Tr(draft,empty).c_str(),value<0,0,{}))value=-1;
                for(int code=2;code<=255;++code) {
                    if(code==60)continue; // F2 stays reserved for wheel settings.
                    auto text=name(code);if(text.starts_with("Key "))continue;
                    text+="###key_"+std::to_string(code);
                    if(api.select(text.c_str(),value==code,0,{}))value=code;
                }
                api.endCombo();
            }
        }
        void Pad(const char* key,int& value,bool follow=false) {
            if(api.beginCombo(Row(key).c_str(),PadLabel(draft,value,follow).c_str(),0)) {
                if(api.select(PadLabel(draft,-1,follow).c_str(),value<0,0,{}))value=-1;
                for(int code=266;code<=281;++code)if(api.select(PadLabel(draft,code).c_str(),value==code,0,{}))value=code;
                api.endCombo();
            }
        }
        void Refresh(){baseline=Config();draft=baseline;initialized=true;status.clear();}
        const char* PageKey(Page page) {
            switch(page) {
            case Page::Appearance:return "settingsGeneral";
            case Page::Gameplay:return "settingsGameplay";
            case Page::Keyboard:return "settingsKeyboard";
            case Page::Controller:return "settingsController";
            }
            return "settingsGeneral";
        }
        void Defaults(Page page) {
            const Settings defaults;
            switch(page) {
            case Page::Appearance:
                draft.language=defaults.language;draft.theme=defaults.theme;draft.scale=defaults.scale;
                draft.wheelScale=defaults.wheelScale;draft.positionX=defaults.positionX;draft.positionY=defaults.positionY;
                draft.overlayOpacity=defaults.overlayOpacity;draft.showHints=defaults.showHints;
                draft.showActionNotifications=defaults.showActionNotifications;
                draft.sounds=defaults.sounds;draft.animations=defaults.animations;break;
            case Page::Gameplay:
                draft.allInventory=defaults.allInventory;draft.hideEmptyCategories=defaults.hideEmptyCategories;
                draft.rememberLastWheel=defaults.rememberLastWheel;draft.keepOpen=defaults.keepOpen;
                draft.keepOpenConsumables=defaults.keepOpenConsumables;draft.timeMode=defaults.timeMode;
                draft.slowPercent=defaults.slowPercent;
                draft.kbMoveWhileOpen=defaults.kbMoveWhileOpen;draft.kbCategoryPrevKey=defaults.kbCategoryPrevKey;draft.kbCategoryNextKey=defaults.kbCategoryNextKey;break;
            case Page::Keyboard:
                draft.hotkey=defaults.hotkey;draft.actionHotkey=defaults.actionHotkey;draft.switchKey=defaults.switchKey;
                draft.hotkeyModifier=defaults.hotkeyModifier;draft.actionModifier=defaults.actionModifier;
                draft.sensitivity=defaults.sensitivity;draft.swapMouseHands=defaults.swapMouseHands;break;
            case Page::Controller:
                draft.gamepadHotkey=defaults.gamepadHotkey;draft.gamepadModifier=defaults.gamepadModifier;
                draft.gamepadActionModifier=defaults.gamepadActionModifier;draft.gamepadCategoryButtons=defaults.gamepadCategoryButtons;
                draft.gamepadMoveWhileOpen=defaults.gamepadMoveWhileOpen;break;
            }
            status.clear();
        }
        void RenderPage(Page page) {
            if(!initialized)Refresh();
            const auto current=Config();
            if(draft==baseline && current!=baseline)Refresh();
            Wrapped(Tr(draft,"frameworkDraftHelp"));
            api.spacing();
            if(IsOpen()){Text("frameworkCloseWheel");return;}
            if(api.button(Label("frameworkApply").c_str(),{})) {
                if(SaveExternalSettings(baseline,draft)){Refresh();status=Tr(draft,"frameworkSaved");}
                else {status=Tr(draft,"frameworkSaveFailed");SKSE::log::warn("Framework settings: {}",SettingsDiagnostic());}
            }
            api.sameLine(0,-1);
            if(api.button(Label("cancel").c_str(),{}))Refresh();
            api.sameLine(0,-1);
            if(api.button(Label("defaults").c_str(),{})) {
                Defaults(page);
            }
            if(!status.empty())Wrapped(status);
            if(Config()!=baseline)Wrapped(Tr(draft,"frameworkConflict"));
            else if(draft!=baseline)Wrapped(Tr(draft,"frameworkPending"));
            api.spacing();api.separatorText(Tr(draft,PageKey(page)).c_str());api.spacing();
            Vec2 available;api.available(&available);
            // ImGui table/column flags from the framework C ABI; no private ImGui context access.
            if(!api.beginTable("###fw_settings",2,(1<<4)|(3<<13),{std::min(available.x,api.fontSize()*48.f),0},0))return;
            api.setupColumn("label",1<<3,.44f,0);api.setupColumn("value",1<<3,.56f,0);
            const auto before=draft;
            switch(page) {
            case Page::Appearance: {
                auto languageName=Tr(draft,"languageAuto");
                for(const auto& item:Languages())if(item.id==draft.language)languageName=item.name;
                if(api.beginCombo(Row("language").c_str(),languageName.c_str(),0)) {
                    if(api.select(Tr(draft,"languageAuto").c_str(),draft.language=="auto",0,{}))draft.language="auto";
                    for(const auto& item:Languages())if(api.select((item.name+"###"+item.id).c_str(),draft.language==item.id,0,{}))draft.language=item.id;
                    api.endCombo();
                }
                const auto themeName=Style(draft).name;
                if(api.beginCombo(Row("theme").c_str(),themeName.c_str(),0)) {
                    for(const auto& item:Themes())if(api.select((item.name+"###"+item.id).c_str(),draft.theme==item.id,0,{}))draft.theme=item.id;
                    api.endCombo();
                }
                Scale("scale",draft.scale,.6f,1.5f);Scale("wheelSize",draft.wheelScale,.6f,1.5f);
                Slider("positionX",draft.positionX,0,100);Slider("positionY",draft.positionY,0,100);
                Slider("dim",draft.overlayOpacity,0,80);
                Check("hints",draft.showHints);Check("actionNotifications",draft.showActionNotifications);
                Check("sounds",draft.sounds);Check("animations",draft.animations);
                break;
            }
            case Page::Gameplay: {
                int scope=draft.allInventory;
                Choice("inventoryScope",scope,0,1,[](int i){return Tr(draft,i?"scopeAll":"scopeFavorites");});draft.allInventory=scope!=0;
                Check("hideEmptyCategories",draft.hideEmptyCategories);Check("rememberLastWheel",draft.rememberLastWheel);
                Check("keepOpen",draft.keepOpen);Check("keepOpenConsumables",draft.keepOpenConsumables);
                Choice("timeMode",draft.timeMode,0,2,[](int i){return Tr(draft,std::array{"timePause","timeSlow","timeNormal"}[i]);});
                api.beginDisabled(draft.timeMode!=1);
                Slider("slowTimePercent",draft.slowPercent,50,100);
                api.endDisabled();
                Check("kbMoveWhileOpen",draft.kbMoveWhileOpen);Keyboard("kbCategoryPrevKey",draft.kbCategoryPrevKey);Keyboard("kbCategoryNextKey",draft.kbCategoryNextKey);
                break;
            }
            case Page::Keyboard: {
                Keyboard("hotkey",draft.hotkey);Keyboard("actionHotkey",draft.actionHotkey);Keyboard("switchWheelKey",draft.switchKey,false);
                Choice("favoriteModifier",draft.hotkeyModifier,0,7,[](int i){return ModifierLabel(draft,i);});
                Choice("actionModifier",draft.actionModifier,0,7,[](int i){return ModifierLabel(draft,i);});
                Scale("sensitivity",draft.sensitivity,.2f,3.f);Check("swapMouseHands",draft.swapMouseHands);
                break;
            }
            case Page::Controller: {
                Pad("gamepadHotkey",draft.gamepadHotkey,true);Pad("gamepadModifier",draft.gamepadModifier);Pad("gamepadActionModifier",draft.gamepadActionModifier);
                Choice("gamepadCategoryButtons",draft.gamepadCategoryButtons,0,1,[](int i){return std::string(i?"LT / RT":"LB / RB");});
                Check("gamepadMoveWhileOpen",draft.gamepadMoveWhileOpen);
                break;
            }
            }
            api.endTable();
            if(draft!=before)status.clear();
        }
        template<Page page> void __stdcall Render() {
            try {RenderPage(page);}catch(const std::exception& e){SKSE::log::error("Framework settings rendering failed: {}",e.what());}
        }
    }
    void Register() {
        static bool registered=false;if(registered)return;
        const auto module=GetModuleHandleW(L"SKSEMenuFramework.dll");
        if(!module){SKSE::log::info("SKSE Menu Framework absent; wheel settings remain available");return;}
        const bool compatible=Bind(module,api.add,"AddSectionItem") && Bind(module,api.text,"igTextUnformatted") &&
            Bind(module,api.check,"igCheckbox") && Bind(module,api.slider,"igSliderInt") && Bind(module,api.sliderFloat,"igSliderFloat") &&
            Bind(module,api.button,"igButton") && Bind(module,api.sameLine,"igSameLine") &&
            Bind(module,api.separatorText,"igSeparatorText") && Bind(module,api.spacing,"igSpacing") &&
            Bind(module,api.pushWrap,"igPushTextWrapPos") && Bind(module,api.popWrap,"igPopTextWrapPos") &&
            Bind(module,api.nextWidth,"igSetNextItemWidth") &&
            Bind(module,api.alignText,"igAlignTextToFramePadding") && Bind(module,api.available,"igGetContentRegionAvail") &&
            Bind(module,api.fontSize,"igGetFontSize") && Bind(module,api.beginTable,"igBeginTable") &&
            Bind(module,api.endTable,"igEndTable") && Bind(module,api.nextRow,"igTableNextRow") &&
            Bind(module,api.nextColumn,"igTableNextColumn") && Bind(module,api.setupColumn,"igTableSetupColumn") &&
            Bind(module,api.beginDisabled,"igBeginDisabled") && Bind(module,api.endDisabled,"igEndDisabled") &&
            Bind(module,api.beginCombo,"igBeginCombo") && Bind(module,api.endCombo,"igEndCombo") && Bind(module,api.select,"igSelectable_Bool");
        if(!compatible){SKSE::log::warn("SKSE Menu Framework API incomplete; optional settings page not registered");return;}
        const auto config=Config();
        api.add(("FavoriteWheel/"+Tr(config,"settingsGeneral")).c_str(),Render<Page::Appearance>);
        api.add(("FavoriteWheel/"+Tr(config,"settingsGameplay")).c_str(),Render<Page::Gameplay>);
        api.add(("FavoriteWheel/"+Tr(config,"settingsKeyboard")).c_str(),Render<Page::Keyboard>);
        api.add(("FavoriteWheel/"+Tr(config,"settingsController")).c_str(),Render<Page::Controller>);
        registered=true;
        SKSE::log::info("SKSE Menu Framework registered four settings pages");
    }
}
