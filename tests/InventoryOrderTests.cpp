#include "InventoryOrderModel.h"
#include "SortDrag.h"
#include <cstdlib>
#include <iostream>
#include <limits>
using namespace Wheel::Ordering;
struct Row {Key key;std::string name;bool equipped=false;int sortType=999,sortSlot=999;std::optional<float> sortNumber;};
void Check(bool ok,const char* why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
void Ac4_RemoveDuplicateKeys();
int main(){
    std::vector<Row> source{{{1,0,0,0,"B"},"B",false},{{2,0,0,0,"A"},"A",true},{{3,0,0,0,"C"},"C",false}};
    auto id=[](const Row& r){return r.key;};Group g;
    auto rows=source;Sort(g,rows.begin(),rows.end(),id);Check(rows[0].key.form==1,"Default keeps original order");
    g.mode=Mode::NameAscending;Sort(g,rows.begin(),rows.end(),id);Check(rows[0].name=="A" && rows[2].name=="C","Name ascending");
    g.mode=Mode::NameDescending;Sort(g,rows.begin(),rows.end(),id);Check(rows[0].name=="C" && rows[2].name=="A","Name descending");
    rows=source;g.mode=Mode::EquippedFirst;Sort(g,rows.begin(),rows.end(),id);Check(rows[0].equipped && rows[1].name=="B","Equipped first, stable remainder");
    rows=source;rows[0].sortType=2;rows[1].sortType=1;rows[2].sortType=1;
    g.mode=Mode::Type;Sort(g,rows.begin(),rows.end(),id);
    Check(rows[0].name=="A" && rows[1].name=="C" && rows[2].name=="B","Type groups then name within group");
    rows[0].sortSlot=5;rows[1].sortSlot=0;rows[2].sortSlot=999;g.mode=Mode::Slot;
    Sort(g,rows.begin(),rows.end(),id);Check(rows[0].name=="C" && rows[2].name=="B","Slots and unknown slot last");
    Check(NextMode(Mode::Custom,false,false)==Mode::Default,"Unsupported category skips type/slot modes");
    Check(NextMode(Mode::Custom,true,false)==Mode::Type && NextMode(Mode::Type,true,false)==Mode::Default,"Weapon and spell modes skip armor slots");
    Check(NextMode(Mode::Type,true,true)==Mode::Slot,"Armor exposes slot sorting");
    State extended;extended[0].mode=Mode::Type;extended[1].mode=Mode::Slot;extended[2].mode=Mode::Custom;State restored;
    Check(Decode(Encode(extended),restored) && Encode(extended)==Encode(restored),"New modes persist without renumbering old custom mode");
    Check(!Decode("FWS 1\n11 0\n",restored),"Invalid new mode rejected");
    for(auto mode:{Mode::StatAscending,Mode::StatDescending,Mode::WeightAscending,Mode::ValueDescending}) {
        rows=source;rows[0].sortNumber=0.f;rows[1].sortNumber=10.f;rows[2].sortNumber=10.f;
        rows.push_back({{4},"Unknown"});rows.push_back({{5},"Invalid"});rows.back().sortNumber=std::numeric_limits<float>::quiet_NaN();
        g.mode=mode;Sort(g,rows.begin(),rows.end(),id);
        const bool asc=mode==Mode::StatAscending || mode==Mode::WeightAscending;
        Check(rows[0].name==(asc?"B":"A") && rows[1].name==(asc?"A":"C"),"Numeric direction, zero value and stable name tie-break");
        Check(rows[3].name=="Invalid" && rows[4].name=="Unknown","Missing/non-finite numeric stats sort last in either direction");
        State numeric;numeric[0].mode=mode;State decoded;
        Check(Decode(Encode(numeric),decoded) && decoded[0].mode==mode,"Numeric modes survive serialization");
    }
    Check(NextMode(Mode::StatAscending,true,false,true,false)==Mode::Default,"Spell cost modes skip physical weight/value");
    Check(NextMode(Mode::StatAscending,true,true,true,true)==Mode::WeightAscending &&
        NextMode(Mode::WeightAscending,true,true,true,true)==Mode::ValueDescending,"Physical items expose weight and value modes");
    g={Mode::Custom,{}};rows=source;Sort(g,rows.begin(),rows.end(),id);
    Check(Move(g,source[0].key,source[1].key),"Manual move");Sort(g,rows.begin(),rows.end(),id);Check(rows[0].name=="A","Manual order applied");
    rows={source[0],source[2]};Sort(g,rows.begin(),rows.end(),id);Check(g.order.size()==3,"Missing entry retains position");
    rows=source;rows.push_back({{4,0,0,0,"AA"},"AA",false});Sort(g,rows.begin(),rows.end(),id);Check(rows[0].name=="A" && rows.back().name=="AA","Returning entry restored and new entry appended");
    rows.push_back(rows.back());Sort(g,rows.begin(),rows.end(),id);Check(g.order.size()==4,"Indistinguishable rows share a stable rank");
    State state;state[0]=g;state[9].mode=Mode::NameDescending;State loaded;
    Check(Decode(Encode(state),loaded) && Encode(state)==Encode(loaded),"All scopes/modes round-trip");
    {
        struct GroupedDigits:std::numpunct<char> {
            char do_thousands_sep()const override{return ',';}
            std::string do_grouping()const override{return "\3";}
        };
        State localized=state;localized[0].order.push_back({1234567,2345678,3456789,12345,"Quoted \"name\" 中文"});
        const auto originalLocale=std::locale();
        std::locale::global(std::locale::classic());const auto canonical=Encode(localized);
        std::locale::global(std::locale(std::locale::classic(),new GroupedDigits));
        const auto grouped=Encode(localized);State parsed;
        const bool loadedInGroupedLocale=Decode(canonical,parsed);
        std::locale::global(originalLocale);
        Check(grouped==canonical && loadedInGroupedLocale && Encode(parsed)==canonical,
            "Other plugins changing process digit grouping cannot alter saved ordering or prevent its reload");
    }
    const auto before=Encode(loaded);Check(!Decode(Encode(state)+"garbage",loaded) && Encode(loaded)==before,"Malformed input is transactional");
    auto duplicate=state;duplicate[0].order.push_back(duplicate[0].order[0]);Check(!Decode(Encode(duplicate),loaded),"Duplicate persisted keys rejected");
    Check(!Decode("FWS 2",loaded),"Unknown version rejected");
    std::vector<Row> many;for(unsigned i=1;i<=25;++i)many.push_back({{i,0,0,0,std::to_string(i)},std::to_string(i),false});
    Group cross{Mode::Custom,{}};Sort(cross,many.begin(),many.end(),id);auto moving=many[9].key;
    Check(Move(cross,moving,many[10].key),"Cross-page move accepted");Sort(cross,many.begin(),many.end(),id);Check(many[10].key==moving,"Moved item follows into next page");
    {
        auto dragRows=source;Group drag{Mode::Custom,{}};Sort(drag,dragRows.begin(),dragRows.end(),id);
        Check(InsertAt(drag,source[0].key,source[2].key),"Drop forward inserts rather than swaps");
        Sort(drag,dragRows.begin(),dragRows.end(),id);
        Check(dragRows[0].name=="A" && dragRows[1].name=="C" && dragRows[2].name=="B","Intervening entries shift in order");
        Check(InsertAt(drag,source[0].key,source[1].key),"Drop backward inserts at target");
        Sort(drag,dragRows.begin(),dragRows.end(),id);Check(dragRows[0].name=="B" && dragRows[1].name=="A","Backward insertion restores source order");
        const auto before=drag.order;
        Check(!InsertAt(drag,source[0].key,source[0].key) && !InsertAt(drag,Key{999},source[0].key) && drag.order==before,"Same-slot and vanished source drops are no-ops");
        Check(InsertAt(cross,moving,many.back().key),"Drag across page boundaries");
        Sort(cross,many.begin(),many.end(),id);Check(many.back().key==moving,"Cross-page insertion lands at target position");
        Wheel::SortDrag gesture;gesture.Begin(.5f,0);gesture.Move(.51f,0);
        Check(gesture.held && !gesture.active,"Pointer jitter remains a short click");
        gesture.Move(.6f,0);gesture.Move(.5f,0);Check(gesture.active,"Drag stays active after returning to its origin");
        gesture.Cancel();Check(!gesture.held && !gesture.active,"Cancellation clears the gesture");
        gesture.Move(1,1);Check(!gesture.active,"Late movement cannot revive a canceled drag");
        gesture.Begin(0,0);gesture.Page();Check(gesture.active,"Paging while held cannot fall back to short-click movement");
    }
    Ac4_RemoveDuplicateKeys();
    std::cout<<"Inventory ordering modes, retained identities, scopes, cross-page moves and serialization passed\n";
}
// AC-4: renumbered FormIDs can make two stored keys share one identity after a
// load. Deduplication keeps the first occurrence so the next save stays decodable;
// Decode itself still rejects a record that was written with duplicates.
void Ac4_RemoveDuplicateKeys() {
    State state;
    state[0].mode=Mode::Type;
    state[0].order={{100,0,0,0,"Sword"},{200,0,0,0,"Axe"},{100,0,0,0,"Sword"},{300,0,0,0,"Shield"}};
    state[3].mode=Mode::NameDescending;
    state[3].order={{400,0,0,0,"Helmet"},{500,0,0,0,"Boots"}};
    state[7].mode=Mode::Custom;
    state[7].order={{600,0,0,0,"Ring"},{600,0,0,0,"Ring"},{600,0,0,0,"Ring"}};
    State probe;
    Check(!Decode(Encode(state),probe),"AC-4: a record written with duplicate identities is still rejected");
    const auto before=state;
    RemoveDuplicateKeys(state);
    Check(state[0].order.size()==3 && Token(state[0].order[0])=="100:0:0:0:Sword" &&
        Token(state[0].order[1])=="200:0:0:0:Axe" && Token(state[0].order[2])=="300:0:0:0:Shield",
        "AC-4: a repeated key collapses to its first position and the relative order is kept");
    Check(state[7].order.size()==1 && Token(state[7].order[0])=="600:0:0:0:Ring",
        "AC-4: a group of identical keys keeps exactly one entry");
    for(int index=0;index<groupCount;++index) {
        Check(state[index].mode==before[index].mode,"AC-4: deduplication never changes a sort mode");
        if(index!=0 && index!=7)
            Check(state[index].order==before[index].order,"AC-4: groups without duplicates are untouched");
    }
    State decoded;
    Check(Decode(Encode(state),decoded) && Encode(decoded)==Encode(state),
        "AC-4: the deduplicated state saves and loads again");
    const auto deduplicated=Encode(state);
    RemoveDuplicateKeys(state);
    Check(Encode(state)==deduplicated,"AC-4: deduplication is idempotent");
    std::string legacy="FWS 1\n";
    for(int i=0;i<18;++i)legacy+=(i==9?"2 0\n":"0 0\n");
    State migrated;
    Check(Decode(legacy,migrated) && migrated[9].mode==Mode::NameDescending &&
        migrated[18].mode==Mode::Default && migrated[19].order.empty(),
        "Legacy sorting preserves both inventory scopes and initializes book groups");
    State loaded; migrated[18].mode=Mode::NameAscending;migrated[19].mode=Mode::Custom;
    Check(Decode(Encode(migrated),loaded) && loaded[18].mode==Mode::NameAscending && loaded[19].mode==Mode::Custom,
        "Book groups round trip independently");

}
