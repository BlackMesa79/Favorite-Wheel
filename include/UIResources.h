#pragma once
#include "Settings.h"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
namespace Wheel {
    struct Language { std::string id, name, font; std::unordered_map<std::string,std::string> text; };
    struct Theme {
        std::string id="classic", name="Classic", font;
        std::uint32_t accent=0xFF8BC9E7, text=0xFFEAEEED, muted=0xFFB5A89E;
        std::uint32_t sector=0xF02C2017, empty=0xBE1F160F, hover=0xEB3D5B6B;
        std::uint32_t panel=0xFC241C15, background=0xFF000000, border=0xFF70604B;
        // Optional visual tokens. Old themes inherit these; no game behavior lives here.
        float borderWidth=1.f, cornerRadius=10.f, ornament=.65f, relief=.5f;
        float textShadow=.32f, iconScale=1.f, titleScale=1.f, labelScale=1.f;
        float hoverDuration=.10f, pageDuration=.12f;
    };
    void LoadResources(const std::string& root="Data/SKSE/Plugins/FavoriteWheel");
    const std::vector<Language>& Languages();
    const std::vector<Theme>& Themes();
    const Theme& Style(const Settings& config);
    std::string Tr(const Settings& config, const std::string& key);
    std::string UIGlyphs(const Settings& config);
    std::string FontPath(const Settings& config);
    std::string KeyLabel(const Settings& config);
    std::string CycleLanguage(const std::string& id,int delta);
    std::string CycleTheme(const std::string& id,int delta);
}
