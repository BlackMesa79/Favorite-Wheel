#include "Settings.h"
#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;
std::string Utf8(const fs::path& path) {
    const auto text=path.u8string();return {reinterpret_cast<const char*>(text.data()),text.size()};
}
void Require(bool success,const char* stage) {
    if(!success)throw std::runtime_error(std::string(stage)+" win32="+std::to_string(GetLastError()));
}
Wheel::Settings Expected(bool unicodeFont) {
    auto value=Wheel::Settings{};
    value.language="zh_CN";value.theme="frost";value.positionX=64;value.positionY=32;
    value.keepOpen=false;value.kbMoveWhileOpen=true;value.kbCategoryPrevKey=46;value.kbCategoryNextKey=44;
    if(unicodeFont)value.font=Utf8(L"D:/字体/𠀀.ttf");
    return value;
}
int wmain(int argc,wchar_t** argv) try {
    if(argc==4 && std::wstring_view(argv[1])==L"--worker") {
        Wheel::SetSettingsPath(Utf8(argv[3]));Wheel::LoadSettings();
        const auto target=std::wstring_view(argv[3]);
        const bool defaults=target.find(L"defaults")!=target.npos;
        const auto expected=defaults?Wheel::Settings{}:Expected(target.find(L"empty-unicode")!=target.npos);
        if(std::wstring_view(argv[2])==L"save") {
            Wheel::BeginSettings();Wheel::EditSettings(expected);
            const bool saved=Wheel::SaveSettings();
            std::cout<<"Apply "<<Wheel::SettingsDiagnostic()<<'\n';
            std::ofstream report(fs::path(argv[3]).parent_path().parent_path().parent_path().parent_path()/"worker-save.txt");
            report<<Wheel::SettingsDiagnostic()<<'\n';
            if(!saved)return 2;
            Wheel::LoadSettings();
        }
        Require(Wheel::Config()==expected,"persisted fields");
        return 0;
    }
    if(argc!=2)throw std::runtime_error("Usage: SettingsVfsTests <MO2 usvfs_x64.dll>");
    const auto library=LoadLibraryExW(fs::absolute(argv[1]).c_str(),nullptr,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    Require(library!=nullptr,"load usvfs");
    auto address=[&](const char* name){const auto found=GetProcAddress(library,name);Require(found!=nullptr,name);return found;};
    const auto parameters=reinterpret_cast<void*(*)()>(address("usvfsCreateParameters"))();
    const auto setName=reinterpret_cast<void(*)(void*,const char*)>(address("usvfsSetInstanceName"));
    const auto create=reinterpret_cast<BOOL(WINAPI*)(const void*)>(address("usvfsCreateVFS"));
    const auto clear=reinterpret_cast<void(WINAPI*)()>(address("usvfsClearVirtualMappings"));
    const auto link=reinterpret_cast<BOOL(WINAPI*)(LPCWSTR,LPCWSTR,unsigned)>(address("usvfsVirtualLinkDirectoryStatic"));
    const auto launch=reinterpret_cast<decltype(&CreateProcessW)>(address("usvfsCreateProcessHooked"));
    const auto disconnect=reinterpret_cast<void(WINAPI*)()>(address("usvfsDisconnectVFS"));
    const auto freeParameters=reinterpret_cast<void(*)(void*)>(address("usvfsFreeParameters"));
    reinterpret_cast<void(WINAPI*)(bool)>(address("usvfsInitLogging"))(false);
    const auto instance="FavoriteWheel-settings-test-"+std::to_string(GetCurrentProcessId());
    setName(parameters,instance.c_str());Require(create(parameters),"create isolated VFS");
    const auto root=fs::absolute(fs::path("build")/instance);
    wchar_t executable[32768]{};Require(GetModuleFileNameW(nullptr,executable,32768)!=0,"test executable");
    auto worker=[&](const wchar_t* operation,const fs::path& target) {
        std::wstring command=L"\""+std::wstring(executable)+L"\" --worker "+operation+L" \""+target.wstring()+L"\"";
        STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION process{};
        Require(launch(executable,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,
            fs::current_path().c_str(),&startup,&process),"launch hooked worker");
        const auto waited=WaitForSingleObject(process.hProcess,30000);
        DWORD code=1;GetExitCodeProcess(process.hProcess,&code);
        CloseHandle(process.hThread);CloseHandle(process.hProcess);
        Require(waited==WAIT_OBJECT_0,"worker timeout");return code;
    };
    for(const auto mode:{"missing","empty","existing","empty-unicode","overwrite-empty","missing-defaults","bom-defaults","readonly"}) {
        clear();
        const auto fixture=root/mode,virtualDir=fixture/"virtual"/"SKSE"/"Plugins",
            mod=fixture/"mod",overwrite=fixture/"overwrite";
        fs::create_directories(virtualDir);fs::create_directories(mod);fs::create_directories(overwrite);
        if(std::string_view(mode)!="missing" && std::string_view(mode)!="missing-defaults" && std::string_view(mode)!="overwrite-empty") {
            std::ofstream initial(mod/"FavoriteWheel.ini",std::ios::binary);
            if(std::string_view(mode)=="existing")initial<<"; retain this comment\n[Custom]\nKeep=123\n";
            if(std::string_view(mode)=="bom-defaults")initial<<"\xFF\xFE";
        }
        if(std::string_view(mode)=="overwrite-empty") {std::ofstream initial(overwrite/"FavoriteWheel.ini");}
        if(std::string_view(mode)=="readonly")Require(SetFileAttributesW((mod/"FavoriteWheel.ini").c_str(),FILE_ATTRIBUTE_READONLY),"readonly fixture");
        Require(link(mod.c_str(),virtualDir.c_str(),8),"map mod directory");
        Require(link(overwrite.c_str(),virtualDir.c_str(),8|4),"map overwrite creation target");
        const auto target=virtualDir/"FavoriteWheel.ini";
        const auto save=worker(L"save",target);
        const auto read=std::string_view(mode)=="readonly"?STILL_ACTIVE:worker(L"read",target);
        const auto original=mod/"FavoriteWheel.ini",created=overwrite/"FavoriteWheel.ini";
        const auto modSize=fs::exists(original)?fs::file_size(original):0,
            overwriteSize=fs::exists(created)?fs::file_size(created):0;
        std::cout<<mode<<": save_exit="<<save<<" fresh_read_exit="<<(read==STILL_ACTIVE?"skipped":std::to_string(read))
            <<" mod_bytes="<<modSize<<" overwrite_bytes="<<overwriteSize<<'\n';
        if(std::string_view(mode)=="readonly") {
            Require(save==2 && modSize==0 && !fs::exists(created),"read-only failure preserves the original empty INI");
            Require(SetFileAttributesW(original.c_str(),FILE_ATTRIBUTE_NORMAL),"restore fixture permissions");
            continue;
        }
        Require(save==0 && read==0,"MO2-compatible Apply and fresh-process reload");
        Require(modSize+overwriteSize>2,"physical INI contains more than a BOM even when every setting is default");
        if(std::string_view(mode)=="existing") {
            std::ifstream content(modSize?original:created,std::ios::binary);
            const std::string bytes((std::istreambuf_iterator<char>(content)),{});
            Require(bytes.find("Keep=123")!=std::string::npos,"unknown key preservation");
        }
    }
    clear();disconnect();freeParameters(parameters);
    std::cout<<"Isolated usvfs settings persistence passed; fixtures: "<<root.string()<<'\n';
    return 0;
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
