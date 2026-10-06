#pragma once
#include "Favorites.h"
#include <imgui.h>
#include <initializer_list>

namespace Wheel {
// Original, code-drawn vector pictograms. Coordinates use a 40 x 40 design box.
// Only value data reaches the renderer; icon selection never queries game objects.
inline void DrawIcon(ImDrawList* d, ImVec2 origin, const Item& item, float s, ImU32 ink) {
    auto p=[&](float x,float y){return ImVec2{origin.x+x*s,origin.y+y*s};};
    const float stroke=1.8f*s;
    auto line=[&](float x,float y,float a,float b){d->AddLine(p(x,y),p(a,b),ink,stroke);};
    auto path=[&](std::initializer_list<ImVec2> points){
        for(auto v:points)d->PathLineTo(p(v.x,v.y));
        d->PathStroke(ink,ImDrawFlags_None,stroke);
    };
    auto curve=[&](ImVec2 a,ImVec2 b,ImVec2 c,ImVec2 e){d->AddBezierCubic(p(a.x,a.y),p(b.x,b.y),p(c.x,c.y),p(e.x,e.y),ink,stroke);};
    auto shape=[&](std::initializer_list<ImVec2> points){
        for(auto v:points)d->PathLineTo(p(v.x,v.y));
        d->PathStroke(ink,ImDrawFlags_Closed,stroke);
    };
    auto circle=[&](float x,float y,float r){d->AddCircle(p(x,y),r*s,ink,32,stroke);};
    auto diamond=[&](float x,float y,float r){shape({{x,y-r},{x+r,y},{x,y+r},{x-r,y}});};
    auto torso=[&](){shape({{-7,-15},{-15,-11},{-18,-3},{-11,0},{-10,15},{10,15},{11,0},{18,-3},{15,-11},{7,-15},{5,-9},{-5,-9}});};
    if(item.action==ActionKind::SaveOutfit) {
        shape({{-14,-16},{9,-16},{15,-10},{15,16},{-14,16}});
        path({{-7,-16},{-7,-5},{6,-5},{6,-16}});
        line(-6,6,6,6);line(0,0,0,12);return;
    }
    if(item.action==ActionKind::ImportOutfits) {
        shape({{-16,3},{-16,15},{16,15},{16,3},{8,3},{5,8},{-5,8},{-8,3}});
        line(0,-17,0,1);path({{-6,-5},{0,1},{6,-5}});return;
    }
    if(item.action==ActionKind::Outfit) {
        // Open hook flows into the centred neck; both shoulders share exact endpoints.
        curve({-5,-13},{-5,-21},{8,-21},{5,-12});
        curve({5,-12},{4,-10},{0,-10},{0,-6});
        shape({{0,-6},{17,7},{17,11},{-17,11},{-17,7}});return;
    }
    auto kind=item.icon;
    if(kind==IconKind::Auto) {
        constexpr IconKind fallback[]={IconKind::Sword,IconKind::Armor,IconKind::Potion,IconKind::Food,IconKind::Magic,IconKind::Other};
        kind=fallback[static_cast<int>(item.category)];
    }
    switch(kind) {
    case IconKind::LightPlayer:case IconKind::LightTarget:case IconKind::LightGroup:
        circle(0,-7,6);curve({-11,16},{-10,1},{10,1},{11,16});
        if(kind==IconKind::LightGroup) {
            curve({-11,-13},{-21,-14},{-21,-1},{-13,-1});curve({11,-13},{21,-14},{21,-1},{13,-1});
            line(-15,5,-18,14);line(15,5,18,14);
        } else {line(-17,-10,-14,-8);line(14,-8,17,-10);line(-17,1,-14,0);line(14,0,17,1);}
        if(kind==IconKind::LightTarget){line(0,-20,0,-16);line(-4,-18,4,-18);}
        break;
    case IconKind::Back:
        path({{-1,-12},{-13,0},{-1,12}});line(-13,0,15,0);break;
    case IconKind::Sword:case IconKind::Dagger: {
        const float tip=kind==IconKind::Dagger?-15.f:-19.f;
        shape({{0,tip},{5,tip+7},{3,6},{-3,6},{-5,tip+7}});
        line(0,tip+6,0,5);line(-9,7,9,7);line(0,8,0,16);diamond(0,18,2);break;
    }
    case IconKind::Axe:
        line(-3,-18,-3,18);shape({{-3,-12},{3,-12},{10,-18},{15,-12},{17,-5},{15,2},{10,6},{3,-3},{-3,-3}});line(-6,14,0,14);break;
    case IconKind::Mace:
        // Flanged head, collar and closed grip instead of a polygon on a stick.
        shape({{-4,-18},{4,-18},{10,-12},{10,-6},{5,-1},{-5,-1},{-10,-6},{-10,-12}});
        path({{-3,-15},{-5,-10},{-3,-4}});path({{3,-15},{5,-10},{3,-4}});
        shape({{-3,-1},{3,-1},{3,4},{-3,4}});
        shape({{-2,4},{2,4},{3,17},{-3,17}});line(-3,13,3,13);break;
    case IconKind::Bow:
        d->AddBezierCubic(p(-8,-18),p(20,-12),p(20,12),p(-8,18),ink,stroke);
        path({{-8,-18},{-3,0},{-8,18}});line(-16,0,18,0);path({{12,-5},{18,0},{12,5}});break;
    case IconKind::Crossbow:
        shape({{-3,-8},{3,-8},{3,18},{-3,18}});line(-17,-9,0,-15);line(0,-15,17,-9);line(-17,-9,0,2);line(0,2,17,-9);line(0,-19,0,0);break;
    case IconKind::Staff:
        // Symmetric crystal cradle with a continuous double-sided shaft.
        diamond(0,-12,5);
        path({{-8,-16},{-8,-9},{-4,-4},{4,-4},{8,-9},{8,-16}});
        shape({{-2,-4},{2,-4},{2,18},{-2,18}});line(-2,12,2,12);break;
    case IconKind::Arrow:
        {
        // Construct in one axial coordinate system, then rotate the whole arrow.
        auto a=[](float x,float y){return ImVec2{(x-y)*.70710678f,(x+y)*.70710678f};};
        shape({a(0,-20),a(6,-9),a(0,-11),a(-6,-9)});
        const auto head=a(0,-11),tail=a(0,19);line(head.x,head.y,tail.x,tail.y);
        path({a(-5,7),a(-5,14),a(0,18),a(5,14),a(5,7)});
        path({a(-5,7),a(0,11),a(5,7)});break;
    }
    case IconKind::Armor:
        torso();line(-7,-2,0,1);line(0,1,7,-2);line(0,1,0,8);line(-8,10,8,10);break;
    case IconKind::Robe:
        shape({{-6,-16},{-15,-11},{-18,0},{-11,3},{-8,-3},{-6,2},{-13,17},{13,17},{6,2},{8,-3},{11,3},{18,0},{15,-11},{6,-16},{0,-10}});
        line(-5,2,5,2);line(0,-9,0,0);line(-2,5,-5,13);line(2,5,5,13);break;
    case IconKind::Helmet:
        // Rounded skull, continuous cheek guards and a readable T-shaped visor.
        curve({-14,-2},{-14,-22},{14,-22},{14,-2});
        path({{-14,-2},{-13,12},{-6,16},{-6,4},{-10,0},{-3,0},{-3,9},{3,9},{3,0},{10,0},{6,4},{6,16},{13,12},{14,-2}});
        line(0,-13,0,-5);break;
    case IconKind::Gloves:
        shape({{-9,17},{-12,3},{-15,-3},{-11,-6},{-6,0},{-6,-14},{-2,-16},{1,-13},{5,-15},{9,-12},{11,-6},{10,7},{7,17}});
        line(-8,10,8,10);line(1,-10,1,-2);line(6,-9,6,-2);break;
    case IconKind::Boots:
        shape({{-8,-16},{6,-16},{5,4},{14,8},{16,14},{14,17},{-10,17},{-11,10}});
        line(-8,-10,5,-10);line(-9,12,14,12);line(-4,-5,3,-3);line(-5,1,3,3);break;
    case IconKind::Ring:
        curve({-6,-4},{-19,4},{-9,19},{0,17});curve({0,17},{9,19},{19,4},{6,-4});diamond(0,-10,7);path({{-6,-4},{0,-2},{6,-4}});break;
    case IconKind::Amulet:
        d->AddBezierCubic(p(-12,-17),p(-16,0),p(-4,1),p(0,5),ink,stroke);
        d->AddBezierCubic(p(12,-17),p(16,0),p(4,1),p(0,5),ink,stroke);
        diamond(0,10,8);circle(0,10,2);break;
    case IconKind::Shield:
        shape({{0,-17},{14,-12},{12,6},{7,13},{0,18},{-7,13},{-12,6},{-14,-12}});
        line(0,-12,0,12);line(-9,-5,9,-5);break;
    case IconKind::Potion:
        shape({{-5,-14},{5,-14},{5,-5},{12,2},{13,11},{8,17},{-8,17},{-13,11},{-12,2},{-5,-5}});
        line(-6,-18,6,-18);line(-6,-14,6,-14);line(0,2,0,12);line(-5,7,5,7);break;
    case IconKind::Food:
        shape({{0,-8},{-7,-12},{-13,-8},{-15,0},{-12,10},{-6,16},{0,14},{6,16},{12,10},{15,0},{13,-8},{7,-12}});
        line(0,-8,2,-17);shape({{2,-15},{8,-19},{12,-16},{6,-12}});line(-9,-4,-10,2);break;
    case IconKind::Magic:
        diamond(0,0,12);diamond(0,0,5);line(0,-20,0,-16);line(0,16,0,20);line(-20,0,-16,0);line(16,0,20,0);
        line(-13,-13,-10,-10);line(10,10,13,13);line(10,-10,13,-13);line(-13,13,-10,10);break;
    case IconKind::Scroll:
        shape({{-10,-14},{10,-14},{10,11},{6,16},{-10,16},{-10,-8},{-15,-8},{-15,-12}});
        line(-10,-8,-10,-12);line(-5,-5,5,-5);line(-5,0,5,0);line(-5,5,2,5);line(-8,11,10,11);break;
    case IconKind::Torch:
        shape({{-4,2},{4,2},{2,18},{-2,18}});shape({{0,-19},{2,-10},{7,-14},{10,-5},{5,0},{-5,0},{-9,-5},{-6,-13},{-4,-8}});break;
    default:
        shape({{-8,-11},{8,-11},{7,-5},{14,4},{14,12},{8,17},{-8,17},{-14,12},{-14,4},{-7,-5}});
        line(-7,-6,7,-6);line(-8,-16,8,-16);line(-8,-16,-5,-11);line(8,-16,5,-11);break;
    }
}
}
