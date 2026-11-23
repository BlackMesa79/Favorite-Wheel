#include "IconCache.h"
#include <Windows.h>
#include <bcrypt.h>
#include <miniz.h>
#include <algorithm>
#include <array>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace Wheel::IconCache {
namespace {
    namespace fs = std::filesystem;
    struct Handle {
        HANDLE value = INVALID_HANDLE_VALUE;
        ~Handle() { if(value!=INVALID_HANDLE_VALUE)CloseHandle(value); }
    };
    bool Digest(std::string_view value) {
        return value.size()==64 && std::ranges::all_of(value,[](char c){return (c>='0' && c<='9') || (c>='a' && c<='f');});
    }
    bool Owned(const fs::path& file, bool temporary=false) {
        const auto utf8=file.filename().u8string();
        const std::string name(reinterpret_cast<const char*>(utf8.data()),utf8.size());
        return name.size()>64 && Digest(std::string_view(name).substr(0,64)) &&
            name.substr(64)==(temporary?".fwic.tmp":".fwic");
    }
    fs::directory_iterator CacheFiles(const fs::path& directory) {
        std::error_code error;
        fs::directory_iterator files(directory,error);
        if(error && error!=std::errc::no_such_file_or_directory)
            throw fs::filesystem_error("list I4 icon cache",directory,error);
        return files;
    }
    bool Valid(const Key& key) {
        return !key.label.empty() && key.label.size()<=maxLabelBytes && key.edge>=1 && key.edge<=256 &&
            (key.variant==Swf::Variant::Original || key.variant==Swf::Variant::Mask);
    }
    std::uint64_t Raw(const Key& key) { return std::uint64_t(key.edge)*key.edge*4; }
    void U32(std::vector<std::uint8_t>& bytes,std::uint32_t value) {
        for(unsigned n=0;n<4;++n)bytes.push_back(static_cast<std::uint8_t>(value>>(n*8)));
    }
    void U64(std::vector<std::uint8_t>& bytes,std::uint64_t value) { U32(bytes,std::uint32_t(value));U32(bytes,std::uint32_t(value>>32)); }
    struct Cursor {
        std::span<const std::uint8_t> bytes;
        std::size_t at=0;
        std::span<const std::uint8_t> Take(std::size_t n) {
            if(n>bytes.size()-at)throw std::runtime_error("FWIC bounds");
            auto result=bytes.subspan(at,n);at+=n;return result;
        }
        std::uint32_t U32() { const auto v=Take(4);return std::uint32_t(v[0]) | std::uint32_t(v[1])<<8 | std::uint32_t(v[2])<<16 | std::uint32_t(v[3])<<24; }
        std::uint64_t U64() { const auto lo=U32();return lo | std::uint64_t(U32())<<32; }
    };
}
std::string Hash(std::span<const std::uint8_t> bytes) {
    if(bytes.size()>std::numeric_limits<ULONG>::max())return {};
    std::array<UCHAR,32> digest{};
    if(BCryptHash(BCRYPT_SHA256_ALG_HANDLE,nullptr,0,const_cast<PUCHAR>(bytes.data()),
        static_cast<ULONG>(bytes.size()),digest.data(),static_cast<ULONG>(digest.size()))<0)return {};
    constexpr char hex[]="0123456789abcdef";
    std::string result;result.reserve(64);
    for(auto b:digest){result+=hex[b>>4];result+=hex[b&15];}
    return result;
}
Store::Store(fs::path directory,Logger logger,std::uint32_t converter):
    directory_(std::move(directory)),logger_(std::move(logger)),converter_(converter) {}
void Store::Unavailable() {
    unavailable_=true;
    if(!ioWarned_) { ioWarned_=true;if(logger_)logger_("I4 icon cache unavailable; using memory only"); }
}
void Store::Configure(bool enabled,unsigned budgetMB) {
    const bool toggled=enabled_!=enabled;
    const bool changed=enabled_!=enabled || budgetMB_!=std::clamp(budgetMB,8u,512u);
    enabled_=enabled;budgetMB_=std::clamp(budgetMB,8u,512u);
    if(toggled) { active_.clear();entries_.clear();pendingEntries_=0;pendingBytes_=0; }
    if(enabled_ && changed)Trim();
}
void Store::Invalid(const std::string& hash) {
    entries_.clear();
    if(invalidWarned_.insert(hash).second && logger_)logger_("I4 icon cache discarded: "+hash+" (invalid FWIC)");
    std::error_code error;fs::remove(directory_/(hash+".fwic"),error);
    if(error)Unavailable();
}
std::optional<Swf::Bitmap> Store::Decode(const Key& key,const Entry& entry) {
    Swf::Bitmap bitmap{key.edge,key.edge,std::vector<std::uint8_t>(static_cast<std::size_t>(Raw(key)))};
    mz_ulong size=static_cast<mz_ulong>(bitmap.rgba.size()),sourceSize=static_cast<mz_ulong>(entry.compressed.size());
    if(mz_uncompress2(bitmap.rgba.data(),&size,entry.compressed.data(),&sourceSize)!=MZ_OK ||
        size!=bitmap.rgba.size() || sourceSize!=entry.compressed.size() ||
        mz_crc32(MZ_CRC32_INIT,bitmap.rgba.data(),bitmap.rgba.size())!=entry.crc)return {};
    return bitmap;
}
bool Store::Load(const fs::path& file) {
    // 先检查文件长度再分配；同一句柄下读完整快照，禁止写入但允许原子替换。
    Handle handle{CreateFileW(file.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr)};
    if(handle.value==INVALID_HANDLE_VALUE)throw std::runtime_error("FWIC open");
    LARGE_INTEGER length{};
    if(!GetFileSizeEx(handle.value,&length) || length.QuadPart<20 || std::uint64_t(length.QuadPart)>maxFileBytes)return false;
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length.QuadPart));DWORD read=0;
    if(!ReadFile(handle.value,bytes.data(),static_cast<DWORD>(bytes.size()),&read,nullptr) || read!=bytes.size())return false;
    try {
        Cursor header{bytes};const auto magic=header.Take(4);
        if(!std::ranges::equal(magic,std::array<std::uint8_t,4>{'F','W','I','C'}) || header.U32()!=formatVersion || header.U32()!=converter_)return false;
        const auto count=header.U32(),tableSize=header.U32();
        if(!count || count>maxEntries || tableSize>bytes.size()-20 || tableSize<std::uint64_t(count)*41)return false;
        Cursor table{header.Take(tableSize)};
        std::uint64_t next=20ull+tableSize,decoded=0;
        std::map<Key,Entry> loaded;
        for(std::uint32_t i=0;i<count;++i) {
            const auto labelSize=table.U32(),variant=table.U32(),edge=table.U32(),width=table.U32(),height=table.U32();
            const auto offset=table.U64();const auto compressed=table.U32(),raw=table.U32(),crc=table.U32();
            if(!labelSize || labelSize>maxLabelBytes || variant>1 || edge<1 || edge>256 || width!=edge || height!=edge)return false;
            const auto label=table.Take(labelSize);
            Key key{std::string(reinterpret_cast<const char*>(label.data()),label.size()),static_cast<Swf::Variant>(variant),edge};
            if(raw!=Raw(key) || !compressed || offset!=next || offset>bytes.size() || compressed>bytes.size()-offset)return false;
            decoded+=raw;if(decoded>maxDecodedBytes)return false;
            next+=compressed;
            Entry entry{crc,{bytes.begin()+static_cast<std::ptrdiff_t>(offset),bytes.begin()+static_cast<std::ptrdiff_t>(next)}};
            // 所有条目通过解压/CRC 才发布，不能使用损坏文件的“好”条目。
            if(!Decode(key,entry) || !loaded.emplace(std::move(key),std::move(entry)).second)return false;
        }
        if(table.at!=table.bytes.size() || next!=bytes.size())return false;
        entries_=std::move(loaded);return true;
    } catch(const std::runtime_error&) {return false;}
}
void Store::Select(const std::string& hash) {
    if(active_==hash)return;
    Flush();
    active_=hash;entries_.clear();pendingEntries_=0;pendingBytes_=0;
    if(!DiskAvailable())return;
    try {
        const auto file=directory_/(hash+".fwic");
        if(fs::exists(file) && !Load(file))Invalid(hash);
    } catch(...) {Unavailable();}
}
void Store::Touch(const std::string& hash) {
    if(!DiskAvailable())return;
    std::error_code error;
    fs::last_write_time(directory_/(hash+".fwic"),fs::file_time_type::clock::now(),error);
    // mtime 只影响淘汰顺序；MO2 拒绝更新属性时仍可读取、写入缓存内容。
    if(error && error!=std::errc::no_such_file_or_directory && !touchWarned_) {
        touchWarned_=true;
        if(logger_)logger_("I4 icon cache: cannot update access time; LRU order may be stale");
    }
}
std::optional<Swf::Bitmap> Store::Get(const std::string& hash,const Key& key) {
    if(!enabled_ || !Digest(hash) || !Valid(key))return {};
    try {
        Select(hash);
        if(const auto it=entries_.find(key);it!=entries_.end()) {
            auto bitmap=Decode(key,it->second);
            if(bitmap)Touch(hash);
            return bitmap;
        }
    } catch(...) {Unavailable();}
    return {};
}
void Store::Save() {
    if(!DiskAvailable())return;
    try {
        std::uint64_t tableSize=0,total=20,decoded=0;
        for(const auto& [key,entry]:entries_) {tableSize+=40+key.label.size();total+=40+key.label.size()+entry.compressed.size();decoded+=Raw(key);}
        if(entries_.empty() || entries_.size()>maxEntries || total>maxFileBytes || decoded>maxDecodedBytes)return;
        std::vector<std::uint8_t> bytes{'F','W','I','C'};bytes.reserve(static_cast<std::size_t>(total));
        U32(bytes,formatVersion);U32(bytes,converter_);U32(bytes,static_cast<std::uint32_t>(entries_.size()));U32(bytes,static_cast<std::uint32_t>(tableSize));
        std::uint64_t offset=20+tableSize;
        for(const auto& [key,entry]:entries_) {
            U32(bytes,static_cast<std::uint32_t>(key.label.size()));U32(bytes,static_cast<std::uint32_t>(key.variant));
            U32(bytes,key.edge);U32(bytes,key.edge);U32(bytes,key.edge);U64(bytes,offset);
            U32(bytes,static_cast<std::uint32_t>(entry.compressed.size()));U32(bytes,static_cast<std::uint32_t>(Raw(key)));U32(bytes,entry.crc);
            bytes.insert(bytes.end(),key.label.begin(),key.label.end());offset+=entry.compressed.size();
        }
        for(const auto& [key,entry]:entries_)bytes.insert(bytes.end(),entry.compressed.begin(),entry.compressed.end());
        // MO2 会把建目录和临时文件重定向到 Overwrite，即使缓存已经从模组命中。
        // 只有实际提交位图时才创建目录并打开写句柄。
        fs::create_directories(directory_);
        const auto final=directory_/(active_+".fwic"),temporary=directory_/(active_+".fwic.tmp");
        {
            Handle handle{CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr)};
            DWORD written=0;
            if(handle.value==INVALID_HANDLE_VALUE || !WriteFile(handle.value,bytes.data(),static_cast<DWORD>(bytes.size()),&written,nullptr) ||
                written!=bytes.size() || !FlushFileBuffers(handle.value)) {Unavailable();return;}
        }
        // 同目录同卷替换；不允许退化为跨卷复制。中断最多留下 .tmp 或完整旧文件。
        if(!MoveFileExW(temporary.c_str(),final.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){Unavailable();return;}
        ++writeCount_;++writesSinceTrim_;bytesSinceTrim_+=bytes.size();
        pendingEntries_=0;pendingBytes_=0;
        // 小文件每 8 次原子写入扫描一次；大文件累计写入容量的 1/4（最多 16 MiB）再扫描。
        if(writesSinceTrim_>=8 || bytesSinceTrim_>=std::min(std::uint64_t(budgetMB_)*1024*1024/4,16ull*1024*1024))Trim();
    } catch(...) {Unavailable();}
}
void Store::Put(const std::string& hash,const Key& key,const Swf::Bitmap& bitmap) {
    if(!enabled_ || !Digest(hash) || !Valid(key) || bitmap.width!=key.edge || bitmap.height!=key.edge || bitmap.rgba.size()!=Raw(key))return;
    try {
        Select(hash);
        Entry entry{static_cast<std::uint32_t>(mz_crc32(MZ_CRC32_INIT,bitmap.rgba.data(),bitmap.rgba.size())),{}};
        mz_ulong size=mz_compressBound(static_cast<mz_ulong>(bitmap.rgba.size()));entry.compressed.resize(size);
        if(mz_compress2(entry.compressed.data(),&size,bitmap.rgba.data(),static_cast<mz_ulong>(bitmap.rgba.size()),MZ_BEST_SPEED)!=MZ_OK)return;
        entry.compressed.resize(size);
        // 超大库按文件边界重新开始，保证本次写出的文件总能被读取上限接受。
        std::uint64_t total=20+40+key.label.size()+entry.compressed.size(),decoded=Raw(key);
        for(const auto& [old,value]:entries_)if(old!=key){total+=40+old.label.size()+value.compressed.size();decoded+=Raw(old);}
        if((!entries_.contains(key) && entries_.size()>=maxEntries) || total>maxFileBytes || decoded>maxDecodedBytes) {
            Flush();entries_.clear();pendingEntries_=0;pendingBytes_=0;
        }
        pendingBytes_+=entry.compressed.size();++pendingEntries_;
        entries_.insert_or_assign(key,std::move(entry));
        if(pendingEntries_>=32 || pendingBytes_>=4ull*1024*1024)Flush();
    } catch(...) {Unavailable();}
}
void Store::Flush() {
    if(enabled_ && pendingEntries_)Save();
}
void Store::Trim() {
    if(!DiskAvailable())return;
    try {
        ++trimCount_;writesSinceTrim_=0;bytesSinceTrim_=0;
        struct File { fs::path path;fs::file_time_type time;std::uint64_t size; };
        std::vector<File> files;std::uint64_t total=0;
        for(const auto& item:CacheFiles(directory_)) {
            if(!item.is_regular_file())continue;
            if(!cleanedTemporaryFiles_ && Owned(item.path(),true))fs::remove(item.path());
            if(!Owned(item.path()))continue;
            const auto size=item.file_size();
            if(size>std::numeric_limits<std::uint64_t>::max()-total)throw std::runtime_error("FWIC capacity");
            total+=size;files.push_back({item.path(),item.last_write_time(),size});
        }
        cleanedTemporaryFiles_=true;
        std::ranges::sort(files,[](const File& a,const File& b){return a.time!=b.time?a.time<b.time:a.path<b.path;});
        for(const auto& file:files) {
            if(total<=std::uint64_t(budgetMB_)*1024*1024)break;
            fs::remove(file.path);total-=file.size;
        }
    } catch(...) {
        // 扫描、统计和淘汰只负责容量维护；失败后在下一个维护点重试。
        if(!trimWarned_) {
            trimWarned_=true;
            if(logger_)logger_("I4 icon cache: cannot maintain LRU budget; cached size or order may be stale");
        }
    }
}
bool Store::Clear() {
    active_.clear();entries_.clear();pendingEntries_=0;pendingBytes_=0;
    if(!DiskAvailable())return false;
    try {
        for(const auto& item:CacheFiles(directory_))
            if(item.is_regular_file() && (Owned(item.path()) || Owned(item.path(),true)))fs::remove(item.path());
        return true;
    } catch(...) {Unavailable();return false;}
}
}
