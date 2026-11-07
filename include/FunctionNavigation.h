#pragma once
namespace Wheel::FaceLight {
    enum class Section { Outfits, Lighting, Followers, Combat, Cycles, CycleMembers, OutfitPieces };
    constexpr int TypeCount(bool faceLightAvailable) { return faceLightAvailable?4:3; }
    constexpr int TypeIndex(Section section) {return section==Section::Outfits||section==Section::OutfitPieces?0:section==Section::Combat?1:section==Section::Cycles || section==Section::CycleMembers?2:3;}
    constexpr Section TypeAt(int index) {return index==0?Section::Outfits:index==1?Section::Combat:index==2?Section::Cycles:Section::Lighting;}
    constexpr const char* TypeKey(int index){return index==0?"outfitTitle":index==1?"combatTitle":index==2?"cycleTitle":"lightTitle";}
    constexpr Section VisibleSection(Section section,bool faceLightAvailable) {
        return faceLightAvailable || section==Section::Outfits || section==Section::Combat || section==Section::Cycles || section==Section::CycleMembers || section==Section::OutfitPieces?section:Section::Outfits;
    }
    // A/D cycles outfits, combat, cycles, then optional lighting; lists are children.
    constexpr Section ChangeType(Section current, int direction,bool faceLightAvailable=true) {
        if(current==Section::OutfitPieces)return direction?Section::Outfits:current;
        current=VisibleSection(current,faceLightAvailable);
        if(!direction)return current;
        const int count=TypeCount(faceLightAvailable),index=TypeIndex(current);
        return TypeAt((index+direction%count+count)%count);
    }
}
