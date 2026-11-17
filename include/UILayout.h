#pragma once
#include "LayoutSkin.h"
#include "EquipPolicy.h"
#include <cmath>
#include <array>
#include <span>
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
    inline constexpr Rect outfitOverwriteButton{-270,-95,260,44}, outfitDeleteButton{10,-95,260,44};
    inline constexpr Rect outfitEditButton{-270,-30,260,44},outfitShareButton{10,-30,260,44};
    inline constexpr Rect outfitDraftList{-300,-105,290,200},outfitInventoryList{10,-105,290,200};
    inline constexpr Rect outfitEditSaveButton{100,254,185,36},outfitEditCancelButton{-95,254,175,36};
    inline constexpr float outfitEditMessageY=218,outfitEditDividerY=236,outfitEditHelpY=308;
    inline constexpr Rect outfitMissingButton{-300,166,600,32};
    inline constexpr Rect outfitRemoveButton{-300,122,290,32},outfitAddButton{10,122,290,32};
    inline constexpr Rect OutfitFilterTab(int index){return {-300.f+index*86,-174,84,30};}
    inline constexpr Rect outfitExportButton{-270,-30,540,44};
    inline constexpr Rect combatRightButton{-270,-30,170,44},combatLeftButton{-85,-30,170,44},combatVoiceButton{100,-30,170,44};
    inline constexpr Rect generalTab{-300,-210,112,28},iconsTab{-178,-210,112,28},
        controlsTab{-56,-210,112,28},gamepadTab{66,-210,112,28},gameplayTab{188,-210,112,28};
    inline constexpr std::array settingsTabs{generalTab,controlsTab,gameplayTab,gamepadTab,iconsTab};
    inline int SettingsTabAt(float x,float y) {
        for(int tab=0;tab<static_cast<int>(settingsTabs.size());++tab)if(settingsTabs[tab].Contains(x,y))return tab;
        return -1;
    }
    inline constexpr int GeneralRows[]{0,1,2,3,4,6,7,8,9,10,28};
    inline constexpr int ControlRows[]{5,12,13,14,11,25};
    inline constexpr int GamepadRows[]{15,16,17,21,23};
    inline constexpr int GameplayRows[]{18,19,20,22,24,26,27,29,30,31};
    inline constexpr int IconRows[]{32,33,34};
    inline constexpr std::array<std::span<const int>,5> settingsRows{
        GeneralRows,ControlRows,GameplayRows,GamepadRows,IconRows};
    inline int SettingRow(int tab,int slot){return settingsRows[tab][slot];}
    inline int SettingCount(int tab){return static_cast<int>(settingsRows[tab].size());}
    inline constexpr int settingsTabOrder[]{0,4,1,3,2};
    inline int NextSettingsTab(int tab,int direction){for(int i=0;i<5;++i)if(settingsTabOrder[i]==tab)return settingsTabOrder[Wrap(i+direction,5)];return 0;}
    inline bool BindingRow(int row){return row==5 || row==11 || row==13 || row==15 || row==30 || row==31;}
    inline constexpr int SettingStride(int tab){return tab==0||tab==2?28:31;}
    inline constexpr int SettingHeight(int){return 26;}
    inline Rect MinusButton(int row,int tab=0){return {40,-173.f+row*SettingStride(tab),34,float(SettingHeight(tab))};}
    inline Rect PlusButton(int row,int tab=0){return {252,-173.f+row*SettingStride(tab),34,float(SettingHeight(tab))};}
    inline Rect ValueButton(int row,int tab=0){return {80,-173.f+row*SettingStride(tab),166,float(SettingHeight(tab))};}
    inline int WheelSlot(float x,float y,const LayoutSkin& skin={}) {return SkinWheelSlot(x,y,skin);}
}
