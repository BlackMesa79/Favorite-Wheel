#include "WheelFonts.h"
#include "Settings.h"
#include "UIResources.h"
#include <array>
#include <fstream>
#include <filesystem>
#include <limits>
#include <map>
#include <stdexcept>

namespace Wheel {
    namespace {
        constexpr std::array sizes{12.f,14.f,15.f,16.f,17.f,18.f,20.f,21.f,22.f,25.f,30.f};
        ImFontGlyphRangesBuilder glyphs;
        ImVector<ImWchar> ranges;
        std::map<int, ImFont*> fonts;
        float bakedScale = 0;
        float bakedTitle=0,bakedLabel=0;
        std::vector<char> fontData;
        std::string loadedFont;
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
        ranges.clear();
        glyphs.BuildRanges(&ranges);
        io.FontDefault = nullptr;
        fonts.clear();
        if (fontChanged) {
            loadedFont=path; fontData.clear();
            std::ifstream file(std::filesystem::u8path(path), std::ios::binary | std::ios::ate);
            const auto length = file ? static_cast<std::streamoff>(file.tellg()) : 0;
            if (length > 0 && length <= std::numeric_limits<int>::max()) {
                fontData.resize(static_cast<std::size_t>(length));
                file.seekg(0);
                if (!file.read(fontData.data(), length)) fontData.clear();
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
            ImFont* font = !fontData.empty() ? io.Fonts->AddFontFromMemoryTTF(fontData.data(), static_cast<int>(fontData.size()), cfg.SizePixels, &cfg, ranges.Data) : nullptr;
            if (!font) font = io.Fonts->AddFontDefault(&cfg);
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
