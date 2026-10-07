#include "OutfitModel.h"
#include "OutfitStep.h"
#include <cstdlib>
#include <iostream>
using namespace Wheel::Outfits;
void Check(bool result,const char* label){if(!result){std::cerr<<label<<'\n';std::exit(1);}}
struct Candidate{Piece piece;bool worn=false;};
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
    Check(Resolve(outfit,inventory,matches)&&!Wearing(inventory,matches),"Extra armor triggers full replacement, not false equipped badge");
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
    std::cout<<"Outfit serialization, malformed input, instance matching, preflight and toggle tests passed\n";
}
