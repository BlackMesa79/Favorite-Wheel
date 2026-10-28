#include "LayoutSkin.h"
#include "InputBindings.h"
#include "UILayout.h"
#include <cstdlib>
#include <iostream>

namespace {
    void Check(bool ok,const char* message){if(!ok){std::cerr<<message<<'\n';std::exit(1);}}
}
int main() {
    using namespace Wheel;
    constexpr float step=2*std::numbers::pi_v<float>/slots;
    Check(SortingShortcut(20,0)==0 && SortingShortcut(20,1)==1,"T cycles and Shift+T edits");
    Check(SortingShortcut(20,2)==-1 && SortingShortcut(20,3)==-1 && SortingShortcut(20,4)==-1 && SortingShortcut(60,0)==-1,
        "Other modifiers and F2 are not sorting shortcuts");
    WheelOpeningMemory memory;
    Check(!memory.Resolve(false,true,true),"First normal opening defaults to inventory");
    memory.Observe(true,true);
    Check(memory.Resolve(false,true,true) && !memory.Resolve(false,false,true),"Remember switch only changes the normal entrance when enabled");
    memory.Observe(false,true);
    Check(memory.Resolve(true,true,true) && memory.Resolve(true,false,true),"Dedicated actions shortcut remains direct");
    memory.Observe(true,true);memory.Observe(false,false);
    Check(!memory.Resolve(true,true,false) && memory.Resolve(false,true,true),"Restricted forms force inventory without overwriting human memory");
    memory.Reset();Check(!memory.Resolve(false,true,true),"Loading a save resets opening memory");
    Check(SettingCount(2)==6 && SettingRow(2,5)==26,"Gameplay settings expose the remembered-wheel option");
    int hits=0,placements=0;
    for(const auto shape:{WheelShape::Circle,WheelShape::Ellipse,WheelShape::Polygon})
        for(const float x:{.9f,1.2f,1.25f})for(const float y:{.9f,.94f,1.2f}) {
            LayoutSkin skin;skin.shape=shape;skin.stretchX=x;skin.stretchY=y;
            for(int slot=0;slot<slots;++slot)for(const float offset:{-.24f,-.1f,0.f,.1f,.24f}) {
                const float angle=slot*step+offset;
                const auto inside=ShapePoint(skin,189.f/224,angle),outside=ShapePoint(skin,270.f/224,angle),neutral=ShapePoint(skin,.2f,angle);
                Check(SkinWheelSlot(inside.x,inside.y,skin)==slot,"Visible sector interiors select the matching item in every shape");
                Check(SkinWheelSlot(outside.x,outside.y,skin)==-1 && SkinWheelSlot(neutral.x,neutral.y,skin)==-1,
                    "Outside and neutral pointers cannot activate a sector");
                float px=inside.x,py=inside.y;MoveWheelPointer(px,py,inside.x*100,inside.y*100,skin);
                const auto bounded=WheelCoordinates(skin,px,py);
                Check(std::abs(std::hypot(bounded.x,bounded.y)-1)<.00001f && SkinWheelSlot(px,py,skin)==slot,
                    "Mouse clamping keeps the shape boundary and selected direction");
                px=py=0;AimStick(px,py,inside.x,-inside.y);ClampWheelPointer(px,py,skin);
                Check(SkinWheelSlot(px,py,skin)==slot,"Controller rays select the same visible sector as mouse rays");
                ++hits;
            }
            for(const auto position:{DetailPosition::Auto,DetailPosition::Left,DetailPosition::Right,DetailPosition::Top,DetailPosition::Bottom,DetailPosition::Center})
                for(const auto viewport:{LayoutPoint{1280,720},LayoutPoint{2560,1440},LayoutPoint{3440,1440}})
                    for(const float scale:{.35f,.65f,1.f})for(const float height:{218.f,500.f,2500.f})
                        for(const float cx:{viewport.x*.25f,viewport.x*.5f,viewport.x*.75f}) {
                            skin.detailPosition=position;
                            const auto placed=PlaceDetail(skin,cx,viewport.y*.5f,scale,302*scale,height*scale,viewport.x,viewport.y);
                            if(placed.visible) {
                                const auto& box=placed.box;const float margin=14*scale;
                                Check(box.x>=margin && box.y>=margin && box.x+box.w<=viewport.x-margin && box.y+box.h<=viewport.y-margin,
                                    "Detail cards stay fully inside the viewport at all supported sizes");
                                for(const auto& obstacle:SkinObstacles(skin,cx,viewport.y*.5f,scale))
                                    Check(!Overlaps(box,obstacle),"Cards cannot cover the wheel, category strip, page rail or hints");
                            }
                            Check(position!=DetailPosition::Center || !placed.visible,"Explicit centre mode uses compact details");
                            ++placements;
                        }
        }
    LayoutSkin circle;
    float oldX=0,oldY=0,newX=0,newY=0;
    for(int i=0;i<500;++i) {
        const float dx=std::sin(i*.1f)*.05f,dy=std::cos(i*.17f)*.04f;
        MovePointer(oldX,oldY,dx,dy);MoveWheelPointer(newX,newY,dx,dy,circle);
        Check(std::abs(oldX-newX)<.00001f && std::abs(oldY-newY)<.00001f,"Legacy circular mouse motion remains unchanged");
    }
    LayoutSkin polygon;polygon.shape=WheelShape::Polygon;
    for(const float angle:{-.3f,-.15f,0.f,.15f,.3f}) {
        const auto point=ShapePoint(polygon,264,angle);
        Check(std::abs(point.y+264*std::cos(step*.5f))<.0001f,"Polygon face is a straight edge with the expected apothem");
    }
    const auto corner=ShapePoint(polygon,264,step*.5f);
    Check(std::abs(std::hypot(corner.x,corner.y)-264)<.0001f,"Polygon vertices retain the configured outer radius");
    for(const auto position:{DetailPosition::Left,DetailPosition::Right,DetailPosition::Top,DetailPosition::Bottom}) {
        circle.detailPosition=position;
        const auto placed=PlaceDetail(circle,1280,720,.5f,151,109,2560,1440);
        Check(placed.visible && placed.position==position,"Requested position is honored when it has room");
    }
    circle.detailPosition=DetailPosition::Right;
    const auto opposite=PlaceDetail(circle,1100,360,.65f,196,200,1280,720);
    Check(opposite.visible && opposite.position==DetailPosition::Left,"A clipped preferred side falls back across the wheel");
    Check(!PlaceDetail(circle,640,360,1,900,800,1280,720).visible,"Oversized cards fall back to centre instead of spilling off screen");
    circle.detailPosition=DetailPosition::Auto;
    const auto legacy=PlaceDetail(circle,700,400,1,278,218,1920,1080);
    Check(legacy.visible && legacy.box.x==1046 && legacy.box.y==291,"Legacy automatic card coordinates are preserved when they fit");
    std::cout<<"Layout skins: "<<hits<<" sector/pointer cases and "<<placements<<" card/viewport cases passed\n";
    int badges=0;
    for(const auto shape:{WheelShape::Circle,WheelShape::Ellipse,WheelShape::Polygon})
        for(float x:{.9f,1.f,1.25f})for(float y:{.9f,1.f,1.2f})for(float scale:{.432f,1.f,2.16f})
            for(auto hand:{HandEquip::Left,HandEquip::Right,HandEquip::Dual,HandEquip::TwoHanded})
                for(int variant=0;variant<2;++variant)for(int slot=0;slot<slots;++slot) {
                const bool innerBadge=hand==HandEquip::Left || hand==HandEquip::Right;
                const float halfHeight=innerBadge?7.f:9.f;
                const float half=innerBadge?(variant?8.f:6.f):hand==HandEquip::Dual?(variant?14.f:12.f):(variant?12.f:10.f);
                LayoutSkin skin;skin.shape=shape;skin.stretchX=x;skin.stretchY=y;
                const float angle=slot*step,outer=264*scale;
                const auto at=HandBadgePoint(skin,116*scale,outer,scale,angle,hand,half*scale);
                const Rect badge{at.x-half*scale,at.y-halfHeight*scale,2*half*scale,2*halfHeight*scale};
                for(float dx:{-half,half})for(float dy:{-halfHeight,halfHeight}) {
                    const auto point=WheelCoordinates(skin,at.x+dx*scale,at.y+dy*scale);
                    Check(std::hypot(point.x,point.y)<=outer-scale,"Full hand badge remains inside the wheel rim");
                    Check(std::hypot(point.x,point.y)>=117*scale,"Hand marker stays outside the wheel centre");
                    if(SkinWheelSlot((at.x+dx*scale)/(224*scale),(at.y+dy*scale)/(224*scale),skin)!=slot)
                        std::cerr<<"badge corner: shape="<<int(shape)<<" hand="<<int(hand)<<" x="<<x<<" y="<<y<<" half="<<half<<" slot="<<slot<<'\n';
                    Check(SkinWheelSlot((at.x+dx*scale)/(224*scale),(at.y+dy*scale)/(224*scale),skin)==slot,
                        "Every badge corner remains in its own sector");
                }
                const auto content=ShapePoint(skin,189*scale,angle),key=ShapePoint(skin,131*scale,angle);
                const Rect status{content.x-28*scale,content.y+30*scale,56*scale,12*scale}; // Quantity, up to x99999.
                const float nameWidth=innerBadge?88.f:102.f;
                const Rect name{content.x-nameWidth*.5f*scale*std::min(1.f,skin.AxisX()),content.y+7*scale,
                    nameWidth*scale*std::min(1.f,skin.AxisX()),16*scale};
                const Rect icon{content.x-24*scale,content.y-43*scale,48*scale,48*scale};
                const Rect keycap{key.x-11*scale,key.y-11*scale,22*scale,22*scale}; // 2-unit clearance around the visible keycap.
                if(Overlaps(badge,status) || Overlaps(badge,name) || Overlaps(badge,icon) || Overlaps(badge,keycap))
                    std::cerr<<"badge collision: shape="<<int(shape)<<" hand="<<int(hand)<<" x="<<x<<" y="<<y<<" scale="<<scale<<" half="<<half<<" slot="<<slot
                        <<" status="<<Overlaps(badge,status)<<" name="<<Overlaps(badge,name)<<" icon="<<Overlaps(badge,icon)<<'\n';
                Check(!Overlaps(badge,status) && !Overlaps(badge,name) && !Overlaps(badge,icon) && !Overlaps(badge,keycap),
                    "Hand marker stays clear of the name/status/icon and inner quick-slot keycap");
                const auto normalized=WheelCoordinates(skin,at.x,at.y);
                const float markerAngle=std::atan2(normalized.x,-normalized.y),relative=std::remainder(markerAngle-angle,2*std::numbers::pi_v<float>);
                Check((hand==HandEquip::Left || hand==HandEquip::Dual)?relative<0:relative>0,"Requested local left/right corner is consistent across all sectors");
                Check(innerBadge?std::hypot(normalized.x,normalized.y)<145*scale:std::hypot(normalized.x,normalized.y)>230*scale,
                    "L/R use the inner edge; L+R/2H use the outer edge");
                const auto arrowLeft=ShapePoint(skin,261*scale,angle-.019f),arrowTip=ShapePoint(skin,256*scale,angle),
                    arrowRight=ShapePoint(skin,261*scale,angle+.019f);
                const float minX=std::min({arrowLeft.x,arrowTip.x,arrowRight.x}),minY=std::min({arrowLeft.y,arrowTip.y,arrowRight.y}),
                    maxX=std::max({arrowLeft.x,arrowTip.x,arrowRight.x}),maxY=std::max({arrowLeft.y,arrowTip.y,arrowRight.y});
                Check(!Overlaps(badge,{minX,minY,maxX-minX,maxY-minY}),"Hand badge remains clear of the complete hover arrow");++badges;
            }
    std::cout<<badges<<" hand marker layout cases passed\n";
}
