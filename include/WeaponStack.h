#pragma once
#include "EquipPolicy.h"
#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>

namespace Wheel {
    // One physical extra list, or the unextended remainder (extra == 0).
    // count is partitioned before filtering; it is never the row's merged count.
    struct WeaponCopy {
        std::uintptr_t extra=0;
        int count=0;
        bool plain=false,favorite=false,right=false,left=false;
        int hotkey=-1;
    };
    struct PlainWeaponStack {
        int anchor=-1,count=0,hotkey=-1;
        bool favorite=false,right=false,left=false;
        bool Contains(const WeaponCopy& copy)const {
            // A differently bound favorite remains a separate entry. Neither
            // protected nor modified instances can enter this plain pool.
            return anchor>=0 && copy.count>0 && copy.plain &&
                (!copy.favorite || copy.hotkey==hotkey || (favorite && copy.hotkey==255));
        }
    };
    inline PlainWeaponStack BuildPlainWeaponStack(std::span<const WeaponCopy> copies,std::optional<int> binding={}) {
        PlainWeaponStack stack;
        for(int i=0;i<static_cast<int>(copies.size());++i) {
            const auto& copy=copies[i];
            if(binding && copy.favorite && copy.hotkey!=*binding)continue;
            if(copy.count>0 && copy.plain && (stack.anchor<0 || (!stack.favorite && copy.favorite) ||
                (copy.favorite && stack.hotkey==255 && copy.hotkey!=255))) {
                stack.anchor=i;stack.favorite=copy.favorite;stack.hotkey=copy.hotkey;
            }
        }
        std::int64_t count=0;
        for(const auto& copy:copies)if(stack.Contains(copy)) {
            count+=copy.count;stack.right|=copy.right;stack.left|=copy.left;
        }
        stack.count=static_cast<int>(std::min<std::int64_t>(count,std::numeric_limits<int>::max()));
        return stack;
    }
    struct StackWeaponPlan {
        int index=-1;
        WeaponHandAction action=WeaponHandAction::Equip;
        bool nativeSpare=false; // An unextended copy within a worn plain stack.
    };
    inline StackWeaponPlan PlanStackWeapon(std::span<const WeaponCopy> copies,const PlainWeaponStack& stack,bool left,bool twoHanded) {
        // Toggling the requested hand must target its actual physical list,
        // even if the row's favorite marker lives on the other copy.
        for(int i=0;i<static_cast<int>(copies.size());++i) {
            const auto& copy=copies[i];
            if(stack.Contains(copy) && (twoHanded?(copy.left||copy.right):(left?copy.left:copy.right)))
                return {i,WeaponHandAction::Unequip};
        }
        // Choose a genuinely unworn copy. Never submit the worn anchor merely
        // because the display includes a second copy in its merged quantity.
        for(int i=0;i<static_cast<int>(copies.size());++i) {
            const auto& copy=copies[i];
            if(stack.Contains(copy) && !copy.left && !copy.right)return {i,WeaponHandAction::Equip};
        }
        for(int i=0;i<static_cast<int>(copies.size());++i) {
            const auto& copy=copies[i];
            if(stack.Contains(copy) && copy.count>int(copy.left)+int(copy.right))
                return {i,WeaponHandAction::Equip,true};
        }
        for(int i=0;i<static_cast<int>(copies.size());++i)if(stack.Contains(copies[i]))
            return {i,twoHanded?WeaponHandAction::Equip:WeaponHandAction::Move};
        return {};
    }
}
