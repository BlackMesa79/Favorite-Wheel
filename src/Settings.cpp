#include "Settings.h"
#include <Windows.h>
#include <algorithm>
#include <filesystem>
#include <mutex>
#include <fstream>
#include <vector>
#include <cmath>

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
            diagnostic="path='"+UTF8(settingsPath)+"' stage="+operation+" win32="+std::to_string(error);
            return false;
        }
        std::string Read(const std::filesystem::path& path,const wchar_t* section, const wchar_t* key, const std::string& fallback) {
            wchar_t value[2048]{};
            const auto file = path.wstring();
            GetPrivateProfileStringW(section, key, L"", value, 2048, file.c_str());
            if (!*value) return fallback;
            const int size = WideCharToMultiByte(CP_UTF8,0,value,-1,nullptr,0,nullptr,nullptr);
            std::string result(size, '\0');
            WideCharToMultiByte(CP_UTF8,0,value,-1,result.data(),size,nullptr,nullptr);
            result.pop_back(); return result;
        }
        void Clamp(Settings& v) {
            v.gamepadCategoryButtons=std::clamp(v.gamepadCategoryButtons,0,1);
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
    void SetSettingsPath(const std::string& path) {
        std::lock_guard lock(settingsMutex);
        settingsPath=path.empty()?std::filesystem::path{}:std::filesystem::absolute(std::filesystem::u8path(path));
    }
    std::string SettingsDiagnostic() {std::lock_guard lock(settingsMutex);return diagnostic;}
    namespace {
        Settings ReadSettings(const std::filesystem::path& file) {
            const auto path=file.wstring();
            Settings settings;
            auto number = [&](const wchar_t* section, const wchar_t* key, int value) {
                return static_cast<int>(GetPrivateProfileIntW(section,key,value,path.c_str()));
            };
            settings.enabled = number(L"General",L"Enabled",1) != 0;
            settings.allInventory = number(L"General",L"AllInventory",0) != 0;
            settings.keepOpen = number(L"General",L"KeepOpen",1) != 0;
            settings.timeMode=number(L"General",L"TimeMode",0);
            settings.slowPercent=number(L"General",L"SlowTimePercent",20);
            const auto legacyLanguage = Read(file,L"General",L"Chinese","");
            settings.language = Read(file,L"General",L"Language",legacyLanguage.empty() ? "auto" :
                (number(L"General",L"Chinese",1) ? "zh_CN" : "en"));
            settings.theme = Read(file,L"Display",L"Theme","classic");
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
            settings.gamepadCategoryButtons=number(L"Controls",L"GamepadCategoryButtons",0);
            settings.gamepadMoveWhileOpen=number(L"Controls",L"GamepadMoveWhileOpen",0)!=0;
            settings.scale = number(L"Display",L"ScalePercent",100)/100.f;
            settings.sensitivity = number(L"Controls",L"SensitivityPercent",100)/100.f;
            settings.font = Read(file,L"Display",L"Font",settings.font);
            Clamp(settings);return settings;
        }
        }
    void LoadSettings() {
        std::lock_guard lock(settingsMutex);
        const auto& path=SettingsFile();
        if(path.empty()) {settings=Settings{};Report("resolve",GetLastError());}
        else {
            settings=ReadSettings(path);
            const auto attributes=GetFileAttributesW(path.c_str());
            Report(attributes==INVALID_FILE_ATTRIBUTES?"load-defaults":"loaded",
                attributes==INVALID_FILE_ATTRIBUTES?GetLastError():ERROR_SUCCESS);
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
    bool SaveSettings() {
        std::lock_guard lock(settingsMutex);
        // Stage away from the virtual Data tree, then write through the final
        // INI's own handle. Renaming a new virtual .tmp onto an existing mapped
        // INI can use different backing destinations in mod managers.
        const auto path=SettingsFile();
        if(path.empty())return Report("resolve",GetLastError());
        std::error_code error;
        std::filesystem::create_directories(path.parent_path(),error);
        if(error)return Report("create-directory",static_cast<DWORD>(error.value()));
        const auto tempDir=std::filesystem::temp_directory_path(error);
        if(error)return Report("temp-directory",static_cast<DWORD>(error.value()));
        wchar_t tempName[MAX_PATH]{};
        if(!GetTempFileNameW(tempDir.c_str(),L"FWh",0,tempName))return Report("create-stage",GetLastError());
        const std::filesystem::path temporary=tempName;
        struct Cleanup {
            std::filesystem::path file;
            ~Cleanup(){WritePrivateProfileStringW(nullptr,nullptr,nullptr,file.c_str());DeleteFileW(file.c_str());}
        } cleanup{temporary};
        const auto attributes=GetFileAttributesW(path.c_str());
        if(attributes!=INVALID_FILE_ATTRIBUTES) {
            if(!CopyFileW(path.c_str(),temporary.c_str(),FALSE))return Report("copy-stage",GetLastError());
            // The original may be read-only: staging can still be edited, while
            // the final write must fail honestly and preserve that original.
            SetFileAttributesW(temporary.c_str(),FILE_ATTRIBUTE_NORMAL);
        }else {
            const auto missing=GetLastError();
            if(missing!=ERROR_FILE_NOT_FOUND && missing!=ERROR_PATH_NOT_FOUND)return Report("inspect",missing);
            std::ofstream file(temporary,std::ios::binary|std::ios::trunc);
            file.put(static_cast<char>(0xFF));file.put(static_cast<char>(0xFE));
            if(!file)return Report("initialize-stage",ERROR_WRITE_FAULT);
        }
        auto write = [&](const wchar_t* section,const wchar_t* key,const std::string& value) {
            const int size=MultiByteToWideChar(CP_UTF8,0,value.c_str(),-1,nullptr,0);
            std::wstring wide(size,L'\0');
            MultiByteToWideChar(CP_UTF8,0,value.c_str(),-1,wide.data(),size);
            return WritePrivateProfileStringW(section,key,wide.c_str(),temporary.c_str()) != 0;
        };
        const bool ok = write(L"General",L"Language",settings.language) &&
            write(L"General",L"AllInventory",settings.allInventory?"1":"0") &&
            write(L"General",L"KeepOpen",settings.keepOpen?"1":"0") &&
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
            write(L"Controls",L"GamepadCategoryButtons",std::to_string(settings.gamepadCategoryButtons)) &&
            write(L"Controls",L"GamepadMoveWhileOpen",settings.gamepadMoveWhileOpen?"1":"0") &&
            write(L"Controls",L"SensitivityPercent",std::to_string(static_cast<int>(settings.sensitivity*100+.5f))) &&
            write(L"Display",L"ScalePercent",std::to_string(static_cast<int>(settings.scale*100+.5f)));
        if(!ok)return Report("edit-stage",GetLastError());
        WritePrivateProfileStringW(nullptr,nullptr,nullptr,temporary.c_str());
        auto persisted=settings;
        persisted.scale=std::round(settings.scale*100)/100;
        persisted.wheelScale=std::round(settings.wheelScale*100)/100;
        persisted.sensitivity=std::round(settings.sensitivity*100)/100;
        if(ReadSettings(temporary)!=persisted)return Report("verify-stage",ERROR_INVALID_DATA);
        std::ifstream input(temporary,std::ios::binary);
        const std::vector<char> bytes((std::istreambuf_iterator<char>(input)),{});
        if(!input || bytes.empty() || bytes.size()>2*1024*1024)return Report("read-stage",ERROR_READ_FAULT);
        // OPEN_ALWAYS retains the old bytes until the staged document is ready.
        // Existing mod-managed destinations are opened directly, never renamed.
        const HANDLE target=CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ,
            nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(target==INVALID_HANDLE_VALUE)return Report("open-target",GetLastError());
        struct Close {HANDLE handle;~Close(){if(handle!=INVALID_HANDLE_VALUE)CloseHandle(handle);}} close{target};
        LARGE_INTEGER length{};
        if(!GetFileSizeEx(target,&length) || length.QuadPart>2*1024*1024)return Report("backup-size",ERROR_FILE_TOO_LARGE);
        std::vector<char> before(static_cast<std::size_t>(length.QuadPart));DWORD read=0;
        if(!before.empty() && (!ReadFile(target,before.data(),static_cast<DWORD>(before.size()),&read,nullptr) || read!=before.size()))
            return Report("backup-read",GetLastError());
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
        LARGE_INTEGER start{};DWORD verified=0;std::vector<char> actual(bytes.size());
        if(!SetFilePointerEx(target,start,nullptr,FILE_BEGIN) ||
            !ReadFile(target,actual.data(),static_cast<DWORD>(actual.size()),&verified,nullptr) || verified!=actual.size() || actual!=bytes) {
            const bool restored=writeBytes(before);
            return Report(restored?"verify-target-rolled-back":"verify-target-rollback-failed",ERROR_INVALID_DATA);
        }
        // Refresh Win32's cached view of the original INI only after closing it.
        if(!CloseHandle(target))return Report("close-target",GetLastError());
        close.handle=INVALID_HANDLE_VALUE;
        WritePrivateProfileStringW(nullptr,nullptr,nullptr,path.c_str());
        std::ifstream reopen(path,std::ios::binary);
        const std::vector<char> reopened((std::istreambuf_iterator<char>(reopen)),{});
        if(!reopen || reopened!=bytes || ReadSettings(path)!=persisted)return Report("verify-reopened-target",ERROR_INVALID_DATA);
        settings=persisted;saved=settings;editing=false;
        Report("saved-verified");return true;
    }
}
