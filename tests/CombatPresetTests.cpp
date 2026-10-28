#include "CombatPresetModel.h"
#include <cstdlib>
#include <iostream>
using namespace Wheel::Combat;
void Check(bool ok,const char* message){if(!ok){std::cerr<<message<<'\n';std::exit(1);}}
struct Candidate {Piece piece;int count=1;bool right=false,left=false;};
int main() {
    Piece sword{{"Skyrim.esm",0x1234},{},{},0,1.25f,"Steel \"blade\"","钢剑"};
    Preset dual{1,"双持",{Kind::Object,sword},{Kind::Object,sword}};
    auto r=Resolve(dual,std::vector<Candidate>{{sword,1}});
    Check(!r && r.problem==Problem::NotEnough && r.left,"One physical copy cannot satisfy both hands");
    r=Resolve(dual,std::vector<Candidate>{{sword,2}});
    Check(r && r.index[0]==0 && r.index[1]==0,"Two interchangeable copies in one physical stack can dual wield");
    r=Resolve(dual,std::vector<Candidate>{{sword,1,false,true},{sword,1,true,false}});
    Check(r && r.index[0]==1 && r.index[1]==0,"Match already-worn equivalent copies to their actual hands");
    auto distinct=sword;distinct.enchantment={"Skyrim.esm",0x4567};
    Preset pair{2,"Spellsword",{Kind::Object,sword},{Kind::Object,distinct}};
    r=Resolve(pair,std::vector<Candidate>{{distinct},{sword}});
    Check(r && r.index[0]==1 && r.index[1]==0,"Same base form with different enchantments remains distinct");
    auto changed=sword;changed.health=1.5f;
    r=Resolve(dual,std::vector<Candidate>{{changed,2}});
    Check(!r && r.problem==Problem::Changed,"Changed temper/enchantment signature does not silently equip another weapon");
    auto unique=sword;unique.unique=42;unique.owner={"Skyrim.esm",0x14};
    dual.right.piece=dual.left.piece=unique;
    Check(Resolve(dual,std::vector<Candidate>{{unique,2}}).problem==Problem::NotEnough,"A unique instance cannot occupy two hands");
    Check(Resolve(dual,std::vector<Candidate>{{unique},{unique}}).problem==Problem::Ambiguous,"Duplicate unique IDs fail preflight");
    Preset empty{3,"Unarmed"};
    Piece spell{{"Skyrim.esm",0x5678},{},{},0,1.f,"","Flames"};
    Preset magic{4,"Dual flames",{Kind::Spell,spell},{Kind::Spell,spell}};
    Preset both{5,"Bow",{Kind::Object,sword},{},true};
    const std::vector<Preset> all{pair,empty,magic,both};std::vector<Preset> decoded;
    Check(Decode(Encode(all),decoded) && decoded==all,"Character records round trip empty, mixed, dual-spell and two-handed targets with Unicode names");
    auto invalid=both;invalid.left={Kind::Spell,spell};
    Check(!Valid(invalid),"Two-handed presets cannot contain an independent left-hand target");
    invalid=empty;invalid.right.piece=sword;
    Check(!Valid(invalid),"Empty targets cannot hide an item reference");
    Check(!Decode(Encode({empty,empty}),decoded) && decoded==all,"Invalid duplicate IDs preserve the previous result");
    Check(!Decode(Encode(all)+"garbage",decoded) && decoded==all,"Trailing/corrupt data cannot partly overwrite presets");
    Check(Decide(true,false,0)==Decision::Advance,"Already equipped targets skip calls, including repeated Apply");
    Check(Decide(false,false,0)==Decision::Issue && Decide(false,true,.5)==Decision::Wait,
        "Issue each engine operation once, then wait for the observed hand state");
    Check(Decide(false,true,3)==Decision::Timeout && Decide(true,true,3)==Decision::Advance,
        "A rejected equip has a deadline; observed completion wins at that boundary");
    Check(!StepComplete(empty,Step::ClearLeft,magic.left),"An explicit empty left target clears the actual spell source");
    Check(!StepComplete(empty,Step::ClearLeft,{},true),"Do not advance while a native spell callback is still pending, even if the slot looks empty");
    Check(StepComplete(empty,Step::ClearRight,{}) && !StepComplete(empty,Step::ClearLeft,magic.left),
        "Clearing one source of a two-handed spell never hides a remaining spell in the other source");
    Check(StepComplete(magic,Step::ClearRight,magic.right) && StepComplete(magic,Step::EquipRight,magic.right),
        "Restoring an already equipped spell does not clear or toggle it");
    Check(StepComplete(pair,Step::ClearLeft,magic.left),"A nonempty replacement uses native equip to replace the old spell");
    Check(StepComplete(both,Step::EquipLeft,magic.left),"A two-handed target uses one equip operation, not an empty-left cleanup after equip");
    auto damaged=pair.right;damaged.piece.health+=.1f;
    Check(!SameTarget(pair.right,damaged),"Observed completion must match the exact target weapon signature");
    auto bindings=all;int assigned=-1;
    Check(AssignQuickSlot(bindings,pair.id,0,&assigned) && assigned==0 && PresetForQuickSlot(bindings,0)==pair.id,
        "A combat preset can own native slot 1");
    Check(AssignQuickSlot(bindings,magic.id,0) && PresetForQuickSlot(bindings,0)==magic.id && bindings[0].quickSlot==-1,
        "Moving a slot to another preset removes the previous combat owner");
    Check(AssignQuickSlot(bindings,magic.id,7) && !PresetForQuickSlot(bindings,0) && PresetForQuickSlot(bindings,7)==magic.id,
        "Moving a preset to slot 8 releases its old slot");
    std::vector<SlotBinding> savedBindings;
    Check(Decode(Encode(bindings),decoded,1) && decoded==all && DecodeBindings(EncodeBindings(bindings),savedBindings),
        "Equipment retains the original FWC1 format; shortcuts have a separate record");
    ApplyBindings(decoded,savedBindings);
    Check(decoded==bindings,"Independent shortcut and equipment records restore the complete preset");
    auto beforeEquipment=std::vector<Preset>{};ApplyBindings(beforeEquipment,savedBindings);
    Check(Decode(Encode(all),beforeEquipment,1),"Bindings can load before equipment");ApplyBindings(beforeEquipment,savedBindings);
    Check(beforeEquipment==bindings,"Record load order does not change restored shortcuts");
    const auto unchanged=bindings;
    Check(!AssignQuickSlot(bindings,999,1) && !AssignQuickSlot(bindings,pair.id,8) && bindings==unchanged,
        "Invalid IDs and non-native slots do not displace any binding");
    Check(AssignQuickSlot(bindings,magic.id,7,&assigned) && assigned==-1 && !PresetForQuickSlot(bindings,7),
        "Pressing the same slot again unbinds without affecting another slot");
    // A genuine legacy FWC1 fixture must load without interpreting item tokens as bindings.
    const std::string legacy="FWC 1 1\n3 \"Unarmed\" 0\n0 \"\" 0 \"\" 0 \"\" 0 0 1 \"\" \"\"\n0 \"\" 0 \"\" 0 \"\" 0 0 1 \"\" \"\"\n";
    Check(Decode(legacy,decoded,1) && decoded==std::vector<Preset>{empty} && decoded[0].quickSlot==-1,
        "Old 0.6.2 combat records load with no default hotkeys");
    auto bad=empty;bad.quickSlot=8;
    Check(!Valid(bad) && !DecodeBindings("FWQ 1 1\n3 8\n",savedBindings),"Corrupt slot values are rejected");
    auto collision=all;collision[0].quickSlot=0;collision[1].quickSlot=0;
    Check(!DecodeBindings(EncodeBindings(collision),savedBindings) && decoded==std::vector<Preset>{empty},
        "Corrupt binding records cannot overwrite equipment presets");
    Check(!Decode(legacy,decoded,2) && !DecodeBindings("FWQ 2 0\n",savedBindings),"Unknown record versions are rejected");
    Check(!DecodeBindings("FWQ 1 2\n2 0\n2 1\n",savedBindings) && !DecodeBindings("FWQ 1 1\n0 0\n",savedBindings) &&
        !DecodeBindings("FWQ 1 9\n",savedBindings) && !DecodeBindings("FWQ 1 0\ngarbage",savedBindings),
        "Duplicate IDs, invalid IDs, excessive slots and trailing data are rejected");
    ApplyBindings(decoded,{{999,0}});Check(decoded==std::vector<Preset>{empty},"Bindings for deleted presets do not create phantom entries");
    Check(CombatQuickSlotMayRun(false,false,false,false,0,false),"An idle gameplay press can apply a preset");
    Check(!CombatQuickSlotMayRun(true,false,false,false,0,false) && !CombatQuickSlotMayRun(false,true,false,false,0,false) &&
        !CombatQuickSlotMayRun(false,false,true,false,0,false) && !CombatQuickSlotMayRun(false,false,false,true,0,false) &&
        !CombatQuickSlotMayRun(false,false,false,false,1,false) && !CombatQuickSlotMayRun(false,false,false,false,0,true),
        "Menus, wheel capture, queued transitions, modifiers and wheel entrances do not activate combat slots");
    std::cout<<"Combat preset identity, capacity, serialization, slot binding and step policy passed\n";
}
