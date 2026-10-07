#include "Settings.h"
#include <Windows.h>
#include <algorithm>
#include <filesystem>
#include <mutex>
#include <fstream>

namespace Wheel {
    namespace {
        std::mutex settingsMutex;
        Settings settings, saved;
        bool editing = false;
        std::filesystem::path settingsPath = "Data/SKSE/Plugins/FavoriteWheel.ini";
        std::string Read(const wchar_t* section, const wchar_t* key, const std::string& fallback) {
            wchar_t value[2048]{};
            const auto file = std::filesystem::absolute(settingsPath).wstring();
            GetPrivateProfileStringW(section, key, L"", value, 2048, file.c_str());
            if (!*value) return fallback;
            const int size = WideCharToMultiByte(CP_UTF8,0,value,-1,nullptr,0,nullptr,nullptr);
            std::string result(size, '\0');
            WideCharToMultiByte(CP_UTF8,0,value,-1,result.data(),size,nullptr,nullptr);
            result.pop_back(); return result;
        }
        void Clamp(Settings& v) {
            v.timeMode=std::clamp(v.timeMode,0,2);
            v.slowPercent=std::clamp(v.slowPercent,5,100);
            v.scale = std::clamp(v.scale,.6f,1.5f);
            v.wheelScale = std::clamp(v.wheelScale,.6f,1.5f);
            v.positionX=std::clamp(v.positionX,0,100);
            v.positionY=std::clamp(v.positionY,0,100);
            v.overlayOpacity=std::clamp(v.overlayOpacity,0,80);
            v.sensitivity = std::clamp(v.sensitivity,.2f,3.f);
            if(v.switchKey<2 || v.switchKey>255 || v.switchKey==60)v.switchKey=19;
            if (v.hotkey < -1 || v.hotkey > 255) v.hotkey = -1;
            if(v.actionHotkey< -1 || v.actionHotkey>255)v.actionHotkey=-1;
            v.hotkeyModifier=std::clamp(v.hotkeyModifier,0,7);
            v.actionModifier=std::clamp(v.actionModifier,0,7);
            if(v.gamepadHotkey!= -1 && (v.gamepadHotkey<266 || v.gamepadHotkey>281))v.gamepadHotkey=-1;
            if(v.gamepadModifier!= -1 && (v.gamepadModifier<266 || v.gamepadModifier>281))v.gamepadModifier=-1;
            if(v.gamepadActionModifier!= -1 && (v.gamepadActionModifier<266 || v.gamepadActionModifier>281))v.gamepadActionModifier=274;
        }
    }
    Settings Config() { std::lock_guard lock(settingsMutex); return settings; }
    void SetSettingsPath(const std::string& path) { settingsPath = std::filesystem::u8path(path); }
    void LoadSettings() {
        std::lock_guard lock(settingsMutex);
        const auto path = std::filesystem::absolute(settingsPath).wstring();
        auto number = [&](const wchar_t* section, const wchar_t* key, int value) {
            return static_cast<int>(GetPrivateProfileIntW(section,key,value,path.c_str()));
        };
        settings = Settings{};
        settings.enabled = number(L"General",L"Enabled",1) != 0;
        settings.allInventory = number(L"General",L"AllInventory",0) != 0;
        settings.timeMode=number(L"General",L"TimeMode",0);
        settings.slowPercent=number(L"General",L"SlowTimePercent",20);
        const auto legacyLanguage = Read(L"General",L"Chinese","");
        settings.language = Read(L"General",L"Language",legacyLanguage.empty() ? "auto" :
            (number(L"General",L"Chinese",1) ? "zh_CN" : "en"));
        settings.theme = Read(L"Display",L"Theme","classic");
        settings.showHints = number(L"Display",L"ShowHints",1) != 0;
        settings.wheelScale=number(L"Display",L"WheelScalePercent",100)/100.f;
        settings.positionX=number(L"Display",L"PositionXPercent",28);
        settings.positionY=number(L"Display",L"PositionYPercent",46);
        settings.overlayOpacity=number(L"Display",L"OverlayOpacityPercent",35);
        settings.sounds=number(L"Effects",L"Sounds",1)!=0;
        settings.animations=number(L"Effects",L"Animations",1)!=0;
        settings.switchKey=number(L"Controls",L"SwitchWheelKey",19);
        settings.hotkey = number(L"Controls",L"Hotkey",-1);
        settings.hotkeyModifier=number(L"Controls",L"HotkeyModifier",0);
        settings.actionHotkey=number(L"Controls",L"ActionHotkey",-1);
        settings.actionModifier=number(L"Controls",L"ActionModifier",1);
        settings.gamepadHotkey=number(L"Controls",L"GamepadHotkey",-1);
        settings.gamepadModifier=number(L"Controls",L"GamepadModifier",-1);
        settings.gamepadActionModifier=number(L"Controls",L"GamepadActionModifier",274);
        settings.scale = number(L"Display",L"ScalePercent",100)/100.f;
        settings.sensitivity = number(L"Controls",L"SensitivityPercent",100)/100.f;
        settings.font = Read(L"Display",L"Font",settings.font);
        Clamp(settings); saved = settings; editing = false;
    }
    void BeginSettings() { std::lock_guard lock(settingsMutex); if (!editing) { saved=settings; editing=true; } }
    void EditSettings(Settings values) { std::lock_guard lock(settingsMutex); Clamp(values); settings=std::move(values); }
    void RevertSettings() { std::lock_guard lock(settingsMutex); if (editing) settings=saved; editing=false; }
    void DefaultSettings() {
        std::lock_guard lock(settingsMutex);
        auto defaults = Settings{};
        defaults.enabled=settings.enabled; defaults.font=settings.font;
        settings=std::move(defaults);
    }
    bool SaveSettings() {
        std::lock_guard lock(settingsMutex);
        // Stage the complete update before replacing the INI. Preserve unrelated keys.
        std::error_code error;
        auto path=std::filesystem::absolute(settingsPath), temporary=path;
        temporary += L".tmp";
        std::filesystem::create_directories(path.parent_path(),error);
        if (error) return false;
        if (std::filesystem::exists(path)) {
            std::filesystem::copy_file(path,temporary,std::filesystem::copy_options::overwrite_existing,error);
            if (error) return false;
        } else {
            std::ofstream file(temporary,std::ios::binary|std::ios::trunc);
            file.put(static_cast<char>(0xFF)); file.put(static_cast<char>(0xFE));
            if (!file) return false;
        }
        auto write = [&](const wchar_t* section,const wchar_t* key,const std::string& value) {
            const int size=MultiByteToWideChar(CP_UTF8,0,value.c_str(),-1,nullptr,0);
            std::wstring wide(size,L'\0');
            MultiByteToWideChar(CP_UTF8,0,value.c_str(),-1,wide.data(),size);
            return WritePrivateProfileStringW(section,key,wide.c_str(),temporary.c_str()) != 0;
        };
        const bool ok = write(L"General",L"Language",settings.language) &&
            write(L"General",L"AllInventory",settings.allInventory?"1":"0") &&
            write(L"General",L"TimeMode",std::to_string(settings.timeMode)) &&
            write(L"General",L"SlowTimePercent",std::to_string(settings.slowPercent)) &&
            write(L"Display",L"Theme",settings.theme) &&
            write(L"Display",L"ShowHints",settings.showHints?"1":"0") &&
            write(L"Display",L"WheelScalePercent",std::to_string(static_cast<int>(settings.wheelScale*100+.5f))) &&
            write(L"Display",L"PositionXPercent",std::to_string(settings.positionX)) &&
            write(L"Display",L"PositionYPercent",std::to_string(settings.positionY)) &&
            write(L"Display",L"OverlayOpacityPercent",std::to_string(settings.overlayOpacity)) &&
            write(L"Effects",L"Sounds",settings.sounds?"1":"0") &&
            write(L"Effects",L"Animations",settings.animations?"1":"0") &&
            write(L"Controls",L"SwitchWheelKey",std::to_string(settings.switchKey)) &&
            write(L"Controls",L"Hotkey",std::to_string(settings.hotkey)) &&
            write(L"Controls",L"HotkeyModifier",std::to_string(settings.hotkeyModifier)) &&
            write(L"Controls",L"ActionHotkey",std::to_string(settings.actionHotkey)) &&
            write(L"Controls",L"ActionModifier",std::to_string(settings.actionModifier)) &&
            write(L"Controls",L"GamepadHotkey",std::to_string(settings.gamepadHotkey)) &&
            write(L"Controls",L"GamepadModifier",std::to_string(settings.gamepadModifier)) &&
            write(L"Controls",L"GamepadActionModifier",std::to_string(settings.gamepadActionModifier)) &&
            write(L"Controls",L"SensitivityPercent",std::to_string(static_cast<int>(settings.sensitivity*100+.5f))) &&
            write(L"Display",L"ScalePercent",std::to_string(static_cast<int>(settings.scale*100+.5f)));
        WritePrivateProfileStringW(nullptr,nullptr,nullptr,temporary.c_str());
        if (!ok || !MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) return false;
        WritePrivateProfileStringW(nullptr,nullptr,nullptr,path.c_str());
        saved=settings; editing=false; return true;
    }
}
