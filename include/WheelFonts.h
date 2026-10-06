#pragma once
#include "Wheel.h"
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
        return std::min(std::min(width/1280.f,height/900.f)*view.config.wheelScale*.82f,
            std::min(width/810.f,height/820.f));
    }
    inline ImVec2 ViewCentre(float width,float height,const View& view) {
        const float s=ViewScale(width,height,view);
        return (view.settingsOpen || view.outfitDialog)?ImVec2(width*.5f,height*.5f):
            ImVec2(std::clamp(width*view.config.positionX/100.f,405*s,width-405*s),std::clamp(height*view.config.positionY/100.f,410*s,height-410*s));
    }
    // Render-thread only, before NewFrame; true means the GPU atlas needs rebuilding.
    bool PrepareFonts(const View& view, float scale);
    ImFont* FontAt(float pixelSize);
}
