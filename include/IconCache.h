#pragma once
#include "SwfRaster.h"
#include <filesystem>
#include <functional>
#include <map>
#include <set>

namespace Wheel::IconCache {
    // 修改 SwfRaster 栅格化逻辑时必须递增；与容器格式版本独立。
    inline constexpr std::uint32_t converterVersion = 1;
    inline constexpr std::uint32_t formatVersion = 1;
    inline constexpr std::uint32_t maxEntries = 4096, maxLabelBytes = 1024;
    inline constexpr std::uint64_t maxFileBytes = 64ull*1024*1024;
    inline constexpr std::uint64_t maxDecodedBytes = 256ull*1024*1024;
    using Logger = std::function<void(const std::string&)>;
    // 仅由后台线程调用。失败返回空字符串，由调用方继续栅格化。
    std::string Hash(std::span<const std::uint8_t> bytes);
    struct Key {
        std::string label;
        Swf::Variant variant = Swf::Variant::Mask;
        std::uint32_t edge = 128;
        auto operator<=>(const Key&) const = default;
    };
    // 单线程所有权：构造不做 IO；所有方法由同一后台线程依次调用。
    // 只保留最近一个 SWF 的压缩条目，GPU/源文件的内存缓存仍由 I4Textures 管理。
    class Store {
    public:
        explicit Store(std::filesystem::path directory, Logger logger = {},
            std::uint32_t converter = converterVersion);
        void Configure(bool enabled, unsigned budgetMB);
        std::optional<Swf::Bitmap> Get(const std::string& hash, const Key& key);
        void Put(const std::string& hash, const Key& key, const Swf::Bitmap& bitmap);
        // 仅由工作线程在队列空闲时显式刷新。析构不做 IO；进程退出允许丢失待写条目。
        void Flush();
        bool Clear(); // 禁用时不访问磁盘；失败返回 false。
        bool DiskAvailable() const { return enabled_ && !unavailable_ && !directory_.empty(); }
        std::uint64_t WriteCount() const { return writeCount_; }
        std::uint64_t TrimCount() const { return trimCount_; }
    private:
        struct Entry { std::uint32_t crc; std::vector<std::uint8_t> compressed; };
        std::filesystem::path directory_;
        Logger logger_;
        std::uint32_t converter_;
        bool enabled_=false, unavailable_=false, initialized_=false, ioWarned_=false;
        bool touchWarned_=false, trimWarned_=false;
        unsigned budgetMB_=64;
        std::uint64_t writeCount_=0;
        std::uint64_t trimCount_=0, bytesSinceTrim_=0, writesSinceTrim_=0;
        std::uint64_t pendingBytes_=0;
        unsigned pendingEntries_=0;
        std::string active_;
        std::map<Key,Entry> entries_;
        std::set<std::string> invalidWarned_;
        void Unavailable();
        bool Prepare();
        void Select(const std::string& hash);
        void Invalid(const std::string& hash);
        void Trim();
        void Touch(const std::string& hash);
        bool Load(const std::filesystem::path& file);
        void Save();
        static std::optional<Swf::Bitmap> Decode(const Key&, const Entry&);
    };
}
