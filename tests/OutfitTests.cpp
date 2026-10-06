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
    std::cout<<"Outfit serialization, malformed input, instance matching, preflight and toggle tests passed\n";
}
