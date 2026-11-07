#include "EquipPolicy.h"
#include "WeaponStack.h"
#include <cstdlib>
#include <iostream>
#include <vector>

void Check(bool result,const char* message) {
    if(!result){std::cerr<<message<<'\n';std::exit(1);}
}
struct NativeInventory {
    bool right=true,left=false,resolve=true,unequip=true,destroyExtra=false,sourceLeft=false;
    int count=1,liveExtra=10,selectedExtra=10;
    std::vector<int> calls;
    bool Unequip(bool fromLeft) {
        calls.push_back(fromLeft?1:2);
        sourceLeft=fromLeft;
        if(!unequip)return false;
        (fromLeft?left:right)=false;
        if(destroyExtra)++liveExtra;
        return destroyExtra; // Native bool is NOT an unequip success flag.
    }
    bool Refresh() {
        calls.push_back(3);if(!resolve || (sourceLeft?left:right))return false;
        selectedExtra=liveExtra;return true;
    }
    bool Equip(bool toLeft) {
        calls.push_back(toLeft?4:5);
        Check(selectedExtra==liveExtra,"Never pass the extra-list pointer invalidated by native unequip");
        if(count==1 && (toLeft?right:left))return false; // Reproduce old native refusal.
        (toLeft?left:right)=true;return true;
    }
};
struct SpellHands {
    bool left=true,right=true,accept=true;
    std::vector<int> calls;
    bool Clear(bool hand) {
        calls.push_back(hand?1:2);
        if(!accept)return false;
        (hand?left:right)=false;return true;
    }
    bool Equip(bool hand) {
        calls.push_back(hand?3:4);
        (hand?left:right)=true;return true;
    }
};
int main() {
    using namespace Wheel;
    {
        std::vector<WeaponCopy> steel{{10,1,true,true,true,false,255},{0,1,true}};
        auto stack=BuildPlainWeaponStack(steel);
        Check(stack.count==2 && stack.right && !stack.left && stack.favorite,"Favorite worn steel sword plus unextended spare is one x2 row");
        auto plan=PlanStackWeapon(steel,stack,true,false);
        Check(plan.index==1 && plan.action==WeaponHandAction::Equip && !plan.nativeSpare && steel[plan.index].extra==0,
            "Reproduce steel x2: choose the spare, never re-submit the already-worn favorite extra");
        steel[1]={20,1,true,false,false,true};
        stack=BuildPlainWeaponStack(steel);
        Check(stack.count==2 && stack.left && stack.right,"Split worn lists aggregate L+R without losing quantity");
        plan=PlanStackWeapon(steel,stack,true,false);
        Check(plan.index==1 && plan.action==WeaponHandAction::Unequip,"Same-hand toggle selects the actual left copy, not the favorite right anchor");
        plan=PlanStackWeapon(steel,stack,false,false);
        Check(plan.index==0 && plan.action==WeaponHandAction::Unequip,"Right toggle selects the right copy");
        steel[0]={30,1,true,false,true,false};steel[1]={40,1,true,true,false,true,255};
        stack=BuildPlainWeaponStack(steel,255);
        Check(stack.anchor==1 && stack.count==2 && stack.left && stack.right,"Native favorite-anchor migration preserves the logical pool and both hand states");
        steel[0].right=false;stack=BuildPlainWeaponStack(steel,255);
        plan=PlanStackWeapon(steel,stack,false,false);
        Check(plan.index==0 && plan.action==WeaponHandAction::Equip,"Favorite marker in left hand does not turn a spare right-hand equip into a destructive move");
    }
    {
        std::vector<WeaponCopy> split{{10,1,true,true,true,false,255},{20,1,true,false}};
        auto stack=BuildPlainWeaponStack(split);auto plan=PlanStackWeapon(split,stack,true,false);
        Check(stack.count==2 && plan.index==1 && plan.action==WeaponHandAction::Equip,"An explicit unfavorited spare is counted and selected consistently too");
        split.resize(1);split[0].count=2;stack=BuildPlainWeaponStack(split);plan=PlanStackWeapon(split,stack,true,false);
        Check(plan.index==0 && plan.nativeSpare && plan.action==WeaponHandAction::Equip,"A count-two worn plain list requests native spare selection, not its worn pointer");
        split[0].count=1;stack=BuildPlainWeaponStack(split);plan=PlanStackWeapon(split,stack,true,false);
        Check(plan.action==WeaponHandAction::Move,"An actual sole copy retains the verified single-weapon move path");
    }
    {
        // Modified/protected/unknown extra-data lists are deliberately not plain.
        std::vector<WeaponCopy> variants{{10,1,true,true,true,false,1},{20,1,false,false},{30,1,false,true,false,true,255},
            {40,1,true,true,false,false,3},{0,1,true}};
        auto stack=BuildPlainWeaponStack(variants);auto plan=PlanStackWeapon(variants,stack,true,false);
        Check(stack.count==2 && plan.index==4 && !stack.left,"Modified/protected copies and differently bound favorites never inflate the plain pool or its hand state");
        auto other=BuildPlainWeaponStack(variants,3);
        Check(other.anchor==3 && !other.Contains(variants[0]) && other.Contains(variants[4]),"Re-resolution uses the requested raw binding even when native list order changes");
        int rows=stack.count;for(const auto& copy:variants)if(!stack.Contains(copy))rows+=copy.count;
        Check(rows==5,"Merged display row and remaining rows partition all physical counts exactly once");
        variants[3].hotkey=255;
        auto unbound=BuildPlainWeaponStack(variants);
        Check(unbound.count==3,"Unbound favorite copies are available spares, not independent reserved bindings");
        variants[3].hotkey=3;
        variants[0].hotkey=255;
        auto bound=BuildPlainWeaponStack(variants);
        Check(bound.hotkey==3 && bound.count==3,"Prefer the assigned marker as anchor without losing unbound favorite copies");
    }
    NativeInventory legacy;
    Check(!legacy.Equip(true) && legacy.right && !legacy.left,"Reproduce direct equip refusing the sole copy already in the other hand");
    NativeInventory moveLeft;
    Check(ExecuteWeaponHand(WeaponHandPlan(false,true,true,false,1),true,moveLeft),"Move a single weapon right to left");
    Check(!moveLeft.right && moveLeft.left && moveLeft.calls==std::vector<int>{2,3,4},"Unequip right, resolve fresh identity, then equip left in one request");
    NativeInventory moveRight;moveRight.right=false;moveRight.left=true;moveRight.destroyExtra=true;
    Check(ExecuteWeaponHand(WeaponHandPlan(false,false,false,true,1),false,moveRight),"Move a single weapon left to right");
    Check(moveRight.right && !moveRight.left && moveRight.calls==std::vector<int>{1,3,5},"Opposite direction preserves the same operation order");
    NativeInventory toggle;
    Check(ExecuteWeaponHand(WeaponHandPlan(false,false,true,false,1),false,toggle) && !toggle.right &&
        toggle.calls==std::vector<int>{2},"Using a worn weapon in the same hand still toggles only that hand off");
    NativeInventory dual;dual.count=2;
    Check(ExecuteWeaponHand(WeaponHandPlan(false,true,true,false,2),true,dual) && dual.right && dual.left &&
        dual.calls==std::vector<int>{4},"Multiple copies keep native dual wield; do not remove the first hand");
    NativeInventory blocked;blocked.unequip=false;
    Check(!ExecuteWeaponHand(WeaponHandAction::Move,true,blocked) && blocked.calls==std::vector<int>{2,3} && blocked.right,
        "Observe the still-worn source to stop rejected unequip, rather than testing its native bool");
    NativeInventory changed;changed.resolve=false;
    Check(!ExecuteWeaponHand(WeaponHandAction::Move,true,changed) && changed.calls==std::vector<int>{2,3},
        "An unresolvable instance stops before stale-pointer equip");
    Check(WeaponHandPlan(true,true,true,false,1)==WeaponHandAction::Unequip &&
        WeaponHandPlan(true,false,false,false,1)==WeaponHandAction::Equip,"Two-handed weapons never enter cross-hand transfer");
    for(bool regular:{false,true})for(bool two:{false,true})for(bool targetLeft:{false,true})
        for(bool inLeft:{false,true})for(bool inRight:{false,true}) {
            SpellHands hands;hands.left=inLeft;hands.right=inRight;
            const auto plan=SpellHandPlan(regular,two,targetLeft,inLeft,inRight);
            const bool clearLeft=regular && inLeft && (two || targetLeft);
            const bool clearRight=regular && inRight && (two || !targetLeft);
            Check(plan.clearLeft==clearLeft && plan.clearRight==clearRight,"Spell toggle targets only matching selected slots, excluding powers");
            Check(ExecuteSpellHand(plan,targetLeft,hands),"Spell plan dispatch succeeds");
            if(clearLeft || clearRight) {
                std::vector<int> expected;if(clearLeft)expected.push_back(1);if(clearRight)expected.push_back(2);
                Check(hands.calls==expected && hands.left==(inLeft && !clearLeft) && hands.right==(inRight && !clearRight),
                    "Unselect preserves the other hand and never re-equips; two-hand spell clears all reported occupied slots");
            }else Check(hands.calls==std::vector<int>{targetLeft?3:4},"Unselected spells and voice powers use the equip path");
        }
    SpellHands repeated;repeated.left=false;repeated.right=false;
    for(bool expected:{true,false,true,false}) {
        Check(ExecuteSpellHand(SpellHandPlan(true,false,false,repeated.left,repeated.right),false,repeated),"Repeated same-hand spell click accepted");
        Check(repeated.right==expected && !repeated.left,"Repeated same-hand clicks alternate equipped and empty");
    }
    SpellHands addRight;addRight.right=false;
    Check(ExecuteSpellHand(SpellHandPlan(true,false,false,true,false),false,addRight) && addRight.left && addRight.right &&
        addRight.calls==std::vector<int>{4},"Equipping opposite hand still supports dual spells");
    SpellHands rejected;rejected.accept=false;
    Check(!ExecuteSpellHand(SpellHandPlan(true,false,true,true,true),true,rejected) && rejected.left && rejected.right &&
        rejected.calls==std::vector<int>{1},"Rejected clear never falls through into equip or touches the opposite hand");
    SpellHands rejectedBoth;rejectedBoth.accept=false;
    Check(!ExecuteSpellHand(SpellHandPlan(true,true,false,true,true),false,rejectedBoth) && rejectedBoth.calls==std::vector<int>{1},
        "Two-handed dispatch stops if its first clear is rejected");
    std::cout<<"Hand equip planning and native-transfer executor tests passed\n";
}
