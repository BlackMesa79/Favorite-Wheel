#include "IconCache.h"
#include "Settings.h"
#include "I4Runtime.h"
#include <Windows.h>
#include <Aclapi.h>
#include <fstream>
#include <array>
#include <iostream>
#include <random>
#include <stdexcept>

namespace {
using namespace Wheel;
namespace fs=std::filesystem;
unsigned checks=0;
void Check(bool ok,const char* text){++checks;if(!ok)throw std::runtime_error(text);}
std::vector<std::uint8_t> Read(const fs::path& path) {
    std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};
}
void Write(const fs::path& path,const std::vector<std::uint8_t>& bytes) {
    std::ofstream f(path,std::ios::binary|std::ios::trunc);f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());
    Check(bool(f),"write test fixture");
}
std::string Hash(std::string_view source) {return IconCache::Hash({reinterpret_cast<const std::uint8_t*>(source.data()),source.size()});}
Swf::Bitmap Pixels(unsigned edge,bool random=false) {
    Swf::Bitmap result{edge,edge,std::vector<std::uint8_t>(edge*edge*4,123)};
    if(random){std::mt19937 rng(42);for(auto& b:result.rgba)b=static_cast<std::uint8_t>(rng());}
    return result;
}
void U32(std::vector<std::uint8_t>& bytes,std::size_t at,std::uint32_t value) {
    for(unsigned n=0;n<4;++n)bytes.at(at+n)=static_cast<std::uint8_t>(value>>(n*8));
}
void Basic(const fs::path& root) {
    const auto directory=root/"basic";const auto hash=Hash("abc");
    Check(hash=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad","BCrypt SHA-256 known answer");
    IconCache::Key key{"Icon \n",Swf::Variant::Original,64};const auto bitmap=Pixels(64);
    unsigned renders=0,warnings=0;
    auto load=[&](IconCache::Store& cache,const std::string& digest,const IconCache::Key& k) {
        if(auto found=cache.Get(digest,k))return *found;
        ++renders;auto image=Pixels(k.edge);cache.Put(digest,k,image);return image;
    };
    {IconCache::Store first(directory);first.Configure(true,64);load(first,hash,key);first.Flush();}
    {IconCache::Store second(directory);second.Configure(true,64);Check(load(second,hash,key).rgba==bitmap.rgba && renders==1,"independent cache instance skips render");
        for(const auto& k:std::vector<IconCache::Key>{{"icon \n",Swf::Variant::Original,64},{"Icon \n",Swf::Variant::Mask,64},{"Icon \n",Swf::Variant::Original,128},{"Icon",Swf::Variant::Original,64}})
            load(second,hash,k);
        Check(renders==5,"raw label, variant and edge distinguish entries");second.Flush();}
    {IconCache::Store third(directory);third.Configure(true,64);Check(bool(third.Get(hash,key)),"merged file retains first entry");
        Check(Read(directory/(hash+".fwic"))[12]==5,"same hash has five merged entries");
        load(third,Hash("abcd"),key);Check(renders==6,"source byte change causes miss and rebuild");}
    {IconCache::Store changed(directory,[&](const auto&){++warnings;},IconCache::converterVersion+1);changed.Configure(true,64);
        load(changed,hash,key);load(changed,hash,key);Check(renders==7 && warnings==1,"converter increment discards old file once and rebuilds");changed.Flush();}
    {IconCache::Store reload(directory,{},IconCache::converterVersion+1);reload.Configure(true,64);Check(bool(reload.Get(hash,key)),"rebuilt converter version persists");}
    std::cout<<"PASS restart/key/merge/converter\n";
}
void Validation(const fs::path& root) {
    const auto directory=root/"validation";const auto hash=Hash("validation");const auto file=directory/(hash+".fwic");
    IconCache::Key a{"a",Swf::Variant::Original,16},b{"b",Swf::Variant::Mask,16};const auto bitmap=Pixels(16);
    {IconCache::Store seed(directory);seed.Configure(true,64);seed.Put(hash,a,bitmap);seed.Put(hash,b,bitmap);seed.Flush();}
    const auto good=Read(file);
    unsigned cases=0;
    auto reject=[&](const char* name,const std::vector<std::uint8_t>& bytes,bool oversized=false) {
        Write(file,bytes);if(oversized)fs::resize_file(file,IconCache::maxFileBytes+1);
        unsigned warnings=0,renders=0;
        IconCache::Store cache(directory,[&](const auto&){++warnings;});cache.Configure(true,512);
        Check(!cache.Get(hash,a),name);Check(!cache.Get(hash,b),"invalid file discards even other good entry");
        Check(!fs::exists(file) && warnings==1,"invalid file removed and only one warning");
        for(unsigned n=0;n<2;++n)if(!cache.Get(hash,a)){++renders;cache.Put(hash,a,bitmap);}
        Check(renders==1 && warnings==1,"one conversion rebuilds and repeated read hits without warning");
        // 同一进程同一哈希再次遇到损坏，也不重复 warning。
        const auto other=Hash("other");cache.Get(other,a);Write(file,bytes);if(oversized)fs::resize_file(file,IconCache::maxFileBytes+1);
        Check(!cache.Get(hash,a) && warnings==1,"same invalid hash warning is deduplicated");
        ++cases;std::cout<<"PASS invalid "<<name<<'\n';
    };
    for(const auto length:{std::size_t(0),std::size_t(3),std::size_t(19),std::size_t(20),std::size_t(59),good.size()-1})
        reject("truncation",{good.begin(),good.begin()+length});
    auto mutate=[&](const char* name,std::size_t at,std::uint32_t value){auto bytes=good;U32(bytes,at,value);reject(name,bytes);};
    mutate("magic",0,0);mutate("format",4,2);mutate("converter",8,IconCache::converterVersion+1);
    mutate("zero entries",12,0);mutate("entry limit",12,IconCache::maxEntries+1);mutate("table bounds",16,0xffffffffu);
    mutate("label limit",20,IconCache::maxLabelBytes+1);mutate("variant",24,2);
    mutate("zero edge",28,0);mutate("large edge",28,257);mutate("width",32,15);mutate("height",36,15);
    mutate("offset low",40,0);mutate("offset high",44,0xffffffffu);mutate("compressed bounds",48,0xffffffffu);
    mutate("exact output size",52,16*16*4-1);mutate("CRC",56,0);
    {auto bytes=good;bytes.back()^=0xff;reject("compressed payload",bytes);}
    {auto bytes=good;bytes.push_back(0);reject("trailing data",bytes);}
    reject("file size limit",good,true);
    // 元数据仍声明合法尺寸，但流实际解压成另一长度。
    {auto bytes=good;for(const auto at:{28,32,36})U32(bytes,at,8);U32(bytes,52,8*8*4);reject("decompressed length",bytes);}
    Check(cases>=27,"validation matrix complete");
}
void DisabledAndClear(const fs::path& root) {
    const auto directory=root/"disabled";const auto hash=Hash("disabled");const auto file=directory/(hash+".fwic");
    IconCache::Key key{"icon",Swf::Variant::Mask,32};auto bitmap=Pixels(32);unsigned warnings=0;
    IconCache::Store cache(directory,[&](const auto&){++warnings;});cache.Configure(false,64);
    cache.Put(hash,key,bitmap);Check(!cache.Get(hash,key) && !cache.Clear() && !fs::exists(directory),"disabled makes no directory and no IO");
    cache.Configure(true,64);cache.Put(hash,key,bitmap);cache.Flush();const auto bytes=Read(file);const auto time=fs::last_write_time(file);
    cache.Configure(false,64);cache.Put(hash,key,bitmap);Check(!cache.Get(hash,key) && !cache.Clear(),"disabled ignores existing cache");
    Check(Read(file)==bytes && fs::last_write_time(file)==time && warnings==0,"disabled does not touch cache bytes or mtime");
    const auto temporary=directory/(hash+".fwic.tmp");Write(temporary,{1,2,3});Write(directory/"user.txt",{42});Write(directory/L"用户文件.txt",{42});
    {IconCache::Store restart(directory);restart.Configure(true,64);Check(!fs::exists(temporary) && bool(restart.Get(hash,key)),"interrupted temp removed, committed file still readable");
        Write(temporary,{4,5});Check(restart.Clear(),"clear succeeds");Check(!fs::exists(file) && !fs::exists(temporary) && fs::exists(directory/"user.txt"),"clear only removes owned files");
        Check(!restart.Get(hash,key),"clear drops in-memory compressed copy");}
    // 没有旧版本的中断写入不应被当作成功缓存。
    Write(temporary,{1});{IconCache::Store restart(directory);restart.Configure(true,64);Check(!restart.Get(hash,key) && !fs::exists(temporary),"orphan temp alone is a miss");}
    std::cout<<"PASS disabled/clear/interrupted write\n";
}
void Unwritable(const fs::path& root) {
    const auto blocker=root/"blocker";Write(blocker,{1});
    const auto hash=Hash("fallback");IconCache::Key key{"icon",Swf::Variant::Mask,32};auto bitmap=Pixels(32);
    unsigned warnings=0;
    IconCache::Store missing(blocker/"cache",[&](const auto&){++warnings;});missing.Configure(true,64);
    missing.Put(hash,key,bitmap);Check(missing.Get(hash,key)->rgba==bitmap.rgba,"cannot create directory retains memory bitmap");
    missing.Put(hash,key,bitmap);Check(!missing.DiskAvailable() && warnings==1,"unwritable directory warns once");
    // 对独立临时目录设置真实 Windows DACL；不用没有权限意义的目录 readonly 属性。
    const auto directory=root/"readonly";fs::create_directories(directory);
    PACL original=nullptr;PSECURITY_DESCRIPTOR descriptor=nullptr;
    Check(GetNamedSecurityInfoW(directory.c_str(),SE_FILE_OBJECT,DACL_SECURITY_INFORMATION,nullptr,nullptr,&original,nullptr,&descriptor)==ERROR_SUCCESS,"read temporary directory ACL");
    struct Restore {
        fs::path directory;PACL original;PSECURITY_DESCRIPTOR descriptor;PACL denied=nullptr;PSID everyone=nullptr;
        ~Restore(){SetNamedSecurityInfoW(const_cast<LPWSTR>(directory.c_str()),SE_FILE_OBJECT,DACL_SECURITY_INFORMATION,nullptr,nullptr,original,nullptr);LocalFree(descriptor);if(denied)LocalFree(denied);if(everyone)FreeSid(everyone);}
    } restore{directory,original,descriptor};
    SID_IDENTIFIER_AUTHORITY world=SECURITY_WORLD_SID_AUTHORITY;
    Check(AllocateAndInitializeSid(&world,1,SECURITY_WORLD_RID,0,0,0,0,0,0,0,&restore.everyone)!=0,"temporary Everyone SID");
    EXPLICIT_ACCESSW access{};access.grfAccessPermissions=FILE_ADD_FILE|FILE_ADD_SUBDIRECTORY;
    access.grfAccessMode=DENY_ACCESS;access.Trustee.TrusteeForm=TRUSTEE_IS_SID;access.Trustee.ptstrName=static_cast<LPWSTR>(restore.everyone);
    Check(SetEntriesInAclW(1,&access,original,&restore.denied)==ERROR_SUCCESS,"build readonly ACL");
    Check(SetNamedSecurityInfoW(const_cast<LPWSTR>(directory.c_str()),SE_FILE_OBJECT,DACL_SECURITY_INFORMATION,nullptr,nullptr,restore.denied,nullptr)==ERROR_SUCCESS,"set readonly ACL only on test directory");
    warnings=0;IconCache::Store readonly(directory,[&](const auto&){++warnings;});readonly.Configure(true,64);readonly.Put(hash,key,bitmap);
    Check(!readonly.DiskAvailable() && readonly.Get(hash,key)->rgba==bitmap.rgba && warnings==1,"real readonly directory falls back to memory once");
    Check(fs::is_empty(directory),"readonly directory has no files");
    std::cout<<"PASS cannot-create/readonly memory fallback\n";
}
// 只在本轮独立夹具上拒绝指定权限，作用域结束恢复原始 DACL。
struct DeniedAccess {
    fs::path path;
    PACL original=nullptr,denied=nullptr;
    PSECURITY_DESCRIPTOR descriptor=nullptr;
    PSID everyone=nullptr;
    explicit DeniedAccess(fs::path value):path(std::move(value)) {}
    void Apply(DWORD permissions) {
        Check(GetNamedSecurityInfoW(path.c_str(),SE_FILE_OBJECT,DACL_SECURITY_INFORMATION,nullptr,nullptr,&original,nullptr,&descriptor)==ERROR_SUCCESS,"read fixture DACL");
        SID_IDENTIFIER_AUTHORITY world=SECURITY_WORLD_SID_AUTHORITY;
        Check(AllocateAndInitializeSid(&world,1,SECURITY_WORLD_RID,0,0,0,0,0,0,0,&everyone)!=0,"create fixture Everyone SID");
        EXPLICIT_ACCESSW access{};access.grfAccessPermissions=permissions;access.grfAccessMode=DENY_ACCESS;
        access.Trustee.TrusteeForm=TRUSTEE_IS_SID;access.Trustee.ptstrName=static_cast<LPWSTR>(everyone);
        Check(SetEntriesInAclW(1,&access,original,&denied)==ERROR_SUCCESS,"build fixture deny DACL");
        Check(SetNamedSecurityInfoW(const_cast<LPWSTR>(path.c_str()),SE_FILE_OBJECT,DACL_SECURITY_INFORMATION,nullptr,nullptr,denied,nullptr)==ERROR_SUCCESS,"apply fixture deny DACL");
    }
    void Restore() {
        if(descriptor)Check(SetNamedSecurityInfoW(const_cast<LPWSTR>(path.c_str()),SE_FILE_OBJECT,DACL_SECURITY_INFORMATION,nullptr,nullptr,original,nullptr)==ERROR_SUCCESS,"restore fixture DACL");
    }
    ~DeniedAccess() {
        if(descriptor){SetNamedSecurityInfoW(const_cast<LPWSTR>(path.c_str()),SE_FILE_OBJECT,DACL_SECURITY_INFORMATION,nullptr,nullptr,original,nullptr);LocalFree(descriptor);}
        if(denied)LocalFree(denied);
        if(everyone)FreeSid(everyone);
    }
};
void TouchFailure(const fs::path& root) {
    const auto directory=root/"touch-failure";const auto hash=Hash("touch-denied"),other=Hash("touch-next");
    const IconCache::Key key{"icon",Swf::Variant::Mask,32};const auto bitmap=Pixels(32);
    {IconCache::Store seed(directory);seed.Configure(true,64);seed.Put(hash,key,bitmap);seed.Flush();}
    const auto file=directory/(hash+".fwic");const auto bytes=Read(file);
    const auto old=fs::file_time_type::clock::now()-std::chrono::hours(24);fs::last_write_time(file,old);
    DeniedAccess deny(file);deny.Apply(FILE_WRITE_ATTRIBUTES);
    std::error_code error;fs::last_write_time(file,fs::file_time_type::clock::now(),error);
    Check(error==std::errc::permission_denied,"fixture really denies mtime updates");
    Check(Read(file)==bytes,"mtime deny still allows reading cache content");
    std::vector<std::string> warnings;
    IconCache::Store cache(directory,[&](const auto& text){warnings.push_back(text);});cache.Configure(true,64);
    for(unsigned n=0;n<3;++n) {
        const auto hit=cache.Get(hash,key);Check(hit && hit->rgba==bitmap.rgba,"Touch failure retains cache hit");
    }
    Check(cache.DiskAvailable() && warnings==std::vector<std::string>{"I4 icon cache: cannot update access time; LRU order may be stale"},"Touch failure warns once without disabling disk cache");
    Check(fs::last_write_time(file)==old,"failed Touch leaves stale mtime");
    cache.Put(other,key,bitmap);cache.Flush();
    Check(cache.WriteCount()==1 && fs::exists(directory/(other+".fwic")),"new hash persists after Touch failure");
    const auto reloaded=cache.Get(hash,key);
    Check(reloaded && reloaded->rgba==bitmap.rgba && warnings.size()==1,"hash switch reloads disk file despite repeated Touch failure");
    cache.Configure(false,64);cache.Configure(true,64);
    Check(bool(cache.Get(hash,key)) && cache.DiskAvailable() && warnings.size()==1,"toggle preserves usable disk cache and warning deduplication");
    {IconCache::Store reader(directory);reader.Configure(true,64);const auto hit=reader.Get(other,key);
        Check(hit && hit->rgba==bitmap.rgba,"independent Store reads write made after Touch failure");}
    // 维护告警不抑制后续真正的写入失败，也不改变其停用语义。
    {DeniedAccess noWrites(directory);noWrites.Apply(FILE_ADD_FILE);
        cache.Put(other,key,bitmap);cache.Flush();cache.Flush();
        Check(!cache.DiskAvailable() && warnings.size()==2 && warnings.back()=="I4 icon cache unavailable; using memory only","real write failure still disables disk and warns once after Touch warning");
        noWrites.Restore();}
    deny.Restore();
    std::cout<<"PASS Touch DACL failure/hits/reload/write/toggle/one warning/real IO fallback\n";
}
void TrimFailure(const fs::path& root) {
    const auto directory=root/"trim-failure";const auto hash=Hash("trim-denied"),other=Hash("trim-next");
    const IconCache::Key key{"icon",Swf::Variant::Mask,32};const auto bitmap=Pixels(32);
    std::vector<std::string> warnings;
    IconCache::Store cache(directory,[&](const auto& text){warnings.push_back(text);});cache.Configure(true,64);
    cache.Put(hash,key,bitmap);cache.Flush();
    DeniedAccess deny(directory);deny.Apply(FILE_LIST_DIRECTORY);
    std::error_code error;fs::directory_iterator probe(directory,error);
    Check(error==std::errc::permission_denied,"fixture really denies LRU directory scan");
    cache.Configure(true,32);cache.Configure(true,16);
    Check(cache.DiskAvailable() && warnings==std::vector<std::string>{"I4 icon cache: cannot maintain LRU budget; cached size or order may be stale"},"Trim scan failure warns once without disabling cache");
    const auto hit=cache.Get(hash,key);Check(hit && hit->rgba==bitmap.rgba,"Trim failure retains existing disk hit");
    cache.Put(other,key,bitmap);cache.Flush();
    const auto reloaded=cache.Get(hash,key);
    Check(cache.WriteCount()==2 && reloaded && reloaded->rgba==bitmap.rgba,"Trim failure still permits writes and disk reloads");
    const auto scans=cache.TrimCount();
    for(unsigned n=0;n<8;++n){cache.Put(other,key,bitmap);cache.Flush();}
    Check(cache.TrimCount()==scans+1 && cache.DiskAvailable() && warnings.size()==1,"scheduled Trim retries and deduplicates warning");
    deny.Restore();
    cache.Configure(true,8);
    Check(cache.TrimCount()==scans+2 && cache.DiskAvailable() && warnings.size()==1,"later maintenance retries after permission recovery");
    {IconCache::Store reader(directory);reader.Configure(true,64);const auto persisted=reader.Get(other,key);
        Check(persisted && persisted->rgba==bitmap.rgba,"independent Store reads write made during Trim failure");}
    std::cout<<"PASS Trim DACL scan failure/hits/write/retry/one warning\n";
}
void TrimEvictionFailure(const fs::path& root) {
    const auto directory=root/"trim-eviction-failure";const auto hash=Hash("trim-large"),other=Hash("trim-small");
    const auto bitmap=Pixels(256,true);const IconCache::Key key{"icon0",Swf::Variant::Original,256};
    std::vector<std::string> warnings;
    IconCache::Store cache(directory,[&](const auto& text){warnings.push_back(text);});cache.Configure(true,64);
    for(unsigned n=0;n<33;++n)cache.Put(hash,{"icon"+std::to_string(n),Swf::Variant::Original,256},bitmap);
    cache.Flush();const auto file=directory/(hash+".fwic");
    Check(fs::file_size(file)>8ull*1024*1024,"eviction fixture exceeds minimum cache budget");
    {
        // 允许读写，但暂不共享删除权限，模拟文件被占用时的淘汰失败。
        struct Hold { HANDLE value;~Hold(){if(value!=INVALID_HANDLE_VALUE)CloseHandle(value);} } hold{
            CreateFileW(file.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr)};
        Check(hold.value!=INVALID_HANDLE_VALUE,"hold fixture without delete sharing");
        cache.Configure(true,8);cache.Configure(true,16);cache.Configure(true,8);
        Check(fs::exists(file) && cache.DiskAvailable() && warnings==std::vector<std::string>{"I4 icon cache: cannot maintain LRU budget; cached size or order may be stale"},"eviction failure leaves cache available and warns once");
        const auto hit=cache.Get(hash,key);Check(hit && hit->rgba==bitmap.rgba,"eviction failure preserves valid content");
        cache.Put(other,{"small",Swf::Variant::Mask,32},Pixels(32));cache.Flush();
        Check(fs::exists(directory/(other+".fwic")) && cache.DiskAvailable(),"writes continue after eviction failure");
    }
    cache.Configure(true,16);cache.Configure(true,8);
    Check(!fs::exists(file) && fs::exists(directory/(other+".fwic")) && cache.DiskAvailable() && warnings.size()==1,"later maintenance evicts unlocked file and restores budget");
    std::cout<<"PASS Trim eviction sharing failure/hits/write/recovery/one warning\n";
}
void Capacity(const fs::path& root) {
    const auto directory=root/"capacity";const auto bitmap=Pixels(256,true);
    const std::array hashes{Hash("LRU-a"),Hash("LRU-b"),Hash("LRU-c")};
    {IconCache::Store seed(directory);seed.Configure(true,64);
        for(const auto& hash:hashes)for(unsigned n=0;n<12;++n)seed.Put(hash,{"icon"+std::to_string(n),Swf::Variant::Original,256},bitmap);
        seed.Flush();}
    const auto now=fs::file_time_type::clock::now();
    for(unsigned n=0;n<3;++n)fs::last_write_time(directory/(hashes[n]+".fwic"),now-std::chrono::hours(72-24*n));
    {IconCache::Store cache(directory);cache.Configure(true,64);Check(bool(cache.Get(hashes[0],{"icon0",Swf::Variant::Original,256})),"LRU touch oldest");
        Check(fs::last_write_time(directory/(hashes[0]+".fwic"))>now-std::chrono::seconds(1),"hit updates mtime");
        cache.Configure(true,8);}
    Check(fs::exists(directory/(hashes[0]+".fwic")) && !fs::exists(directory/(hashes[1]+".fwic")) && fs::exists(directory/(hashes[2]+".fwic")),"capacity shrink evicts least recently used file");
    std::uint64_t size=0;for(const auto& file:fs::directory_iterator(directory))size+=file.file_size();Check(size<=8*1024*1024,"capacity enforced");
    {IconCache::Store cache(directory);cache.Configure(true,8);for(unsigned n=0;n<12;++n)cache.Put(Hash("LRU-d"),{"icon"+std::to_string(n),Swf::Variant::Original,256},bitmap);cache.Flush();}
    size=0;for(const auto& file:fs::directory_iterator(directory))size+=file.file_size();Check(size<=8*1024*1024,"capacity also enforced after writes");
    std::cout<<"PASS mtime LRU/shrink/write capacity bytes="<<size<<'\n';
}
void Batching(const fs::path& root) {
    const auto directory=root/"batching";const auto hash=Hash("batching"),other=Hash("next hash");
    const IconCache::Key key{"icon",Swf::Variant::Mask,32};const auto bitmap=Pixels(32);
    {IconCache::Store cache(directory);cache.Configure(true,64);cache.Put(hash,key,bitmap);
        Check(cache.WriteCount()==0 && !fs::exists(directory/(hash+".fwic")),"Put defers disk write");
        cache.Get(other,key);Check(cache.WriteCount()==1,"hash switch flushes pending entries once");
        cache.Put(other,key,bitmap);Check(cache.Clear(),"clear accepts pending batch");cache.Flush();
        Check(cache.WriteCount()==1 && !fs::exists(directory/(other+".fwic")),"clear discards pending batch without stale flush");
        cache.Put(hash,key,bitmap);cache.Configure(false,64);cache.Flush();
        Check(cache.WriteCount()==1 && !fs::exists(directory/(hash+".fwic")),"disable discards pending data and Flush does no IO");
        cache.Configure(true,64);cache.Put(hash,key,bitmap);cache.Configure(true,32);cache.Flush();
        Check(cache.WriteCount()==2,"budget change retains pending entries");
        const auto scans=cache.TrimCount();
        for(unsigned n=0;n<8;++n){cache.Put(hash,key,bitmap);cache.Flush();}
        Check(cache.TrimCount()==scans+1,"eight small writes trigger just one directory scan");
        Check(cache.Clear(),"clear before destructor check");cache.Put(hash,key,bitmap);}
    Check(!fs::exists(directory/(hash+".fwic")),"Store destruction drops pending data without disk IO");
    std::cout<<"PASS batching/switch/clear/disable/destructor/trim cadence\n";
}
void SettingsCoverage(const fs::path& root) {
    const auto file=fs::absolute(root/"settings.ini");const auto utf8=file.u8string();SetSettingsPath({reinterpret_cast<const char*>(utf8.data()),utf8.size()});LoadSettings();
    Check(Config().i4IconCache && Config().i4IconCacheMB==64,"cache defaults");
    for(const auto& [text,value]:{std::pair{"-1",8},std::pair{"999",512},std::pair{"bad",8},std::pair{"",64}}) {
        const auto data=std::string("[Display]\nI4IconCache=bad\nI4IconCacheMB=")+text+"\n";
        Write(file,{data.begin(),data.end()});LoadSettings();
        Check(!Config().i4IconCache && Config().i4IconCacheMB==value,"cache malformed fields follow T-5 normalization");
        Check(Read(file)==std::vector<std::uint8_t>(data.begin(),data.end()),"load leaves user INI unchanged");
    }
    BeginSettings();auto draft=Config();draft.i4IconCache=true;draft.i4IconCacheMB=900;EditSettings(draft);
    Check(!I4::Runtime().iconCache && Config().i4IconCacheMB==512,"draft clamps but waits for Apply");
    Check(SaveSettings(),"persist cache settings");LoadSettings();
    Check(Config().i4IconCache && Config().i4IconCacheMB==512 && I4::Runtime().iconCache && I4::Runtime().iconCacheMB==512,"cache settings roundtrip and publish");
    Check(GetPrivateProfileIntW(L"Display",L"I4IconCache",0,file.c_str())==1 && GetPrivateProfileIntW(L"Display",L"I4IconCacheMB",0,file.c_str())==512,"external INI reader sees Display cache fields");
    BeginSettings();draft=Config();draft.i4IconCache=false;EditSettings(draft);RevertSettings();Check(I4::Runtime().iconCache && Config().i4IconCache,"cancel preserves applied settings");
    std::cout<<"PASS cache settings/defaults/clamping/Apply/persistence\n";
}
}
int main() {
    try {
        const auto root=fs::path("build/i4-phase3/t4b-fix1/unit")/std::to_string(GetCurrentProcessId());fs::create_directories(root);
        Basic(root);Validation(root);DisabledAndClear(root);Unwritable(root);TouchFailure(root);TrimFailure(root);TrimEvictionFailure(root);Capacity(root);Batching(root);SettingsCoverage(root);
        std::cout<<"IconCacheTests passed: "<<checks<<" checks; fixtures="<<root.string()<<'\n';return 0;
    } catch(const std::exception& error){std::cerr<<"IconCacheTests failed: "<<error.what()<<'\n';return 1;}
}
