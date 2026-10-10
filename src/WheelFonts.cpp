#include "WheelFonts.h"
#include "Settings.h"
#include "UIResources.h"
#include <array>
#include <fstream>
#include <filesystem>
#include <limits>
#include <map>
#include <stdexcept>
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include <imstb_truetype.h>

namespace Wheel {
    namespace {
        constexpr std::array sizes{12.f,14.f,15.f,16.f,17.f,18.f,20.f,21.f,22.f,25.f,30.f};
        ImFontGlyphRangesBuilder glyphs;
        ImVector<ImWchar> ranges;
        std::map<int, ImFont*> fonts;
        float bakedScale = 0;
        float bakedTitle=0,bakedLabel=0;
        struct FontFile {std::vector<unsigned char> data;stbtt_fontinfo info{};bool valid=false;};
        std::map<std::string,FontFile> fontFiles;
        struct ExtraFont {FontFile* file;ImVector<ImWchar> ranges;};
        std::vector<ExtraFont> extras; // ImGui retains range pointers until the next atlas clear.
        std::string loadedFont;
        FontFile& ReadFont(const std::string& path) {
            auto [entry,inserted]=fontFiles.try_emplace(path);
            auto& font=entry->second;
            if(!inserted)return font;
            std::ifstream file(std::filesystem::u8path(path),std::ios::binary|std::ios::ate);
            const auto length=file?static_cast<std::streamoff>(file.tellg()):0;
            if(length<=0 || length>std::numeric_limits<int>::max())return font;
            font.data.resize(static_cast<std::size_t>(length));file.seekg(0);
            if(!file.read(reinterpret_cast<char*>(font.data.data()),length)){font.data.clear();return font;}
            const int offset=stbtt_GetFontOffsetForIndex(font.data.data(),0);
            font.valid=offset>=0 && stbtt_InitFont(&font.info,font.data.data(),offset);
            return font;
        }
    }
    bool PrepareFonts(const View& view, float scale) {
        auto& io = ImGui::GetIO();
        const auto path=FontPath(view.config);
        const bool fontChanged=path!=loadedFont;
        const auto& theme=Style(view.config);
        const bool metricsChanged=theme.titleScale!=bakedTitle || theme.labelScale!=bakedLabel;
        if (bakedScale != scale || fontChanged || metricsChanged) {
            glyphs.Clear();
            glyphs.AddRanges(io.Fonts->GetGlyphRangesDefault());
        }
        const auto previous = glyphs.UsedChars;
        glyphs.AddText(view.inventoryGlyphs.c_str());
        glyphs.AddText(view.outfitName.text.c_str());
        glyphs.AddText(UIGlyphs(view.config).c_str());
        for (const auto& item : view.items) {glyphs.AddText(item.name.c_str());glyphs.AddText(item.detail.c_str());glyphs.AddText(ItemInfoGlyphs(item.info).c_str());}
        bool changed = bakedScale != scale || fontChanged || metricsChanged || fonts.empty();
        for (int i = 0; i < glyphs.UsedChars.Size && !changed; ++i)
            changed = previous[i] != glyphs.UsedChars[i];
        if (!changed) return false;

        io.Fonts->Clear();
        extras.clear();
        ranges.clear();
        glyphs.BuildRanges(&ranges);
        io.FontDefault = nullptr;
        fonts.clear();
        loadedFont=path;
        auto& primary=ReadFont(path);
        // Resolve missing requested glyphs once per atlas build, not per frame.
        // Merge only those glyphs; do not bake full CJK alphabets at every size.
        std::vector<ImWchar> missing;
        for(int range=0;ranges[range];range+=2)
            for(unsigned code=ranges[range];code<=ranges[range+1];++code)
                if(!primary.valid || !stbtt_FindGlyphIndex(&primary.info,static_cast<int>(code)))
                    missing.push_back(static_cast<ImWchar>(code));
        extras.reserve(FallbackFontPaths().size());
        for(const auto& fallback:FallbackFontPaths()) {
            if(missing.empty())break;
            if(fallback==path)continue;
            auto& font=ReadFont(fallback);if(!font.valid)continue;
            ImFontGlyphRangesBuilder extraGlyphs;
            const auto before=missing.size();
            std::erase_if(missing,[&](ImWchar code) {
                if(!stbtt_FindGlyphIndex(&font.info,code))return false;
                extraGlyphs.AddChar(code);return true;
            });
            if(missing.size()!=before) {
                auto& extra=extras.emplace_back();extra.file=&font;extraGlyphs.BuildRanges(&extra.ranges);
            }
        }
        std::vector<float> bakeSizes(sizes.begin(),sizes.end());
        bakeSizes.push_back(25*theme.titleScale);
        bakeSizes.push_back(16*theme.labelScale);
        for (float base : bakeSizes) {
            const int pixels = std::max(1, static_cast<int>(std::round(base * scale)));
            if (fonts.contains(pixels)) continue;
            ImFontConfig cfg;
            cfg.SizePixels = static_cast<float>(pixels);
            cfg.OversampleH = 2;
            cfg.OversampleV = 1;
            cfg.PixelSnapH = true;
            // Share the font file across sizes; only glyph bitmaps are per-size.
            cfg.FontDataOwnedByAtlas = false;
            ImFont* font = primary.valid ? io.Fonts->AddFontFromMemoryTTF(primary.data.data(), static_cast<int>(primary.data.size()), cfg.SizePixels, &cfg, ranges.Data) : nullptr;
            if (!font) font = io.Fonts->AddFontDefault(&cfg);
            for(const auto& extra:extras) {
                auto merged=cfg;merged.MergeMode=true;merged.DstFont=font;
                io.Fonts->AddFontFromMemoryTTF(extra.file->data.data(),static_cast<int>(extra.file->data.size()),cfg.SizePixels,&merged,extra.ranges.Data);
            }
            fonts.emplace(pixels, font);
        }
        io.FontDefault = fonts.begin()->second;
        if (!io.Fonts->Build()) throw std::runtime_error("Unable to build wheel font atlas");
        bakedScale = scale;
        bakedTitle=theme.titleScale;bakedLabel=theme.labelScale;
        return true;
    }
    ImFont* FontAt(float pixelSize) {
        const auto found = fonts.find(std::max(1, static_cast<int>(std::round(pixelSize))));
        return found != fonts.end() ? found->second : ImGui::GetFont();
    }
}
