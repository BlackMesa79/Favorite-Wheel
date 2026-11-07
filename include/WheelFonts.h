#pragma once
#include "Wheel.h"
#include "UIResources.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>
namespace Wheel {
    inline float LayoutScale(float width, float height, float userScale) {
        return std::min(std::min(width / 1280.0f, height / 900.0f) * userScale,
            std::min(width / 800.0f, height / 830.0f));
    }
    inline float ViewScale(float width,float height,const View& view) {
        if(view.settingsOpen || view.outfitDialog) return LayoutScale(width,height,view.config.scale);
        const auto& skin=Style(view.config).layout;
        return std::min(std::min(width/1280.f,height/900.f)*view.config.wheelScale*.82f,
            std::min(width/(2*SkinHalfWidth(skin)),height/(2*SkinHalfHeight(skin))));
    }
    inline ImVec2 ViewCentre(float width,float height,const View& view) {
        const float s=ViewScale(width,height,view);
        if(view.settingsOpen || view.outfitDialog)return {width*.5f,height*.5f};
        const auto& skin=Style(view.config).layout;
        const float x=SkinHalfWidth(skin)*s,y=SkinHalfHeight(skin)*s;
        return {std::clamp(width*view.config.positionX/100.f,x,width-x),std::clamp(height*view.config.positionY/100.f,y,height-y)};
    }
    // Render-thread only, before NewFrame; true means the GPU atlas needs rebuilding.
    bool PrepareFonts(const View& view, float scale);
    // Deduplicate catalog text before publishing it to the per-frame view.
    // Uses only local state and can run on the inventory-collection thread.
    std::string CatalogFontGlyphs(const std::vector<Item>& items,const std::string& extra={});
    ImFont* FontAt(float pixelSize);
    const std::string& PreparedFontPath(); // Actual primary font, or empty for the emergency pixel font.
}
