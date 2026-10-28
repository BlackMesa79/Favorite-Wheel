#pragma once
namespace Wheel {
    constexpr bool MouseUsesLeftHand(unsigned button,bool swapped,bool functions) {
        return (button==1) != (swapped && !functions);
    }
    enum class HandEquip { None, Left, Right, Dual, TwoHanded };
    // Physical objects use the current instance's ExtraWorn/ExtraWornLeft;
    // spells use the player's left/right slots. Shields and torches are
    // left-only even when the engine marks them with ordinary ExtraWorn.
    constexpr HandEquip EquippedHand(bool wornRight,bool wornLeft,bool twoHanded=false,bool leftOnly=false) {
        if(!wornRight && !wornLeft)return HandEquip::None;
        if(twoHanded)return HandEquip::TwoHanded;
        if(leftOnly)return HandEquip::Left;
        return wornRight?(wornLeft?HandEquip::Dual:HandEquip::Right):HandEquip::Left;
    }
    constexpr const char* HandEquipLabel(HandEquip hand) {
        switch(hand) {
        case HandEquip::Left:return "L";
        case HandEquip::Right:return "R";
        case HandEquip::Dual:return "L+R";
        case HandEquip::TwoHanded:return "2H";
        default:return "";
        }
    }
    constexpr const char* HandEquipKey(HandEquip hand) {
        switch(hand) {
        case HandEquip::Left:return "equippedLeft";
        case HandEquip::Right:return "equippedRight";
        case HandEquip::Dual:return "equippedDual";
        case HandEquip::TwoHanded:return "equippedTwoHanded";
        default:return "equipped";
        }
    }
    constexpr bool UnequipWeapon(bool twoHanded,bool left,bool wornRight,bool wornLeft) {
        return twoHanded ? wornRight||wornLeft : left?wornLeft:wornRight;
    }
    enum class WeaponHandAction { Equip, Unequip, Move };
    constexpr WeaponHandAction WeaponHandPlan(bool twoHanded,bool left,bool wornRight,bool wornLeft,int count) {
        if(UnequipWeapon(twoHanded,left,wornRight,wornLeft))return WeaponHandAction::Unequip;
        // A single physical copy cannot occupy both hands. Native EquipObject
        // can reject the already-worn instance instead of moving it for us.
        if(!twoHanded && (left?wornRight:wornLeft) && count==1)return WeaponHandAction::Move;
        return WeaponHandAction::Equip;
    }
    template<class Operations>
    bool ExecuteWeaponHand(WeaponHandAction action,bool left,Operations& operations) {
        if(action==WeaponHandAction::Unequip) {
            operations.Unequip(left);
            return true;
        }
        if(action==WeaponHandAction::Move) {
            // The native bool reports extra-list destruction, not success.
            // Observe the fresh inventory instead of branching on that bool.
            operations.Unequip(!left);
            if(!operations.Refresh())return false;
        }
        return operations.Equip(left);
    }
    struct SpellHandAction {
        bool clearLeft=false,clearRight=false;
        constexpr bool Unequip() const {return clearLeft || clearRight;}
    };
    constexpr SpellHandAction SpellHandPlan(bool regularSpell,bool twoHanded,bool left,bool inLeft,bool inRight) {
        if(!regularSpell)return {};
        if(twoHanded)return {inLeft,inRight};
        return {left && inLeft,!left && inRight};
    }
    template<class Operations>
    bool ExecuteSpellHand(SpellHandAction action,bool left,Operations& operations) {
        if(!action.Unequip())return operations.Equip(left);
        // Only clear slots containing this spell. Never replace another hand's
        // spell or fall back to equipping after a rejected native dispatch.
        if(action.clearLeft && !operations.Clear(true))return false;
        if(action.clearRight && !operations.Clear(false))return false;
        return true;
    }
}
