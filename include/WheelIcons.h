#pragma once
#include "Favorites.h"
#include <imgui.h>
#include <initializer_list>

namespace Wheel {
// North Etch: original outline pictograms, centred in a 40 x 40 design box.
// One silhouette and a restrained fine engraving; no fills, textures or game access.
inline void DrawIcon(ImDrawList* d, ImVec2 origin, const Item& item, float s, ImU32 ink) {
    if (s <= 0.f) return;
    auto p = [&](float x, float y) { return ImVec2{origin.x + x*s, origin.y + y*s}; };
    const float stroke = 1.75f*s;
    const float fine = 1.15f*s;
    auto line = [&](float x, float y, float a, float b) { d->AddLine(p(x,y),p(a,b),ink,stroke); };
    auto etch = [&](float x, float y, float a, float b) { d->AddLine(p(x,y),p(a,b),ink,fine); };
    auto path = [&](std::initializer_list<ImVec2> points) {
        for (auto v : points) d->PathLineTo(p(v.x,v.y));
        d->PathStroke(ink,ImDrawFlags_None,stroke);
    };
    auto shape = [&](std::initializer_list<ImVec2> points) {
        for (auto v : points) d->PathLineTo(p(v.x,v.y));
        d->PathStroke(ink,ImDrawFlags_Closed,stroke);
    };
    // Build curved contours as a single stroked path so their joins stay clean.
    auto start = [&](float x, float y) { d->PathLineTo(p(x,y)); };
    auto to = [&](float x, float y) { d->PathLineTo(p(x,y)); };
    auto bend = [&](float x, float y, float a, float b, float u, float v) {
        d->PathBezierCubicCurveTo(p(x,y),p(a,b),p(u,v));
    };
    auto end = [&](bool closed = false) { d->PathStroke(ink,closed ? ImDrawFlags_Closed : ImDrawFlags_None,stroke); };
    auto circle = [&](float x, float y, float r) { d->AddCircle(p(x,y),r*s,ink,24,stroke); };
    auto gem = [&](float x, float y, float r) { shape({{x,y-r},{x+r*.72f,y},{x,y+r},{x-r*.72f,y}}); };
    auto hanger = [&]() {
        // Circular hook with a vertical stem on the shoulder centreline.
        start(-5,-13); bend(-5,-19,5,-19,5,-13);
        bend(5,-10,0,-10,0,-7); to(0,-4); end();
        start(0,-4); to(16,6); bend(18,8,17,11,14,11);
        to(-14,11); bend(-17,11,-18,8,-16,6); end(true);
    };
    if (item.action == ActionKind::SaveOutfit) {
        // Clothing silhouette with an unobstructed add badge at the lower right.
        start(-5,-15); bend(-5,-9,5,-9,5,-15); to(13,-11); to(17,-3);
        to(10,0); to(8,-4); to(8,2); end();
        path({{-5,-15},{-13,-11},{-17,-3},{-10,0},{-8,-4},{-8,15},{1,15}});
        line(6,11,18,11); line(12,5,12,17); return;
    }
    if (item.action == ActionKind::ImportOutfits) {
        path({{-15,2},{-15,13},{-12,16},{12,16},{15,13},{15,2}});
        line(0,-17,0,6); path({{-7,-1},{0,6},{7,-1}}); etch(-8,11,8,11); return;
    }
    if (item.action == ActionKind::Outfit) { hanger(); return; }
    auto kind = item.icon;
    if (kind == IconKind::Auto) {
        constexpr IconKind fallback[] = {IconKind::Sword,IconKind::Armor,IconKind::Potion,IconKind::Food,IconKind::Magic,IconKind::Other};
        kind = fallback[static_cast<int>(item.category)];
    }
    switch (kind) {
    case IconKind::Sword:
        path({{-3,5},{-4,-12},{0,-19},{4,-12},{3,5}});
        etch(0,-12,0,2);
        path({{-9,2},{-7,5},{7,5},{9,2}});
        path({{-2,5},{-2,14},{-3,16},{0,19},{3,16},{2,14},{2,5}});
        etch(-2,13,2,13); break;
    case IconKind::Dagger: {
        // Blade, guard and grip share one rotated axis; the fuller follows the sweep.
        auto a = [](float x,float y) { return ImVec2{(x-y)*.70710678f,(x+y)*.70710678f}; };
        auto q = [&](float x,float y) { auto v=a(x,y); return p(v.x,v.y); };
        d->PathLineTo(q(-3,3));
        d->PathBezierCubicCurveTo(q(-5,-4),q(-2,-13),q(5,-19));
        d->PathBezierCubicCurveTo(q(2,-10),q(5,-3),q(3,3));
        d->PathStroke(ink,ImDrawFlags_None,stroke);
        d->AddBezierCubic(q(0,0),q(0,-4),q(0,-9),q(3,-14),ink,fine);
        path({a(-8,1),a(-6,3),a(6,3),a(8,1)});
        path({a(-2,3),a(-2,14),a(0,17),a(2,14),a(2,3)});
        const auto l=a(-2,12),r=a(2,12); etch(l.x,l.y,r.x,r.y); break;
    }
    case IconKind::Axe:
        shape({{-5,-17},{-5,17},{-2,18},{1,17},{1,-17}});
        start(1,-12); bend(6,-12,10,-14,13,-18); bend(18,-10,18,0,12,6);
        bend(10,0,6,-3,1,-3); end(); etch(12,-12,13,-3); etch(-5,11,1,11); break;
    case IconKind::Mace:
        // Broad flanged steel head, seated directly on a collared grip.
        shape({{-3,-18},{3,-18},{5,-15},{10,-13},{10,-5},{5,-3},{3,0},{-3,0},{-5,-3},{-10,-5},{-10,-13},{-5,-15}});
        path({{-3,-18},{-4,-10},{-3,0}}); path({{3,-18},{4,-10},{3,0}});
        path({{-2,0},{-2,4},{-3,15},{-3,18},{3,18},{3,15},{2,4},{2,0}});
        line(-2,4,2,4); etch(-3,14,3,14); break;
    case IconKind::Bow:
        start(-7,-18); bend(-6,-12,10,-13,10,0); bend(10,13,-6,12,-7,18); end();
        path({{-7,-18},{-3,0},{-7,18}}); line(-16,0,17,0);
        path({{11,-5},{17,0},{11,5}}); etch(-15,-4,-11,0); etch(-15,4,-11,0); break;
    case IconKind::Crossbow:
        start(-17,-4); bend(-12,-12,-6,-13,0,-10); bend(6,-13,12,-12,17,-4); end();
        path({{-17,-4},{0,1},{17,-4}});
        path({{-3,0},{-3,17},{3,17},{3,0}}); line(0,-19,0,-2);
        path({{-4,-15},{0,-19},{4,-15}}); etch(-3,12,3,12); break;
    case IconKind::Staff:
        gem(0,-12,6);
        start(-9,-13); to(-8,-6); bend(-7,-1,-2,-3,-2,3); to(-2,18); to(2,18); to(2,3);
        bend(2,-3,7,-1,8,-6); to(9,-13); end(); etch(-2,12,2,12); break;
    case IconKind::Arrow: {
        auto a = [](float x,float y) { return ImVec2{(x-y)*.70710678f,(x+y)*.70710678f}; };
        shape({a(0,-21),a(5,-10),a(0,-12),a(-5,-10)});
        const auto head=a(0,-12),tail=a(0,20); line(head.x,head.y,tail.x,tail.y);
        // A matched pair of feather vanes, joined precisely to the shaft.
        shape({a(0,5),a(-5,9),a(-5,16),a(0,12)});
        shape({a(0,5),a(5,9),a(5,16),a(0,12)}); break;
    }
    case IconKind::Armor:
        // Sculpted breastplate, open arms and a tapered waist.
        start(-6,-17); bend(-4,-12,4,-12,6,-17); to(14,-13); to(11,-3); to(10,9);
        to(6,15); to(0,18); to(-6,15); to(-10,9); to(-11,-3); to(-14,-13); end(true);
        path({{-8,-5},{0,-1},{8,-5}}); etch(0,-1,0,10); path({{-8,9},{0,12},{8,9}}); break;
    case IconKind::Robe:
        shape({{-5,-17},{-12,-12},{-17,-1},{-11,3},{-7,-5},{-5,1},{-12,17},{12,17},{5,1},{7,-5},{11,3},{17,-1},{12,-12},{5,-17},{0,-10}});
        path({{-5,1},{0,3},{5,1}}); etch(0,-9,0,-1); etch(-2,7,-4,13); etch(2,7,4,13); break;
    case IconKind::Helmet:
        start(0,-18); bend(-10,-17,-14,-11,-14,-2); to(-12,12); to(-5,16); to(-5,3);
        to(-10,-1); to(-3,0); to(0,7); to(3,0); to(10,-1); to(5,3); to(5,16); to(12,12); to(14,-2);
        bend(14,-11,10,-17,0,-18); end(); etch(0,-13,0,-4); break;
    case IconKind::Gloves:
        start(-8,17); to(-10,6); to(-16,-1); bend(-17,-5,-13,-7,-11,-4); to(-7,0);
        to(-7,-12); bend(-7,-16,-3,-16,-2,-13); to(2,-15); to(6,-13); to(10,-9); to(10,3); to(7,17); end(true);
        path({{-8,10},{0,12},{8,10}}); etch(-2,-10,-2,-3); etch(4,-9,4,-3); break;
    case IconKind::Boots:
        start(-8,-17); to(7,-17); to(5,3); bend(8,7,16,6,16,13); to(14,16); to(-10,16); to(-10,9); end(true);
        line(-8,-11,6,-11); path({{-9,11},{-4,12},{15,12}}); etch(-3,-7,2,-4); etch(-4,-1,1,2); break;
    case IconKind::Ring:
        start(-6,-4); bend(-17,1,-14,17,0,17); bend(14,17,17,1,6,-4); end();
        shape({{-7,-10},{-3,-16},{3,-16},{7,-10},{0,-3}});
        etch(-6,-10,6,-10); etch(-2,-15,0,-5);
        start(-8,3); bend(-10,8,-6,12,-3,12); end(); break;
    case IconKind::Amulet:
        start(-13,-17); bend(-13,-5,-7,0,0,4); bend(7,0,13,-5,13,-17); end();
        gem(0,10,8); etch(0,6,0,14); etch(-8,-10,-5,-5); etch(8,-10,5,-5); break;
    case IconKind::Shield:
        start(0,-17); bend(5,-14,10,-13,14,-13); to(12,3); bend(11,10,5,15,0,18);
        bend(-5,15,-11,10,-12,3); to(-14,-13); bend(-10,-13,-5,-14,0,-17); end();
        // An inset rim and round boss describe shield construction, not a rune.
        start(0,-12); to(9,-9); to(8,2); bend(7,7,3,11,0,13);
        bend(-3,11,-7,7,-8,2); to(-9,-9); end(true);
        circle(0,-1,3); break;
    case IconKind::Potion:
        shape({{-5,-18},{5,-18},{5,-14},{-5,-14}});
        start(-4,-14); to(-4,-7); bend(-4,-4,-13,-1,-13,7); bend(-13,14,-8,17,0,17);
        bend(8,17,13,14,13,7); bend(13,-1,4,-4,4,-7); to(4,-14); end();
        gem(0,6,5); break;
    case IconKind::Food:
        start(0,-8); bend(-11,-17,-19,-6,-13,7); bend(-9,18,-5,18,0,14);
        bend(5,18,9,18,13,7); bend(19,-6,11,-17,0,-8); end();
        start(0,-8); bend(-1,-12,0,-16,3,-18); end();
        start(3,-14); bend(6,-20,11,-19,14,-17); bend(10,-12,7,-11,3,-14); end();
        start(-8,-5); bend(-11,-2,-10,2,-9,4); end(); break;
    case IconKind::Magic:
        // Four elongated points echo the wheel's diamond page markers.
        shape({{0,-19},{5,-5},{15,0},{5,5},{0,19},{-5,5},{-15,0},{-5,-5}});
        gem(0,0,4); etch(-13,-13,-9,-9); etch(9,9,13,13); etch(9,-9,13,-13); etch(-13,13,-9,9); break;
    case IconKind::Scroll:
        start(-10,-11); to(-15,-11); to(-15,-14); bend(-15,-18,-7,-18,-7,-14); to(-7,12);
        bend(-7,18,12,18,12,12); to(12,9); to(-2,9); end();
        path({{-11,-17},{9,-17},{12,-14},{12,5}}); etch(-2,-9,7,-9); etch(-2,-4,7,-4); etch(-2,1,3,1); break;
    case IconKind::Torch:
        start(0,-19); bend(-1,-11,-10,-10,-8,-3); bend(-7,3,6,3,8,-3);
        bend(10,-8,6,-12,5,-13); bend(5,-8,2,-7,1,-7); bend(3,-12,1,-16,0,-19); end();
        path({{-7,4},{-4,7},{-2,18},{2,18},{4,7},{7,4}}); line(-4,7,4,7); break;
    case IconKind::LightPlayer: case IconKind::LightTarget: case IconKind::LightGroup:
        circle(0,-6,5);
        start(-10,16); to(-9,9); bend(-8,1,8,1,9,9); to(10,16); end();
        if (kind == IconKind::LightGroup) {
            start(-11,-12); bend(-18,-15,-21,-3,-13,-2); end();
            start(11,-12); bend(18,-15,21,-3,13,-2); end();
            path({{-14,4},{-17,7},{-18,13}}); path({{14,4},{17,7},{18,13}});
        } else if (kind == IconKind::LightTarget) {
            path({{-16,-5},{-16,-16},{-8,-16}}); path({{8,-16},{16,-16},{16,-5}});
            path({{-16,8},{-16,17},{-12,17}}); path({{12,17},{16,17},{16,8}});
        } else {
            line(0,-19,0,-16); line(-13,-14,-10,-11); line(10,-11,13,-14);
            line(-17,-3,-14,-3); line(14,-3,17,-3);
        }
        break;
    case IconKind::Back:
        path({{-3,-11},{-15,0},{-3,11}}); line(-15,0,15,0); break;
    default:
        // One closed silhouette: gathered mouth, neck and bag share endpoints.
        start(-6,-9); to(-10,-17); to(-3,-15); to(0,-17); to(3,-15); to(10,-17); to(6,-9);
        bend(9,-3,14,1,14,8); bend(14,19,-14,19,-14,8); bend(-14,1,-9,-3,-6,-9); end(true);
        line(-6,-9,6,-9); break;
    }
}
}
