#pragma once
#include "Settings.h"
#include "LayoutSkin.h"
#include <cstdint>
#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
namespace Wheel {
    struct Language { std::string id, name, font; std::unordered_map<std::string,std::string> text; };
    enum class ThemeStyle { Etched, ClassicGold, Skyrim };
    enum class FrameStyle { Plain, Nordic };
    struct Theme {
        std::string id="classic", name="Classic Gold", font;
        ThemeStyle style=ThemeStyle::ClassicGold; // Component shapes, independent of colors and layout.
        FrameStyle frameStyle=FrameStyle::Plain;
        LayoutSkin layout;
        std::string iconPack; // Resolved IconPacks/<id>/icons.ini; empty uses built-in icons.
        std::string surfaceTexture; // Resolved local PNG; empty means the solid-color fallback.
        float materialStrength=.85f, materialZoom=1.f;
        std::uint32_t accent=0xFF9CD6EF, text=0xFFE0E9F2, muted=0xFFA1ADB9;
        std::uint32_t sector=0xFF222326, empty=0xFF17191C, hover=0xFF344A60;
        std::uint32_t panel=0xFF191C21, background=0xFF000000, border=0xFF597B9C;
        // Optional visual tokens. Old themes inherit these; no game behavior lives here.
        float borderWidth=1.f, cornerRadius=6.f, ornament=.8f, relief=.65f;
        float textShadow=.32f, iconScale=1.f, titleScale=1.f, labelScale=1.f;
        float hoverDuration=.10f, pageDuration=.12f;
    };
    void LoadResources(const std::string& root="Data/SKSE/Plugins/FavoriteWheel");
    const std::string& ResourceLoadDiagnostic(); // Startup-only diagnostic; empty on success.
    const std::vector<Language>& Languages();
    std::string ResolveLanguage(const std::string& requested,const std::string& systemLocale);
    std::string ActiveLanguage(const Settings& config);
    const std::string& SystemLanguage();
    std::string LanguageLabel(const Settings& config);
    const std::vector<Theme>& Themes();
    const Theme& Style(const Settings& config);
    std::string Tr(const Settings& config, const std::string& key);
    std::string UIGlyphs(const Settings& config);
    std::shared_ptr<const std::string> UIGlyphSnapshot(const Settings& config);
    std::string FontPath(const Settings& config);
    const std::vector<std::string>& FallbackFontPaths(); // Installed Windows fonts, resolved at resource load.
    std::string KeyLabel(const Settings& config);
    std::string ModifierLabel(const Settings& config,int modifier);
    std::string PadLabel(const Settings& config,int key,bool follow=false);
    std::string CycleLanguage(const std::string& id,int delta);
    std::string CycleTheme(const std::string& id,int delta);
}
