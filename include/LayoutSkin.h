#pragma once
#include "WheelLogic.h"
#include <array>

namespace Wheel {
    enum class WheelShape { Circle, Ellipse, Polygon };
    enum class DetailPosition { Auto, Left, Right, Top, Bottom, Center };
    struct LayoutSkin {
        WheelShape shape=WheelShape::Circle;
        float stretchX=1.2f,stretchY=.94f; // Used only by ellipse shapes.
        DetailPosition detailPosition=DetailPosition::Auto;
        float detailGap=20.f;
        float AxisX() const {return shape==WheelShape::Ellipse?stretchX:1.f;}
        float AxisY() const {return shape==WheelShape::Ellipse?stretchY:1.f;}
        float ExtraX() const {return std::max(0.f,274.f*(AxisX()-1.f));}
        float ExtraY() const {return std::max(0.f,274.f*(AxisY()-1.f));}
    };
    struct LayoutPoint {float x,y;};
    struct Rect {
        float x,y,w,h;
        bool Contains(float px,float py) const {return px>=x && px<=x+w && py>=y && py<=y+h;}
    };
    inline bool Overlaps(const Rect& a,const Rect& b) {
        return a.x<b.x+b.w && a.x+a.w>b.x && a.y<b.y+b.h && a.y+a.h>b.y;
    }
    // Polygon faces are centred on the ten item directions. Both rendering
    // and input use this radial boundary, with vertices between adjacent slots.
    inline float ShapeRadius(const LayoutSkin& skin,float angle) {
        if(skin.shape!=WheelShape::Polygon)return 1.f;
        constexpr float step=2*std::numbers::pi_v<float>/slots;
        const float local=std::remainder(angle,step);
        return std::cos(step*.5f)/std::cos(local);
    }
    inline LayoutPoint ShapePoint(const LayoutSkin& skin,float radius,float angle) {
        radius*=ShapeRadius(skin,angle);
        return {std::sin(angle)*radius*skin.AxisX(),-std::cos(angle)*radius*skin.AxisY()};
    }
    inline LayoutPoint WheelCoordinates(const LayoutSkin& skin,float x,float y) {
        x/=skin.AxisX();y/=skin.AxisY();
        const float boundary=ShapeRadius(skin,std::atan2(x,-y));
        return {x/boundary,y/boundary};
    }
    inline int SkinWheelSlot(float x,float y,const LayoutSkin& skin) {
        const auto p=WheelCoordinates(skin,x,y);
        return p.x*p.x+p.y*p.y>(264.f/224)*(264.f/224)?-1:HitTest(p.x,p.y);
    }
    inline void ClampWheelPointer(float& x,float& y,const LayoutSkin& skin) {
        const auto p=WheelCoordinates(skin,x,y);
        const float length=std::hypot(p.x,p.y);
        if(length>1.f){x/=length;y/=length;}
    }
    inline void MoveWheelPointer(float& x,float& y,float dx,float dy,const LayoutSkin& skin) {
        x+=dx;y+=dy;ClampWheelPointer(x,y,skin);
    }
    // Navigation keeps its existing structure; expanded wheels reserve extra
    // room so the top strip, right page rail and bottom hints remain outside it.
    inline float SkinHalfWidth(const LayoutSkin& skin){return 405.f+skin.ExtraX();}
    inline float SkinHalfHeight(const LayoutSkin& skin){return 410.f+skin.ExtraY();}
    inline std::array<Rect,4> SkinObstacles(const LayoutSkin& skin,float cx,float cy,float s) {
        const float x=skin.ExtraX(),y=skin.ExtraY();
        return {{
            {cx-274*s*skin.AxisX(),cy-274*s*skin.AxisY(),548*s*skin.AxisX(),548*s*skin.AxisY()},
            {cx-405*s,cy-(334+y)*s,810*s,50*s},
            {cx+(286+x)*s,cy-155*s,40*s,310*s},
            {cx-282*s,cy+(278+y)*s,564*s,120*s}
        }};
    }
    struct DetailPlacement {
        Rect box{};
        DetailPosition position=DetailPosition::Center;
        bool visible=false; // Center uses the existing compact wheel summary.
    };
    inline DetailPlacement PlaceDetail(const LayoutSkin& skin,float cx,float cy,float s,
        float width,float height,float viewportWidth,float viewportHeight) {
        if(skin.detailPosition==DetailPosition::Center)return {};
        const float margin=14*s,gap=skin.detailGap-20.f;
        if(width>viewportWidth-2*margin || height>viewportHeight-2*margin)return {};
        const auto obstacles=SkinObstacles(skin,cx,cy,s);
        const auto tryPosition=[&](DetailPosition position) {
            Rect rect{};
            if(position==DetailPosition::Left || position==DetailPosition::Right) {
                rect={position==DetailPosition::Left?cx-(304+skin.ExtraX()+gap)*s-width:cx+(346+skin.ExtraX()+gap)*s,
                    std::clamp(cy-height*.5f,margin,viewportHeight-margin-height),width,height};
            }else {
                rect={std::clamp(cx-width*.5f,margin,viewportWidth-margin-width),
                    position==DetailPosition::Top?cy-(346+skin.ExtraY()+gap)*s-height:cy+(410+skin.ExtraY()+gap)*s,width,height};
            }
            const bool fits=rect.x>=margin && rect.y>=margin && rect.x+width<=viewportWidth-margin &&
                rect.y+height<=viewportHeight-margin;
            bool clear=fits;for(const auto& obstacle:obstacles)clear=clear && !Overlaps(rect,obstacle);
            return DetailPlacement{rect,position,clear};
        };
        // Honor the requested side first; use available sides before falling
        // back to the compact centre, rather than clipping or covering controls.
        if(skin.detailPosition!=DetailPosition::Auto) {
            auto requested=tryPosition(skin.detailPosition);if(requested.visible)return requested;
        }
        for(const auto position:{DetailPosition::Right,DetailPosition::Left,DetailPosition::Top,DetailPosition::Bottom}) {
            auto placement=tryPosition(position);if(placement.visible)return placement;
        }
        return {};
    }
}
