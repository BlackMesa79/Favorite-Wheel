#include "OutfitModel.h"
#include "OutfitStep.h"
#include "OutfitPlan.h"
#include <cstdlib>
#include <iostream>
using namespace Wheel::Outfits;
void Check(bool result,const char* label){if(!result){std::cerr<<label<<'\n';std::exit(1);}}
struct Candidate{Piece piece;bool worn=false;std::uint32_t mask=0;};
int main(){
    Check(!ValidName("   ") && !ValidName("a\nb") && ValidName("中文"),"Validate display names");
    using D=StepDecision;
    Check(DecideStep(true,false,false,0)==D::Issue,"Submit unequip once");
    Check(DecideStep(true,false,true,.016)==D::Wait && DecideStep(true,false,true,.5)==D::Wait,"Delayed unequip waits without repeat");
    Check(DecideStep(false,false,true,.1)==D::Advance,"Only advance after observed unequip");
    Check(DecideStep(false,true,false,0)==D::Issue,"Submit equip once");
    Check(DecideStep(false,true,true,.8)==D::Timeout,"Rejected equip times out without repeated calls");
    Check(DecideStep(true,true,true,.8)==D::Advance,"Observed completion wins at timeout boundary");
    Piece armor{{"Skyrim.esm",0x1234},{"",0xFF000ABC},{"Skyrim.esm",0x14},42,1.25f,"旅者 \"甲\"","Armor"};
    {
        auto piece=[&](unsigned id){auto value=armor;value.base.id=id;value.unique=static_cast<std::uint16_t>(id);return value;};
        const auto oldBody=piece(1),oldRing=piece(2),commonBoots=piece(3),newRing=piece(4),newBody=piece(5);
        std::vector<Candidate> current{{oldBody,true,4},{oldRing,true,64},{commonBoots,true,128},
            {newRing,false,64},{newBody,false,4}};
        const std::vector<int> target{3,2,4}; // Preset save order need not put Body first.
        const auto plan=ReplacementPlan(current,target,false);
        Check(plan.size()==4 && plan[0].piece==newBody && plan[0].equip && plan[1].piece==newRing && plan[1].equip,
            "Replacement covers the body before accessories, then cleans old apparel");
        Check(!plan[2].equip && !plan[3].equip && plan[2].piece==oldBody && plan[3].piece==oldRing,
            "Only previously worn pieces outside the target are cleaned up after equipping");
        Check(std::none_of(plan.begin(),plan.end(),[&](const auto& op){return op.piece==commonBoots;}),
            "Common worn instances are never unequipped or reequipped");
        auto simulated=current;
        for(const auto& op:plan) {
            const int at=Match(op.piece,simulated);
            Check(at>=0,"Replacement simulation re-resolves each saved instance");
            if(op.equip)for(auto& item:simulated)if(item.mask&simulated[at].mask)item.worn=false;
            simulated[at].worn=op.equip;
            Check(simulated[2].worn && (simulated[0].worn || simulated[4].worn),
                "Native slot replacement never requires a plugin-driven bare-body stage or removal of shared boots");
        }
        Check(Wearing(simulated,target),"Replacement finishes with the exact target, without old accessories");
        current[0].worn=false;current[1].worn=false;current[3].worn=true;current[4].worn=true;
        Check(ReplacementPlan(current,target,false).empty(),"A completed target has no replacement operations");
        const auto off=ReplacementPlan(current,target,true);
        Check(off.size()==3 && std::none_of(off.begin(),off.end(),[](const auto& op){return op.equip;}),
            "Clicking an exact preset again removes all managed apparel only");
        current[1].worn=true;
        const auto extra=ReplacementPlan(current,target,false);
        Check(extra.size()==1 && !extra[0].equip && extra[0].piece==oldRing,
            "A fully worn target plus extra apparel only removes the extra item");
        Check(OutfitBudgetAvailable(0,0) && OutfitBudgetAvailable(1,2.9) &&
            !OutfitBudgetAvailable(2,0) && !OutfitBudgetAvailable(1,3.),
            "Per-tick budget bounds both engine submissions and elapsed work");
    }
    Preset outfit{1,"夜行套装",{armor}};
    std::vector<Preset> decoded;
    Check(Decode(Encode({outfit}),decoded) && decoded==std::vector<Preset>{outfit},"Unicode and instance fields survive serialization");
    const auto original=decoded;
    Check(!Decode("FWO 9 1",decoded) && decoded==original,"Bad schema is transactional");
    Check(!Decode(Encode({outfit})+"trailing",decoded),"Reject trailing data");
    Check(!Decode("FWO 1 1000000",decoded),"Reject excessive count");
    Check(!Decode(Encode({outfit,outfit}),decoded),"Reject duplicate ids");
    auto bad=outfit;bad.pieces[0].health=std::numeric_limits<float>::infinity();
    Check(!Decode(Encode({bad}),decoded),"Reject invalid health");
    Check(!Portable(outfit),"Dynamic enchantment is character-local");
    auto portable=outfit;portable.pieces[0].enchantment={"Skyrim.esm",0xAB};
    Check(Portable(portable),"Static references are transferable");
    std::vector<Candidate> inventory{{armor,true}};
    std::vector<int> matches;
    Check(Resolve(outfit,inventory,matches) && Wearing(inventory,matches),"Exact equipped preset toggles off");
    auto alternate=armor;alternate.unique=99;alternate.health=2.f;
    inventory.push_back({alternate,false});
    Check(Match(armor,inventory)==0,"Do not equip another tempered instance");
    inventory[0].worn=false;
    Check(Resolve(outfit,inventory,matches)&&!Wearing(inventory,matches),"Present but unworn preset equips");
    inventory.erase(inventory.begin());
    Check(!Resolve(outfit,inventory,matches),"Missing target stops preflight");
    inventory={{armor,true},{armor,false}};
    Check(Match(armor,inventory)==-2,"Ambiguous unique instance rejected");
    Piece plain{{"Skyrim.esm",0x1234},{},{},0,1.f,"","Plain"};
    inventory={{plain,false},{plain,true}};
    Check(Match(plain,inventory)==1,"Equivalent plain copies prefer worn item");
    inventory={{armor,true},{alternate,true}};
    Check(Resolve(outfit,inventory,matches)&&!Wearing(inventory,matches),"Extra armor still triggers full replacement");
    Check(WearingPieces(inventory,matches),"An extra worn item must not hide the preset equipped badge");
    auto duplicate=outfit;duplicate.pieces.push_back(armor);
    Check(!Resolve(duplicate,inventory,matches),"One instance cannot satisfy two pieces");
    {
        Preset accessories{7,"Many accessories",{}};
        std::vector<Candidate> many;
        // A multipart set is limited by worn pieces, not the total forms shipped
        // by its mod. Exercise the full supported 64-piece save/load boundary.
        for(unsigned i=0;i<64;++i) {
            Piece piece{{"AccessoryPack.esp",0x800+i},{},{},0,1.f,"","Accessory"};
            accessories.pieces.push_back(piece);many.push_back({piece,true});
        }
        Check(Decode(Encode({accessories}),decoded) && Resolve(decoded[0],many,matches) && Wearing(many,matches),
            "A full 64-piece outfit saves, resolves and recognizes worn state");
        auto over=accessories;over.pieces.push_back(plain);
        Check(!Decode(Encode({over}),decoded),"65 pieces are rejected at save validation, not marked missing later");
        auto modified=accessories.pieces[17];modified.enchantment={"Skyrim.esm",0xA123};
        modified.health=1.25f;modified.customName="Named accessory";
        accessories.pieces[17]=modified;many[17].piece=modified;
        many.push_back({modified,false});
        Check(Resolve(accessories,many,matches) && matches[17]==17 && Wearing(many,matches),
            "An equivalent named/enchanted/tempered spare with no unique ID cannot make a newly saved multipart outfit missing");
        many[17].worn=false;
        Check(Resolve(accessories,many,matches),"Equivalent complete signatures remain usable after unequipping");
        ResolveProblem problem;
        many[17].piece.health=2.f;many.back().piece.customName="Different accessory";
        Check(!Resolve(accessories,many,matches,&problem) && problem.reason==ResolveReason::Changed && problem.piece==17,
            "Different temper or name never substitutes for the saved accessory and identifies its index");
        many.back().piece=modified;many.back().piece.unique=9;
        Check(!Resolve(accessories,many,matches,&problem) && problem.reason==ResolveReason::Changed,
            "A newly unique instance does not silently substitute for a no-UID recipe");
        many.erase(many.begin()+17);many.pop_back();
        Check(!Resolve(accessories,many,matches,&problem) && problem.reason==ResolveReason::Missing && problem.piece==17,
            "Absent base form is distinguished from a changed signature");
        Preset exact{8,"Exact",{armor}};
        std::vector<Candidate> collision{{armor,true},{armor,false}};
        Check(!Resolve(exact,collision,matches,&problem) && problem.reason==ResolveReason::Ambiguous,
            "Duplicate unique IDs remain an error even when one instance is worn");
        collision.pop_back();exact.pieces.push_back(armor);
        Check(!Resolve(exact,collision,matches,&problem) && problem.reason==ResolveReason::Duplicate && problem.piece==1,
            "Duplicate recipe entry is distinguished from physical absence");
        exact.pieces.pop_back();
        Check(Resolve(exact,collision,matches,&problem) && problem.reason==ResolveReason::None,
            "A successful recheck clears a previous diagnostic");
    }
    {
        auto boots=armor;boots.base.id=0x2345;boots.unique=43;boots.label="Boots";
        auto necklace=plain;necklace.base.id=0x3456;necklace.label="Necklace";
        Preset manual{9,"Manually equipped",{armor,boots}};
        // No applied-preset ID or history is needed. Resolve current worn flags
        // even when inventory order differs and spare copies are present.
        std::vector<Candidate> current{{boots,true},{alternate,false},{armor,true}};
        Check(Resolve(manual,current,matches)&&WearingPieces(current,matches)&&Wearing(current,matches),
            "Manually wearing all saved instances shows equipped and toggles off an exact set");
        current.push_back({necklace,true});
        Check(Resolve(manual,current,matches)&&WearingPieces(current,matches)&&!Wearing(current,matches),
            "An extra necklace or hidden armor accessory preserves the badge without changing full-set transaction checks");
        current[0].worn=false;
        Check(Resolve(manual,current,matches)&&!WearingPieces(current,matches),
            "A single unequipped preset piece removes the badge despite extra apparel");
        current[0].worn=true;current[2].worn=false;current[1].worn=true;
        Check(Resolve(manual,current,matches)&&!WearingPieces(current,matches),
            "A different worn same-base instance cannot falsely mark the saved instance equipped");
        current[2].worn=true;current[1].worn=false;
        Check(Resolve(manual,current,matches)&&WearingPieces(current,matches),
            "Re-equipping the saved item manually restores the badge on the next snapshot");
        Check(!WearingPieces(current,{})&&!Wearing(current,{}),"An empty match list is never an equipped preset");
        Check(!WearingPieces(current,{-1})&&!WearingPieces(current,{99}),"Invalid candidate indices never show equipped");
    }
    std::cout<<"Outfit serialization, malformed input, instance matching, manual wear badges, preflight and toggle tests passed\n";
}
