#include "Settings.h"
#include "TimePolicy.h"
#include "SettingsIni.h"
#include <Windows.h>
#include <algorithm>
#include <filesystem>
#include <mutex>
#include <vector>
#include <cmath>
#include <cwchar>

namespace Wheel {
    namespace {
        std::mutex settingsMutex;
        Settings settings, saved;
        bool editing = false;
        std::filesystem::path settingsPath;
        std::string diagnostic;
        std::string UTF8(const std::filesystem::path& path) {
            const auto text=path.u8string();return {reinterpret_cast<const char*>(text.data()),text.size()};
        }
        const std::filesystem::path& SettingsFile() { // settingsMutex held.
            if(settingsPath.empty()) {
                std::wstring executable(32768,L'\0');
                const auto size=GetModuleFileNameW(nullptr,executable.data(),static_cast<DWORD>(executable.size()));
                if(!size || size>=executable.size())return settingsPath;
                executable.resize(size);
                settingsPath=std::filesystem::path(executable).parent_path()/L"Data/SKSE/Plugins/FavoriteWheel.ini";
            }
            return settingsPath;
        }
        bool Report(const char* operation,DWORD error=ERROR_SUCCESS) {
            diagnostic="path='"+UTF8(settingsPath)+"' stage="+operation+" win32="+std::to_string(error)+" writer=file-bytes-v1";
            return false;
        }
        constexpr std::size_t maxIniBytes=2*1024*1024;
        struct Close {
            HANDLE handle;
            ~Close(){if(handle!=INVALID_HANDLE_VALUE)CloseHandle(handle);}
        };
        bool ReadBytes(HANDLE file,std::vector<char>& bytes) {
            LARGE_INTEGER size{},start{};
            if(!GetFileSizeEx(file,&size))return false;
            if(size.QuadPart<0 || size.QuadPart>maxIniBytes){SetLastError(ERROR_FILE_TOO_LARGE);return false;}
            bytes.resize(static_cast<std::size_t>(size.QuadPart));DWORD read=0;
            if(!SetFilePointerEx(file,start,nullptr,FILE_BEGIN) ||
                (!bytes.empty() && !ReadFile(file,bytes.data(),static_cast<DWORD>(bytes.size()),&read,nullptr)))return false;
            if(read!=bytes.size()){SetLastError(ERROR_READ_FAULT);return false;}
            return true;
        }
        bool ReadBytes(const std::filesystem::path& path,std::vector<char>& bytes) {
            const HANDLE file=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
                nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
            if(file==INVALID_HANDLE_VALUE)return false;
            Close close{file};return ReadBytes(file,bytes);
        }
        std::string Read(const SettingsIni::Document& document,const wchar_t* section,const wchar_t* key,const std::string& fallback) {
            const auto value=document.Get(section,key);
            std::string result;
            return value && !value->empty() && SettingsIni::Utf8(*value,result)?result:fallback;
        }
        void Clamp(Settings& v) {
            v.gamepadCategoryButtons=std::clamp(v.gamepadCategoryButtons,0,1);
            v.timeMode=std::clamp(v.timeMode,0,2);
            v.slowPercent=ClampWheelSlowPercent(v.slowPercent);
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
    bool AppliedGamepadMoveWhileOpen() {std::lock_guard lock(settingsMutex);return (editing?saved:settings).gamepadMoveWhileOpen;}
    void SetSettingsPath(const std::string& path) {
        std::lock_guard lock(settingsMutex);
        settingsPath=path.empty()?std::filesystem::path{}:std::filesystem::absolute(std::filesystem::u8path(path));
    }
    std::string SettingsDiagnostic() {std::lock_guard lock(settingsMutex);return diagnostic;}
    namespace {
        Settings ReadSettings(const SettingsIni::Document& file) {
            Settings settings;
            auto number = [&](const wchar_t* section, const wchar_t* key, int value) {
                const auto text=file.Get(section,key);
                if(!text || text->empty())return value;
                // Match the legacy decimal INI fields, including signed -1
                // bindings. A malformed present value historically reads as 0.
                return static_cast<int>(std::wcstol(text->c_str(),nullptr,10));
            };
            settings.enabled = number(L"General",L"Enabled",settings.enabled) != 0;
            settings.allInventory = number(L"General",L"AllInventory",settings.allInventory) != 0;
            settings.rememberLastWheel=number(L"General",L"RememberLastWheel",settings.rememberLastWheel)!=0;
            settings.hideEmptyCategories=number(L"General",L"HideEmptyCategories",settings.hideEmptyCategories)!=0;
            settings.keepOpenConsumables=number(L"General",L"KeepOpenConsumables",settings.keepOpenConsumables)!=0;
            settings.keepOpen = number(L"General",L"KeepOpen",settings.keepOpen) != 0;
            settings.timeMode=number(L"General",L"TimeMode",settings.timeMode);
            settings.slowPercent=number(L"General",L"SlowTimePercent",settings.slowPercent);
            const auto legacyLanguage = Read(file,L"General",L"Chinese","");
            settings.language = Read(file,L"General",L"Language",legacyLanguage.empty() ? settings.language :
                (number(L"General",L"Chinese",1) ? "zh_CN" : "en"));
            settings.theme = Read(file,L"Display",L"Theme",settings.theme);
            settings.showHints = number(L"Display",L"ShowHints",settings.showHints) != 0;
            settings.showActionNotifications=number(L"Display",L"ShowActionNotifications",settings.showActionNotifications)!=0;
            settings.wheelScale=number(L"Display",L"WheelScalePercent",static_cast<int>(settings.wheelScale*100))/100.f;
            settings.positionX=number(L"Display",L"PositionXPercent",settings.positionX);
            settings.positionY=number(L"Display",L"PositionYPercent",settings.positionY);
            settings.overlayOpacity=number(L"Display",L"OverlayOpacityPercent",settings.overlayOpacity);
            settings.sounds=number(L"Effects",L"Sounds",settings.sounds)!=0;
            settings.animations=number(L"Effects",L"Animations",settings.animations)!=0;
            settings.switchKey=number(L"Controls",L"SwitchWheelKey",settings.switchKey);
            settings.hotkey = number(L"Controls",L"Hotkey",settings.hotkey);
            settings.hotkeyModifier=number(L"Controls",L"HotkeyModifier",settings.hotkeyModifier);
            settings.actionHotkey=number(L"Controls",L"ActionHotkey",settings.actionHotkey);
            settings.actionModifier=number(L"Controls",L"ActionModifier",settings.actionModifier);
            settings.gamepadHotkey=number(L"Controls",L"GamepadHotkey",settings.gamepadHotkey);
            settings.gamepadModifier=number(L"Controls",L"GamepadModifier",settings.gamepadModifier);
            settings.gamepadActionModifier=number(L"Controls",L"GamepadActionModifier",settings.gamepadActionModifier);
            settings.gamepadCategoryButtons=number(L"Controls",L"GamepadCategoryButtons",settings.gamepadCategoryButtons);
            settings.gamepadMoveWhileOpen=number(L"Controls",L"GamepadMoveWhileOpen",settings.gamepadMoveWhileOpen)!=0;
            settings.swapMouseHands=number(L"Controls",L"SwapMouseHands",settings.swapMouseHands)!=0;
            settings.scale = number(L"Display",L"ScalePercent",static_cast<int>(settings.scale*100))/100.f;
            settings.sensitivity = number(L"Controls",L"SensitivityPercent",static_cast<int>(settings.sensitivity*100))/100.f;
            settings.font = Read(file,L"Display",L"Font",settings.font);
            Clamp(settings);return settings;
        }
        }
    void LoadSettings() {
        std::lock_guard lock(settingsMutex);
        // Startup is read-only: absence/partial INIs use struct defaults. Only
        // explicit Apply creates or updates a player's configuration file.
        const auto& path=SettingsFile();
        if(path.empty()) {settings=Settings{};Report("resolve",GetLastError());}
        else {
            const auto attributes=GetFileAttributesW(path.c_str());
            if(attributes==INVALID_FILE_ATTRIBUTES) {
                const auto error=GetLastError();settings=Settings{};
                Report(error==ERROR_FILE_NOT_FOUND || error==ERROR_PATH_NOT_FOUND ? "load-defaults" : "load-unavailable",error);
            }else if(attributes&FILE_ATTRIBUTE_DIRECTORY) {
                settings=Settings{};Report("load-unavailable",ERROR_DIRECTORY);
            }else {
                std::vector<char> bytes;SettingsIni::Document document;
                if(!ReadBytes(path,bytes)){settings=Settings{};Report("load-unavailable",GetLastError());}
                else if(!document.Parse(bytes)){settings=Settings{};Report("load-invalid",ERROR_INVALID_DATA);}
                else {
                    settings=ReadSettings(document);
                    const bool empty=bytes.empty() || (bytes.size()==2 &&
                        ((static_cast<unsigned char>(bytes[0])==0xFF && static_cast<unsigned char>(bytes[1])==0xFE) ||
                         (static_cast<unsigned char>(bytes[0])==0xFE && static_cast<unsigned char>(bytes[1])==0xFF))) ||
                        (bytes.size()==3 && bytes==std::vector<char>{'\xEF','\xBB','\xBF'});
                    Report(empty?"loaded-empty":"loaded");diagnostic+=" bytes="+std::to_string(bytes.size());
                }
            }
        }
        saved=settings;editing=false;
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
    static bool SaveSettingsLocked() { // settingsMutex held.
        // Serialize in memory, then write through the destination's own handle.
        // This preserves MO2 mappings and Vortex hardlinks while bypassing the
        // Windows profile API's cache/redirected view of staged INI files.
        const auto path=SettingsFile();
        if(path.empty())return Report("resolve",GetLastError());
        std::error_code error;
        std::filesystem::create_directories(path.parent_path(),error);
        if(error)return Report("create-directory",static_cast<DWORD>(error.value()));
        const HANDLE target=CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ,
            nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(target==INVALID_HANDLE_VALUE)return Report("open-target",GetLastError());
        Close close{target};std::vector<char> before;
        if(!ReadBytes(target,before))return Report("backup-read",GetLastError());
        SettingsIni::Document document;
        if(!document.Parse(before))return Report("parse-original",ERROR_INVALID_DATA);
        struct Field {std::wstring section,key,value;};
        std::vector<Field> fields;
        auto write = [&](const wchar_t* section,const wchar_t* key,const std::string& value) {
            std::wstring wide;
            if(!SettingsIni::Wide(value,wide) || !document.Set(section,key,wide))return false;
            fields.push_back({section,key,std::move(wide)});return true;
        };
        const bool ok = write(L"General",L"Enabled",settings.enabled?"1":"0") &&
            write(L"Display",L"Font",settings.font) &&
            write(L"General",L"Language",settings.language) &&
            write(L"General",L"AllInventory",settings.allInventory?"1":"0") &&
            write(L"General",L"RememberLastWheel",settings.rememberLastWheel?"1":"0") &&
            write(L"General",L"HideEmptyCategories",settings.hideEmptyCategories?"1":"0") &&
            write(L"General",L"KeepOpenConsumables",settings.keepOpenConsumables?"1":"0") &&
            write(L"General",L"KeepOpen",settings.keepOpen?"1":"0") &&
            write(L"General",L"TimeMode",std::to_string(settings.timeMode)) &&
            write(L"General",L"SlowTimePercent",std::to_string(settings.slowPercent)) &&
            write(L"Display",L"Theme",settings.theme) &&
            write(L"Display",L"ShowHints",settings.showHints?"1":"0") &&
            write(L"Display",L"ShowActionNotifications",settings.showActionNotifications?"1":"0") &&
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
            write(L"Controls",L"GamepadCategoryButtons",std::to_string(settings.gamepadCategoryButtons)) &&
            write(L"Controls",L"GamepadMoveWhileOpen",settings.gamepadMoveWhileOpen?"1":"0") &&
            write(L"Controls",L"SwapMouseHands",settings.swapMouseHands?"1":"0") &&
            write(L"Controls",L"SensitivityPercent",std::to_string(static_cast<int>(settings.sensitivity*100+.5f))) &&
            write(L"Display",L"ScalePercent",std::to_string(static_cast<int>(settings.scale*100+.5f)));
        if(!ok)return Report("serialize",ERROR_INVALID_DATA);
        auto persisted=settings;
        persisted.scale=std::round(settings.scale*100)/100;
        persisted.wheelScale=std::round(settings.wheelScale*100)/100;
        persisted.sensitivity=std::round(settings.sensitivity*100)/100;
        std::vector<char> bytes;
        if(!document.Encode(bytes) || bytes.size()<=2 || bytes.size()>maxIniBytes)return Report("serialize",ERROR_INVALID_DATA);
        SettingsIni::Document verifiedDocument;
        if(!verifiedDocument.Parse(bytes) || ReadSettings(verifiedDocument)!=persisted)return Report("verify-document",ERROR_INVALID_DATA);
        // Defaults are not evidence that a key was written. Require every
        // explicit field in the serialized bytes, even when all values default.
        for(const auto& field:fields)if(verifiedDocument.Get(field.section,field.key)!=field.value)
            return Report("verify-document-key",ERROR_INVALID_DATA);
        auto writeBytes=[&](const std::vector<char>& data) {
            LARGE_INTEGER start{};DWORD written=0;
            return SetFilePointerEx(target,start,nullptr,FILE_BEGIN) &&
                WriteFile(target,data.data(),static_cast<DWORD>(data.size()),&written,nullptr) && written==data.size() &&
                SetEndOfFile(target) && FlushFileBuffers(target);
        };
        if(!writeBytes(bytes)) {
            const auto failed=GetLastError();
            const bool restored=writeBytes(before);
            return Report(restored?"write-target-rolled-back":"write-target-rollback-failed",failed);
        }
        // Verify persisted bytes via the same destination handle, bypassing the
        // Windows profile cache. A fresh process is also covered by tests.
        std::vector<char> actual;
        if(!ReadBytes(target,actual) || actual!=bytes) {
            const bool restored=writeBytes(before);
            return Report(restored?"verify-target-rolled-back":"verify-target-rollback-failed",ERROR_INVALID_DATA);
        }
        if(!CloseHandle(target))return Report("close-target",GetLastError());
        close.handle=INVALID_HANDLE_VALUE;
        std::vector<char> reopened;
        if(!ReadBytes(path,reopened))return Report("reopen-target",GetLastError());
        if(reopened!=bytes) {
            Report("verify-reopened-target",ERROR_INVALID_DATA);
            diagnostic+=" expected_bytes="+std::to_string(bytes.size())+" actual_bytes="+std::to_string(reopened.size());return false;
        }
        settings=persisted;saved=settings;editing=false;
        Report("saved-verified");diagnostic+=" bytes="+std::to_string(bytes.size());return true;
    }
    bool SaveSettings() {
        std::lock_guard lock(settingsMutex);return SaveSettingsLocked();
    }
    bool SaveExternalSettings(const Settings& expected,Settings values) {
        std::lock_guard lock(settingsMutex);
        if(editing || settings!=expected)return Report("external-settings-conflict",ERROR_BUSY);
        // Startup-only settings are not exposed by the optional menu.
        values.enabled=settings.enabled;values.font=settings.font;Clamp(values);
        const auto before=settings;settings=std::move(values);
        if(SaveSettingsLocked())return true;
        settings=before;return false;
    }

}
