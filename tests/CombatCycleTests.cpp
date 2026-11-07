#include "CombatCycleModel.h"
#include "CombatPresetModel.h"
#include "InputGate.h"
#include <cstdlib>
#include <iostream>
#include <locale>

void Check(bool value,const char* message){if(!value){std::cerr<<message<<'\n';std::exit(1);}}
void Ac3_CycleRecordsIgnoreDigitGrouping();
int main() {
    using namespace Wheel;using namespace Wheel::Cycles;
    auto groups=Defaults();Check(Valid(groups),"Fresh/old characters have eight valid empty Shift+1..8 groups");
    for(int i=0;i<8;++i) {
        Check(Match(groups,i+2,1,true)==static_cast<unsigned>(i+1),"The open wheel can seed every default group");
        Check(!Match(groups,i+2,1,false),"Empty default groups never steal gameplay shortcuts");
        Check(!Match(groups,i+2,0,true) && !Match(groups,i+2,3,true),"Native digits and extra modifiers do not match a cycle chord");
    }
    auto& group=groups[0];
    Check(Append(group,0)==AppendResult::Invalid && Append(group,1000001)==AppendResult::Invalid && group.members.empty(),"Invalid preset IDs cannot enter a group");
    Check(Append(group,10)==AppendResult::Added && Append(group,20)==AppendResult::Added && Append(group,30)==AppendResult::Added,"Append presets in insertion order");
    Check(Append(group,20)==AppendResult::Duplicate && group.members.size()==3,"Duplicate quick-save never appends another entry");
    Check(Ordered(group)==std::vector<std::uint32_t>{10,20,30},"First activation starts at entry one");
    Check(!Commit(group,10,false) && group.current==0,"Failed preflight/equip never advances the position");
    Check(Commit(group,10,true) && Ordered(group)==std::vector<std::uint32_t>{20,30,10},"Successful application advances to the next entry");
    Combat::Preset voicePreset{10,"Voice cycle"};
    voicePreset.voice={Combat::Kind::Voice,{{"Skyrim.esm",0x1234},{},{},0,1.f,"","Power"}};
    const auto originalVoice=voicePreset.voice;
    Check(Combat::CycleSlotMode(voicePreset,Combat::Slot::Voice),"Cycle member can ignore its saved Voice");
    std::vector<Combat::Preset> loadedPresets;
    Check(Combat::Decode(Combat::Encode({voicePreset}),loadedPresets,3),"Cycle member retained equipment survives co-save");
    Check(Combat::CycleSlotMode(loadedPresets[0],Combat::Slot::Voice) && Combat::CycleSlotMode(loadedPresets[0],Combat::Slot::Voice) &&
        loadedPresets[0].voice==originalVoice,"Cycle member restores original Voice without recapture");
    Check(!Commit(group,20,false) && group.current==10,"Voice failure does not advance the cycle");
    const auto before=group;
    Check(!Commit(group,999,true) && group==before,"A deleted member cannot be committed by an obsolete callback");
    Check(Commit(group,30,true) && Ordered(group).front()==10,"Last entry wraps to the first");
    Check(Move(group,10,1) && group.members==std::vector<std::uint32_t>{20,10,30} && group.current==30,"Reordering preserves the actual selected preset ID");
    Check(!Move(group,20,-1) && !Move(group,30,1),"Boundary moves do not wrap or corrupt order");
    Check(Remove(group,30) && group.current==0 && Ordered(group)==std::vector<std::uint32_t>{20,10},"Removing current member resets the next choice safely");
    const auto candidates=Ordered(group);std::uint32_t usable=0;
    for(auto id:candidates)if(id==10){usable=id;break;}
    Check(usable==10 && Commit(group,usable,true),"Missing entry can be skipped while only a successful usable entry commits");
    Group full;for(unsigned i=1;i<=memberLimit;++i)Check(Append(full,i)==AppendResult::Added,"Fill member limit");
    Check(Append(full,100)==AppendResult::Full && full.members.size()==memberLimit,"Overfull group cannot create a dangling member");
    Check(ValidChord(2,1) && ValidChord(2,2) && ValidChord(2,4) && ValidChord(2,7),"Shift/Ctrl/Alt and their combinations are supported");
    Check(ValidChord(33,0) && ValidChord(33,3) && ValidChord(-1,0),"Other main keys and disabling are supported");
    for(int key:{1,15,28,42,54,29,157,56,184,60,156,219,220,221,255})Check(!ValidChord(key,1),"Menu keys and modifier-only bindings are rejected");
    for(int key=2;key<=9;++key)Check(!ValidChord(key,0),"Bare native slots stay reserved for vanilla/combat shortcuts");
    Check(!ValidChord(-1,1) && !ValidChord(33,8),"Invalid disabled/modifier encodings are rejected");
    Check(EntranceConflict(16,1,16,0,16,1,19) && EntranceConflict(19,0,16,0,16,1,19),"Wheel entrances and in-wheel switch key cannot be silently overridden");
    Check(!EntranceConflict(2,1,16,0,16,1,19),"Default cycle combination does not conflict with Q/Shift+Q/R");
    groups[1].name="Sword / shield \"A\"";
    groups[2].name="法术循环";groups[2].key=33;groups[2].modifiers=6;
    Groups restored;
    Check(Decode(Encode(groups),restored) && restored==groups,"Unicode names, custom chords, order and positions round-trip in the independent record");
    auto corrupt=groups;corrupt[1].key=groups[0].key;corrupt[1].modifiers=groups[0].modifiers;
    const auto saved=restored;
    Check(!Decode(Encode(corrupt),restored) && restored==saved,"Duplicate bindings reject the whole record without partially changing state");
    corrupt=groups;corrupt[0].members.push_back(corrupt[0].members.front());Check(!Decode(Encode(corrupt),restored),"Duplicate member rejected");
    corrupt=groups;corrupt[0].current=999;Check(!Decode(Encode(corrupt),restored),"Invalid cursor rejected");
    corrupt=groups;corrupt[0].id=9;Check(!Decode(Encode(corrupt),restored),"Invalid group IDs rejected");
    Check(!Decode(Encode(groups)+"garbage",restored) && !Decode(Encode(groups),restored,2) &&
        !Decode("FWCY 2\n",restored) && !Decode(std::string(65537,'x'),restored),"Trailing data, unknown versions and oversized input rejected");
    groups[0].key=groups[1].key=-1;groups[0].modifiers=groups[1].modifiers=0;
    Check(Valid(groups) && !Match(groups,-1,0,true),"Several disabled shortcuts coexist without matching an event");
    // Use the real input gate to protect down/hold/up after the modifier releases.
    groups=Defaults();Append(groups[0],10);InputGate gate;
    const bool owned=Match(groups,2,1,false)!=0;
    Check(gate.Filter(2,true,false,owned)==InputGate::Result::Suppress,"Cycle down does not also trigger native slot 1");
    Check(gate.Filter(2,true,false,false)==InputGate::Result::Suppress && gate.Filter(2,false,true,false)==InputGate::Result::Suppress,
        "Releasing Shift before the main key cannot leak a held or release event to vanilla");
    Check(gate.Filter(2,true,false,Match(groups,2,0,false)!=0)==InputGate::Result::Pass,"A new unmodified 1 press keeps native behavior");
    Ac3_CycleRecordsIgnoreDigitGrouping();
    std::cout<<"Combat cycle model, serialization, conflict and input lifecycle tests passed\n";
}
// AC-3: preset IDs above 999 are formatted with the process locale unless the
// record fixes classic, so another component's digit grouping must not change
// the saved bytes or block a reload.
void Ac3_CycleRecordsIgnoreDigitGrouping() {
    using namespace Wheel;using namespace Wheel::Cycles;
    struct GroupedDigits:std::numpunct<char> {
        char do_thousands_sep()const override{return ',';}
        std::string do_grouping()const override{return "\3";}
    };
    auto groups=Defaults();
    Check(Append(groups[0],999999)==AppendResult::Added,"AC-3: fixture appends a six-digit preset");
    groups[0].current=999999;
    Check(Append(groups[1],1234)==AppendResult::Added && Append(groups[1],5678)==AppendResult::Added,"AC-3: fixture appends four-digit presets");
    groups[1].current=1234;
    Check(Valid(groups),"AC-3: the fixture record is a valid cycle group set");
    const auto original=std::locale();
    std::locale::global(std::locale::classic());const auto canonical=Encode(groups);
    std::locale::global(std::locale(std::locale::classic(),new GroupedDigits));const auto grouped=Encode(groups);
    Groups fromGrouped,fromCanonical;
    const bool groupedDecoded=Decode(grouped,fromGrouped),canonicalDecoded=Decode(canonical,fromCanonical);
    std::locale::global(original);
    Check(grouped==canonical,"AC-3: digit grouping in the global locale cannot change the cycle record bytes");
    Check(groupedDecoded && fromGrouped==groups,"AC-3: the cycle record decodes under a grouped global locale");
    Check(canonicalDecoded && fromCanonical==groups,"AC-3: a classic-locale record loads under a grouped global locale");
}
