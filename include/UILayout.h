#pragma once
#include "LayoutSkin.h"
#include "EquipPolicy.h"
#include <cmath>
namespace Wheel {
    inline LayoutPoint HandBadgePoint(const LayoutSkin& skin,float inner,float outer,float scale,float angle,HandEquip hand,float halfWidth) {
        // Left/right are local to the sector: counterclockwise/clockwise.
        // Single-hand labels flank the unchanged inner number; dual and
        // two-handed labels occupy the left/right corners of the outer rim.
        const bool innerBadge=hand==HandEquip::Left || hand==HandEquip::Right;
        const float side=hand==HandEquip::Left || hand==HandEquip::Dual?-1.f:1.f;
        if(innerBadge) {
            const auto number=ShapePoint(skin,inner+15*scale,angle);
            const auto base=ShapePoint(skin,inner+13*scale,angle);
            const float tx=std::cos(angle)*skin.AxisX(),ty=std::sin(angle)*skin.AxisY(),length=std::hypot(tx,ty);
            const float x=tx/length,y=ty/length;
            const float spacing=std::min((halfWidth+12*scale-std::copysign(1.f,side*x)*(base.x-number.x))/std::max(.001f,std::abs(x)),
                (19*scale-std::copysign(1.f,side*y)*(base.y-number.y))/std::max(.001f,std::abs(y)));
            return {base.x+side*x*spacing,base.y+side*y*spacing};
        }
        const float markerAngle=angle+side*(std::abs(std::cos(angle))>.6f?.21f:.24f);
        float radius=outer-14*scale;
        LayoutPoint point{};
        for(int attempt=0;attempt<4;++attempt) {
            point=ShapePoint(skin,radius,markerAngle);
            float overflow=0;
            for(float x:{-halfWidth,halfWidth})for(float y:{-9*scale,9*scale}) {
                const auto corner=WheelCoordinates(skin,point.x+x,point.y+y);
                overflow=std::max(overflow,std::hypot(corner.x,corner.y)-(outer-2*scale));
            }
            if(overflow<=0)return point;
            radius-=overflow+scale;
        }
        return ShapePoint(skin,radius,markerAngle);
    }
    inline constexpr Rect applyButton{100,235,185,42}, cancelButton{-95,235,175,42}, defaultsButton{-285,235,170,42};
    inline constexpr Rect outfitNameButton{-270,-180,540,48};
    inline constexpr Rect outfitOverwriteButton{-270,-110,260,40}, outfitDeleteButton{10,-110,260,40};
    inline constexpr Rect outfitExportButton{-270,-60,260,40};
    inline constexpr Rect outfitModeButton{10,-60,120,40};
    inline constexpr Rect outfitLooseButton{140,-60,130,40};
    inline constexpr Rect outfitEditPiecesButton{-270,-5,540,38};
    inline constexpr Rect combatRightButton{-270,-30,170,44},combatLeftButton{-85,-30,170,44},combatVoiceButton{100,-30,170,44};
    inline constexpr Rect outfitPartButton(int idx){return idx<4?Rect{-270+idx*137.f,40,132,34}:Rect{-202.5f+(idx-4)*137.f,80,132,34};}
    inline constexpr Rect generalTab{-300,-210,144,28}, controlsTab{-148,-210,144,28}, gamepadTab{4,-210,144,28}, gameplayTab{156,-210,144,28};
    inline constexpr int GeneralRows[]{0,1,2,3,4,6,7,8,9,10,28};
    inline constexpr int ControlRows[]{5,12,13,14,11,25};
    inline constexpr int GamepadRows[]{15,16,17,21,23};
    inline constexpr int GameplayRows[]{18,19,20,22,24,26,27};
    inline int SettingRow(int tab,int slot){return tab==2?GameplayRows[slot]:tab==3?GamepadRows[slot]:tab==1?ControlRows[slot]:GeneralRows[slot];}
    inline int SettingCount(int tab){return tab==2?7:tab==1?6:tab==3?5:11;}
    inline constexpr int settingsTabOrder[]{0,1,3,2};
    inline int NextSettingsTab(int tab,int direction){for(int i=0;i<4;++i)if(settingsTabOrder[i]==tab)return settingsTabOrder[Wrap(i+direction,4)];return 0;}
    inline bool BindingRow(int row){return row==5 || row==11 || row==13 || row==15;}
    inline Rect MinusButton(int row){return {40,-173.f+row*31,34,28};}
    inline Rect PlusButton(int row){return {252,-173.f+row*31,34,28};}
    inline Rect ValueButton(int row){return {80,-173.f+row*31,166,28};}
    inline int WheelSlot(float x,float y,const LayoutSkin& skin={}) {return SkinWheelSlot(x,y,skin);}
}
