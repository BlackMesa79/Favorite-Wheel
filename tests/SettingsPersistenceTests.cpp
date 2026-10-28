#include "Settings.h"
#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <tuple>
#include <algorithm>
#include <stdexcept>

namespace fs=std::filesystem;
void Require(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
std::string Utf8(const fs::path& path) {const auto s=path.u8string();return {reinterpret_cast<const char*>(s.data()),s.size()};}
std::string Bytes(const fs::path& path) {std::ifstream in(path,std::ios::binary);return {(std::istreambuf_iterator<char>(in)),{}};}

// Model an INI API that acknowledges/cache-reads writes without persisting them.
// This is fault injection, not a claim about a particular manager's hooks.
using CacheKey=std::tuple<std::wstring,std::wstring,std::wstring>;
std::map<CacheKey,std::wstring> profileCache;
BOOL WINAPI CachedWrite(LPCWSTR section,LPCWSTR key,LPCWSTR value,LPCWSTR path) {
    if(section && key && value)profileCache[{path,section,key}]=value;
    return TRUE;
}
DWORD WINAPI CachedRead(LPCWSTR section,LPCWSTR key,LPCWSTR fallback,LPWSTR output,DWORD size,LPCWSTR path) {
    const auto found=profileCache.find({path,section?section:L"",key?key:L""});
    const std::wstring value=found==profileCache.end()?(fallback?fallback:L""):found->second;
    if(!size)return 0;
    const auto n=(std::min)(value.size(),static_cast<std::size_t>(size-1));
    std::copy_n(value.data(),n,output);output[n]=0;return static_cast<DWORD>(n);
}
UINT WINAPI CachedInt(LPCWSTR section,LPCWSTR key,INT fallback,LPCWSTR path) {
    const auto found=profileCache.find({path,section,key});
    return found==profileCache.end()?fallback:static_cast<UINT>(std::wcstol(found->second.c_str(),nullptr,10));
}
void InstallProfileFault() {
    const auto base=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    const auto dos=reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    const auto nt=reinterpret_cast<IMAGE_NT_HEADERS*>(base+dos->e_lfanew);
    const auto entry=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    for(auto dll=reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base+entry.VirtualAddress);dll->Name;++dll) {
        if(!dll->OriginalFirstThunk)continue;
        auto names=reinterpret_cast<IMAGE_THUNK_DATA*>(base+dll->OriginalFirstThunk);
        auto imports=reinterpret_cast<IMAGE_THUNK_DATA*>(base+dll->FirstThunk);
        for(;names->u1.AddressOfData;++names,++imports) {
            if(IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal))continue;
            const auto name=reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base+names->u1.AddressOfData)->Name;
            ULONG_PTR replacement=0;
            if(std::string_view(name)=="WritePrivateProfileStringW")replacement=reinterpret_cast<ULONG_PTR>(&CachedWrite);
            if(std::string_view(name)=="GetPrivateProfileStringW")replacement=reinterpret_cast<ULONG_PTR>(&CachedRead);
            if(std::string_view(name)=="GetPrivateProfileIntW")replacement=reinterpret_cast<ULONG_PTR>(&CachedInt);
            if(!replacement)continue;
            DWORD protection=0,ignored=0;
            Require(VirtualProtect(&imports->u1.Function,sizeof(ULONG_PTR),PAGE_READWRITE,&protection)!=0,"patch IAT");
            imports->u1.Function=replacement;
            Require(VirtualProtect(&imports->u1.Function,sizeof(ULONG_PTR),protection,&ignored)!=0,"restore IAT protection");
        }
    }
}
int wmain(int argc,wchar_t** argv) try {
    using namespace Wheel;
    if(argc==3 && std::wstring_view(argv[1])==L"--verify-file") {
        SetSettingsPath(Utf8(argv[2]));LoadSettings();const auto value=Config();
        Require(value.theme=="skyrim" && value.positionX==61 && value.font==Utf8(L"D:/字体/𠀀.ttf"),"fresh-process disk settings");
        // A separate reader must also see explicit default and changed keys.
        Require(GetPrivateProfileIntW(L"General",L"Enabled",-999,argv[2])==1,"explicit default key on disk");
        Require(GetPrivateProfileIntW(L"Display",L"PositionXPercent",-999,argv[2])==61,"external reader sees saved position");
        wchar_t font[256]{};GetPrivateProfileStringW(L"Display",L"Font",L"",font,256,argv[2]);
        Require(std::wstring(font)==L"D:/字体/𠀀.ttf","external reader sees Unicode font");return 0;
    }
    InstallProfileFault();
    const auto root=fs::absolute(fs::path("build")/("settings-persistence-"+std::to_string(GetCurrentProcessId())));
    fs::create_directories(root);
    const auto sentinel=root/"cache-only.ini";
    Require(WritePrivateProfileStringW(L"Test",L"Value",L"17",sentinel.c_str())!=0,"fake API accepts a write");
    Require(GetPrivateProfileIntW(L"Test",L"Value",0,sentinel.c_str())==17 && !fs::exists(sentinel),"profile cache fault is active");
    for(const auto mode:{"missing","zero","bom"}) {
        const auto path=root/(std::string(mode)+".ini");
        if(std::string_view(mode)!="missing") {
            std::ofstream file(path,std::ios::binary);
            if(std::string_view(mode)=="bom")file<<"\xFF\xFE";
        }
        SetSettingsPath(Utf8(path));LoadSettings();
        if(std::string_view(mode)=="bom")Require(SettingsDiagnostic().find("loaded-empty")!=std::string::npos,"BOM-only INI is diagnosed as empty");
        BeginSettings();const bool defaultsSaved=SaveSettings();
        std::cout<<mode<<": result="<<defaultsSaved<<" "<<SettingsDiagnostic()<<'\n';
        Require(defaultsSaved && Bytes(path).size()>2,"Default Apply must persist actual keys even when profile API writes never reach disk");
        BeginSettings();auto changed=Config();changed.theme="skyrim";changed.positionX=61;changed.font=Utf8(L"D:/字体/𠀀.ttf");EditSettings(changed);
        Require(SaveSettings(),"Changed Apply must not depend on profile caches");
        LoadSettings();Require(Config()==changed,"Changed settings reload from file bytes with a misleading profile cache");
        wchar_t executable[32768]{};Require(GetModuleFileNameW(nullptr,executable,32768)!=0,"find executable");
        std::wstring command=L"\""+std::wstring(executable)+L"\" --verify-file \""+path.wstring()+L"\"";
        STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION process{};
        Require(CreateProcessW(executable,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&process)!=0,"launch independent reader");
        const auto waited=WaitForSingleObject(process.hProcess,30000);DWORD code=1;GetExitCodeProcess(process.hProcess,&code);
        CloseHandle(process.hThread);CloseHandle(process.hProcess);
        Require(waited==WAIT_OBJECT_0 && code==0,"independent process and Windows INI API read the complete document");
    }
    // Vortex normally deploys hardlinks. Updating Data must retain the link to
    // staging; replacing/renaming the destination would break that connection.
    const auto staging=root/"vortex-staging.ini",deployed=root/"vortex-data.ini";
    {std::ofstream file(staging,std::ios::binary);file<<"; retained\n[Custom]\nKeep=123\n";}
    Require(CreateHardLinkW(deployed.c_str(),staging.c_str(),nullptr)!=0,"create deployment hardlink");
    SetSettingsPath(Utf8(deployed));LoadSettings();BeginSettings();auto v=Config();v.theme="polygon";EditSettings(v);
    Require(SaveSettings() && Bytes(staging)==Bytes(deployed) && Bytes(staging).find("Theme=polygon")!=std::string::npos,"Data save updates the same staging hardlink");
    Require(Bytes(staging).find("; retained\n[Custom]\nKeep=123\n")!=std::string::npos,"hardlink save preserves comments and unknown keys");
    // Player-owned documents can predate the Unicode writer or come from a
    // text editor. Reading/saving must preserve their other data and encoding.
    for(const auto format:{"utf8","utf8-bom","utf16le","utf16be","ansi"}) {
        const auto path=root/(std::string(format)+".ini");
        const std::wstring text=L"; café retained\r\n[Custom]\r\nKeep=123\r\n[ display ]\r\ntheme = 'skyrim'\r\nPositionXPercent=61\r\n[Display]\r\nTheme=classic\r\nFont=D:/Fonts/old.ttf\r\n";
        std::string original;
        const std::string_view type=format;
        if(type=="utf16le" || type=="utf16be") {
            const bool little=type=="utf16le";
            original=little?"\xFF\xFE":"\xFE\xFF";
            for(const auto c:text){original.push_back(static_cast<char>(little?c&255:c>>8));original.push_back(static_cast<char>(little?c>>8:c&255));}
        }else {
            const auto page=type=="ansi"?CP_ACP:CP_UTF8;
            const auto size=WideCharToMultiByte(page,0,text.data(),static_cast<int>(text.size()),nullptr,0,nullptr,nullptr);
            Require(size>0,"encode existing document");original.resize(size);
            WideCharToMultiByte(page,0,text.data(),static_cast<int>(text.size()),original.data(),size,nullptr,nullptr);
            if(type=="utf8-bom")original="\xEF\xBB\xBF"+original;
        }
        {std::ofstream file(path,std::ios::binary);file.write(original.data(),original.size());}
        SetSettingsPath(Utf8(path));LoadSettings();Require(Config().theme=="skyrim" && Config().positionX==61,"legacy encoding, casing and quoted values read from bytes");
        BeginSettings();auto changed=Config();changed.theme="polygon";changed.font=Utf8(L"D:/字体/𠀀.ttf");EditSettings(changed);
        Require(SaveSettings(),"legacy document accepts Unicode fields");LoadSettings();Require(Config()==changed,"updated duplicate keys and Unicode round trip");
        const auto result=Bytes(path);
        if(type=="utf8" || type=="utf8-bom")Require(result.find("; café retained\r\n[Custom]\r\nKeep=123\r\n")!=std::string::npos,"UTF-8 unrelated content preserved exactly");
        if(type=="utf8-bom")Require(result.starts_with("\xEF\xBB\xBF"),"UTF-8 BOM preserved");
        if(type=="utf16le")Require(result.starts_with("\xFF\xFE"),"UTF-16LE encoding preserved");
        if(type=="utf16be")Require(result.starts_with("\xFE\xFF"),"UTF-16BE encoding preserved");
    }
    const auto malformed=root/"malformed.ini";
    const std::string invalid("\xFF\xFE\x41",3);
    {std::ofstream file(malformed,std::ios::binary);file.write(invalid.data(),invalid.size());}
    SetSettingsPath(Utf8(malformed));LoadSettings();Require(SettingsDiagnostic().find("load-invalid")!=std::string::npos,"invalid Unicode is diagnosed");
    BeginSettings();Require(!SaveSettings() && Bytes(malformed)==invalid,"invalid player document is preserved on failure");
    const auto rejected=root/"bad-value.ini";
    {std::ofstream file(rejected);file<<"; preserve\n[General]\nEnabled=1\n";}
    const auto before=Bytes(rejected);
    SetSettingsPath(Utf8(rejected));LoadSettings();BeginSettings();auto bad=Config();bad.font="x\n[General]\nEnabled=0";EditSettings(bad);
    Require(!SaveSettings() && Bytes(rejected)==before,"invalid multiline value cannot damage existing configuration");
    std::cout<<"INI profile-cache, independent-process and deployment hardlink regressions passed\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
