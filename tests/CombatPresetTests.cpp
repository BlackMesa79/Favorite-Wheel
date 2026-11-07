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
    Piece shout{{"Skyrim.esm",0x6789},{},{},0,1.f,"","Unrelenting Force"};
    Preset voice{6,"Shout"};voice.voice={Kind::Voice,shout};
    Preset preserve{7,"Preserve hands"};preserve.right.kind=Kind::Ignore;preserve.left.kind=Kind::Ignore;preserve.voice=voice.voice;
    const std::vector<Preset> all{pair,empty,magic,both,voice,preserve};std::vector<Preset> decoded;
    Check(Decode(Encode(all),decoded) && decoded==all,"Character records round trip empty, mixed, dual-spell and two-handed targets with Unicode names");
    auto invalid=both;invalid.left={Kind::Spell,spell};
    Check(!Valid(invalid),"Two-handed presets cannot contain an independent left-hand target");
    invalid=both;invalid.left.kind=Kind::Ignore;
    Check(Valid(invalid),"A two-handed target may mark the displaced left slot as ignored");
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
    Check(StepComplete(preserve,Step::ClearRight,{Kind::Object,sword}) &&
        StepComplete(preserve,Step::EquipLeft,{Kind::Spell,spell}),"Ignored hands accept any observed state without changing the target");
    Check(StepComplete(voice,Step::EquipVoice,voice.voice) && StepComplete(voice,Step::ClearVoice,voice.voice) &&
        StepComplete(empty,Step::ClearVoice,{}),"Voice targets equip, clear and verify independently from hand slots");
    Check(StepComplete(voice,Step::ClearVoice,{Kind::Voice,spell}),"Replacing Voice never requires observing an intermediate empty slot");
    Check(!StepComplete(empty,Step::ClearVoice,voice.voice) && !StepComplete(empty,Step::ClearVoice,{},true),
        "Explicit Voice unequip still requires both observed empty and callback completion");
    for(auto p:{empty,pair,magic,both}) {
        p.voice=voice.voice;
        Check(HandednessMatches(p,Slot::Voice,false) && HandednessMatches(p,Slot::Voice,true),
            "Voice preflight is independent of any hand weapon/spell handedness");
        Check(HandednessMatches(p,Slot::Right,TwoHanded(p)) && !HandednessMatches(p,Slot::Right,!TwoHanded(p)),
            "Hand targets retain their handedness validation");
        Check(bool(Resolve(p,std::vector<Candidate>{{sword,2},{distinct}})),"Voice does not consume inventory capacity");
    }
    auto modes=pair;modes.voice=voice.voice;
    for(auto slot:{Slot::Right,Slot::Left,Slot::Voice}) {
        const auto original=modes;
        const auto saved=slot==Slot::Right?modes.right:slot==Slot::Left?modes.left:modes.voice;
        Check(CycleSlotMode(modes,slot),"Set to Ignore");
        const auto ignored=slot==Slot::Right?modes.right:slot==Slot::Left?modes.left:modes.voice;
        Check(ignored.kind==Kind::Ignore && ignored.piece==saved.piece && ignored.remembered==saved.kind,"Ignore retains exact item identity");
        std::vector<Preset> restored;
        Check(Decode(Encode({modes}),restored,3) && restored.front()==modes,"Inactive item survives save/load");
        modes=restored.front();
        Check(CycleSlotMode(modes,slot),"Ignore to Empty");
        Check(Decode(Encode({modes}),restored,3) && restored.front()==modes,"Empty retains saved item too");
        Check(CycleSlotMode(modes,slot) && modes==original,"Empty to Set restores original, without current gear");
    }
    auto bowModes=both;
    Check(CycleSlotMode(bowModes,Slot::Right) && bowModes.twoHanded && !TwoHanded(bowModes),"Ignored bow remembers but does not activate two-handed semantics");
    std::vector<Preset> savedBow;
    Check(Decode(Encode({bowModes}),savedBow,3) && savedBow.front()==bowModes,"Ignored two-handed flag round trips with saved right item");
    Check(CycleSlotMode(bowModes,Slot::Right) && bowModes.twoHanded && !TwoHanded(bowModes),"Empty right preserves bow handedness");
    Check(CycleSlotMode(bowModes,Slot::Right) && bowModes==both && TwoHanded(bowModes),"Restored bow restores two-handed behavior");
    Check(CycleSlotMode(bowModes,Slot::Right),"Ignore bow for conflict test");
    bowModes.left=magic.left;
    Check(CycleSlotMode(bowModes,Slot::Right),"Ignored bow to empty with active left");
    const auto conflict=bowModes;
    Check(!CycleSlotMode(bowModes,Slot::Right) && bowModes==conflict,"Conflicting restore is atomic and preserves saved items");
    auto noItem=empty;
    Check(CycleSlotMode(noItem,Slot::Voice) && noItem.voice.kind==Kind::Ignore &&
        CycleSlotMode(noItem,Slot::Voice) && noItem==empty,"Never-captured slots cycle Empty/Ignore and skip Set");
    auto malformed=voice;malformed.voice.remembered=Kind::Object;
    Check(!Decode(Encode({malformed}),decoded),"Active targets cannot hide a second remembered kind");
    malformed=empty;malformed.voice={Kind::Ignore,sword,Kind::Object};
    Check(!Valid(malformed),"Inactive Voice cannot hide a weapon");
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
    Check(Decode(Encode(bindings),decoded,3) && decoded==all && DecodeBindings(EncodeBindings(bindings),savedBindings),
        "Equipment uses FWC3 for slot states; shortcuts have a separate record");
    ApplyBindings(decoded,savedBindings);
    Check(decoded==bindings,"Independent shortcut and equipment records restore the complete preset");
    auto beforeEquipment=std::vector<Preset>{};ApplyBindings(beforeEquipment,savedBindings);
    Check(Decode(Encode(all),beforeEquipment,3),"Bindings can load before equipment");ApplyBindings(beforeEquipment,savedBindings);
    Check(beforeEquipment==bindings,"Record load order does not change restored shortcuts");
    const auto unchanged=bindings;
    Check(!AssignQuickSlot(bindings,999,1) && !AssignQuickSlot(bindings,pair.id,8) && bindings==unchanged,
        "Invalid IDs and non-native slots do not displace any binding");
    Check(AssignQuickSlot(bindings,magic.id,7,&assigned) && assigned==-1 && !PresetForQuickSlot(bindings,7),
        "Pressing the same slot again unbinds without affecting another slot");
    // A genuine legacy FWC1 fixture must load without interpreting item tokens as bindings.
    const std::string legacy="FWC 1 1\n3 \"Unarmed\" 0\n0 \"\" 0 \"\" 0 \"\" 0 0 1 \"\" \"\"\n0 \"\" 0 \"\" 0 \"\" 0 0 1 \"\" \"\"\n";
    Check(Decode(legacy,decoded,1) && decoded==std::vector<Preset>{Preset{3,"Unarmed"}} && decoded[0].quickSlot==-1,
        "Legacy two-hand-slot records migrate with an empty Voice slot");
    const std::string legacySpell="FWC 1 1\n4 \"Dual flames\" 0\n2 \"Skyrim.esm\" 22136 \"\" 0 \"\" 0 0 1 \"\" \"Flames\"\n2 \"Skyrim.esm\" 22136 \"\" 0 \"\" 0 0 1 \"\" \"Flames\"\n";
    std::vector<Preset> oldSpells;
    Check(Decode(legacySpell,oldSpells,1) && oldSpells.front()==magic,"Populated FWC1 spell references migrate unchanged");
    const std::string legacy2="FWC 2 1\n9 \"Legacy bow and shout\" 1\n1 \"Skyrim.esm\" 4660 \"\" 0 \"\" 0 0 1 \"\" \"Bow\"\n4 \"\" 0 \"\" 0 \"\" 0 0 1 \"\" \"\"\n3 \"Skyrim.esm\" 26505 \"\" 0 \"\" 0 0 1 \"\" \"Shout\"\n";
    std::vector<Preset> migrated;
    Check(Decode(legacy2,migrated,2) && migrated.front().twoHanded && migrated.front().voice.kind==Kind::Voice &&
        migrated.front().left.kind==Kind::Ignore,"Legacy FWC2 retains bow, Voice and Ignore");
    const auto migratedOriginal=migrated;
    Check(CycleSlotMode(migrated.front(),Slot::Right) && CycleSlotMode(migrated.front(),Slot::Right) &&
        CycleSlotMode(migrated.front(),Slot::Right) && migrated==migratedOriginal,"Migrated active items seed retained mode cycling");
    Check(!Decode(legacy2,migrated,3) && !Decode("FWC 4 0\n",migrated),"Mismatched and unknown equipment versions are rejected");
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
