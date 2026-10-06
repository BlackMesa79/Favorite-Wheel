#pragma once
#include "WheelLogic.h"
#include <cmath>
namespace Wheel {
    struct Rect {
        float x,y,w,h;
        bool Contains(float px,float py) const {return px>=x && px<=x+w && py>=y && py<=y+h;}
    };
    inline constexpr Rect applyButton{100,235,185,42}, cancelButton{-95,235,175,42}, defaultsButton{-285,235,170,42};
    inline constexpr Rect outfitNameButton{-270,-180,540,48};
    inline constexpr Rect outfitOverwriteButton{-270,-95,260,44}, outfitDeleteButton{10,-95,260,44};
    inline constexpr Rect outfitExportButton{-270,-30,540,44};
    inline constexpr int settingRows=12;
    inline Rect MinusButton(int row){return {40,-212.f+row*31,34,28};}
    inline Rect PlusButton(int row){return {252,-212.f+row*31,34,28};}
    inline Rect ValueButton(int row){return {80,-212.f+row*31,166,28};}
    inline int WheelSlot(float x,float y) {return x*x+y*y>(264.f/224)*(264.f/224)?-1:HitTest(x,y);}
}
