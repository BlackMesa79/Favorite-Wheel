#pragma once
#include "I4Rules.h"
#include "SwfRaster.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <map>
#include <set>
#include <tuple>

namespace Wheel::I4 {
    enum class ColorMode : std::uint8_t { Theme, I4, Original };
    enum class IconScope : std::uint8_t { All, Rule };
    struct RuntimeConfig {
        bool icons = true;
        ColorMode color = ColorMode::Theme;
        IconScope scope = IconScope::All;
        unsigned textureBudgetMB = 32;
    };
    // T-5 will connect these defaults to the settings snapshot.
    inline std::atomic<RuntimeConfig> runtimeConfig{RuntimeConfig{}};
    inline RuntimeConfig Runtime() { return runtimeConfig.load(std::memory_order_relaxed); }
    inline std::atomic<bool> dataAvailable{false};
    inline bool Eligible(const Icon& icon, bool themeOff = false, const RuntimeConfig& config = Runtime()) {
        return config.icons && !themeOff && dataAvailable.load(std::memory_order_relaxed) &&
            icon.origin != IconOrigin::None && !icon.source.empty() && !icon.label.empty() &&
            (config.scope == IconScope::All || icon.origin == IconOrigin::Rule);
    }
    inline std::string SourceKey(std::string source) {
        for (auto& c : source) { if(c >= 'A' && c <= 'Z') c += 'a'-'A'; if(c == '\\') c = '/'; }
        return source;
    }
    struct TextureKey {
        std::string source, label;
        Swf::Variant variant = Swf::Variant::Mask;
        std::uint32_t edge = 64;
        auto operator<=>(const TextureKey&) const = default;
    };
    using IconKey = std::pair<std::string, std::string>;
    inline IconKey Identity(const Icon& icon) { return {SourceKey(icon.source), icon.label}; }
    inline std::uint32_t TextureEdge(float pixels) {
        if(!std::isfinite(pixels)) return 256;
        return pixels * 2 <= 64 ? 64 : pixels * 2 <= 128 ? 128 : 256;
    }
    inline std::size_t MipBytes(std::uint32_t edge) {
        std::size_t bytes = 0;
        for(; edge; edge /= 2) bytes += std::size_t(edge) * edge * 4;
        return bytes;
    }
    // Render-thread policy, independent of DX11. Pinned identities protect both variants at the active size.
    class TextureBudget {
        struct Record { std::size_t bytes; std::uint64_t touched; };
        std::map<TextureKey, Record> entries;
        std::set<IconKey> pinned;
        std::uint32_t pinnedEdge=0;
        std::uint64_t clock = 0;
        std::size_t used = 0;
    public:
        static constexpr unsigned uploadsPerFrame = 4;
        void Pin(std::set<IconKey> identities,std::uint32_t edge=0) { pinned = std::move(identities); pinnedEdge=edge; }
        void Touch(const TextureKey& key) { if(auto it=entries.find(key);it!=entries.end()) it->second.touched=++clock; }
        bool Contains(const TextureKey& key) const { return entries.contains(key); }
        std::size_t Used() const { return used; }
        void Clear() { entries.clear(); pinned.clear(); used=0; clock=0; }
        bool Admit(const TextureKey& key, std::size_t limit, std::vector<TextureKey>& evicted) {
            if(entries.contains(key)) { Touch(key); return true; }
            const auto bytes=MipBytes(key.edge);
            if(bytes>limit) return false;
            while(used>limit-bytes) {
                auto oldest=entries.end();
                for(auto it=entries.begin();it!=entries.end();++it)
                    if((!pinned.contains({it->first.source,it->first.label}) || (pinnedEdge && it->first.edge!=pinnedEdge)) &&
                        (oldest==entries.end() || it->second.touched<oldest->second.touched)) oldest=it;
                if(oldest==entries.end()) return false;
                used-=oldest->second.bytes;evicted.push_back(oldest->first);entries.erase(oldest);
            }
            entries.emplace(key,Record{bytes,++clock});used+=bytes;return true;
        }
        void Erase(const TextureKey& key) {
            if(auto it=entries.find(key);it!=entries.end()) {used-=it->second.bytes;entries.erase(it);}
        }
    };
}
