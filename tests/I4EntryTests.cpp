#include "I4Entry.h"
#include <cstdlib>
#include <iostream>
using namespace Wheel::I4;
namespace {
int checks = 0;
void Check(bool ok, const char* expression, int line) {
    ++checks;
    if (!ok) { std::cerr << "I4EntryTests:" << line << ": " << expression << '\n'; std::exit(1); }
}
#define CHECK(x) Check(bool(x), #x, __LINE__)
bool Is(const Entry& e, std::string_view k, Value v) { auto p=e.Get(k); return p && *p==v; }
void Armor() {
    RawFields r; r.formType=26;
    // InventoryDataSetter.as:115-142: light/heavy survive both clothing keywords.
    for (int weight : {0,1}) {
        r.armorType=weight;
        for (auto keys : {Value::Keywords{},Value::Keywords{"VendorItemClothing","VendorItemJewelry"}}) {
            r.keywords=keys; CHECK(Is(Derive(r),"weightClass",weight));
        }
    }
    // :118,132-140: NONE -> null, clothing before jewelry, unknown preserved.
    r.armorType=2; r.keywords.reset(); CHECK(Is(Derive(r),"weightClass",Value{}));
    r.keywords=Value::Keywords{}; CHECK(Is(Derive(r),"weightClass",Value{}));
    r.armorType=99; CHECK(Is(Derive(r),"weightClass",99));
    r.keywords=Value::Keywords{"VendorItemJewelry"}; CHECK(Is(Derive(r),"weightClass",4));
    r.keywords->insert("VendorItemClothing"); CHECK(Is(Derive(r),"weightClass",3));
    r.armorType.reset(); r.keywords.reset(); CHECK(Is(Derive(r),"weightClass",Value{}));
    // :415-503 and Armor.as:13-111: each named and unnamed slot, plus Other fallback.
    constexpr int subtype[] = {0,1,3,5,4,10,12,8,7,6,13,2,9,11};
    for (unsigned bit=0;bit<32;++bit) {
        r.partMask=1u<<bit;
        auto e=Derive(r);
        CHECK(Is(e,"mainPartMask",1u<<bit));
        CHECK(Is(e,"subType",bit<14?Value(subtype[bit]):Value(1u<<bit)));
        CHECK(Is(e,"weightClass",bit>=14?Value{}:Value(bit==5||bit==6||bit==12||bit==13?4:3)));
        // :508 early return: classified armor must not become clothing/jewelry.
        r.armorType=0; CHECK(Is(Derive(r),"weightClass",0)); r.armorType.reset();
    }
    // :421-430: every precedence relation, not numeric slot order.
    constexpr unsigned order[]={2,1,3,4,7,8,9,5,6,11,13,0,12,10,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31};
    for (unsigned i=0;i<32;++i) for (unsigned j=i+1;j<32;++j) {
        r.partMask=(1u<<order[i])|(1u<<order[j]); CHECK(Is(Derive(r),"mainPartMask",1u<<order[i]));
    }
    r.partMask=0; CHECK(!Derive(r).Get("subType")); CHECK(!Derive(r).Get("mainPartMask"));
    r.partMask.reset(); CHECK(!Derive(r).Get("mainPartMask"));
    // :536-547: wedding wreath changes only class; vampire lord changes only subtype.
    r.formId=0xAB08895A; r.armorType=1; CHECK(Is(Derive(r),"weightClass",4));
    r.formId=0x03011A84; CHECK(Is(Derive(r),"subType",3)); CHECK(Is(Derive(r),"weightClass",1));
}
void Weapon() {
    RawFields r; r.formType=41;
    // InventoryDataSetter.as:325-395; Weapon.as:3-38. Manual expected animation map.
    constexpr int expected[]={0,1,2,3,4,5,6,8,10,9,0,1,2,3,4,5,6,8,10,9};
    for (int i=0;i<20;++i) {
        r.weaponType=i; r.keywords.reset(); CHECK(Is(Derive(r),"subType",expected[i]));
        r.keywords=Value::Keywords{"WeapTypeWarhammer"}; CHECK(Is(Derive(r),"subType",i==6||i==16?7:expected[i]));
    }
    for (double n:{-1.,20.,1.5}) { r.weaponType=n; CHECK(Is(Derive(r),"subType",Value{})); }
    r.weaponType.reset(); CHECK(Is(Derive(r),"subType",Value{}));
    // :398-413: each base-ID override, including nonzero plugin index.
    for (auto id:{0x0E3C16u,0x06A707u,0x1019D4u}) { r.formId=0x05000000|id; CHECK(Is(Derive(r),"subType",11)); }
    for (auto id:{0x02F2F4u,0x0AE086u}) { r.formId=id; CHECK(Is(Derive(r),"subType",12)); }
}
void BookAmmoKey() {
    RawFields r; r.formType=27;
    // InventoryDataSetter.as:550-572: note, ordinary, read flags, missing/empty keywords,
    // recipe > tome > note. BOOKTYPE_NOTE = 255 (Item.as:65).
    CHECK(Is(Derive(r),"subType",Value{})); CHECK(Is(Derive(r),"isRead",false));
    r.bookType=0; r.keywords=Value::Keywords{}; CHECK(Is(Derive(r),"subType",Value{}));
    r.bookType=255; r.flags=8; CHECK(Is(Derive(r),"subType",1)); CHECK(Is(Derive(r),"isRead",true));
    r.keywords=Value::Keywords{"VendorItemSpellTome"}; CHECK(Is(Derive(r),"subType",0));
    r.keywords->insert("VendorItemRecipe"); CHECK(Is(Derive(r),"subType",2));
    r.flags=4; CHECK(Is(Derive(r),"isRead",false));
    // :574-583: nonbolt bit, not equality with 4; missing flags converts to zero.
    r={}; r.formType=42; CHECK(Is(Derive(r),"subType",1));
    for (unsigned flags:{0u,1u,2u,4u,5u,7u}) { r.flags=flags; CHECK(Is(Derive(r),"subType",(flags&4)?0:1)); }
    // :659-668: key only changes price/display (excluded by plan 4.1).
    r={}; r.formType=45; CHECK(!Derive(r).Get("subType")); CHECK(!Derive(r).Get("infoValue"));
}
void Potion() {
    RawFields r; r.formType=46;
    // InventoryDataSetter.as:670-742; Actor.as / Item.as numeric constants.
    CHECK(Is(Derive(r),"subType",12));
    constexpr int av[]={24,27,155,25,28,156,26,29,157,41,42,43};
    for (int i=0;i<12;++i) { r.actorValue=av[i]; CHECK(Is(Derive(r),"subType",i)); }
    r.actorValue=-1; CHECK(Is(Derive(r),"subType",12));
    r.actorValue=24; r.flags=0x20000; CHECK(Is(Derive(r),"subType",15));
    r.flags=2|0x20000; CHECK(Is(Derive(r),"subType",14));
    r.useSoundFormId=0; CHECK(Is(Derive(r),"subType",14));
    r.useSoundFormId=0xB6435; CHECK(Is(Derive(r),"subType",13));
    r.useSoundFormId=0x010B6435; CHECK(Is(Derive(r),"subType",14));
    r.flags=0x10000; CHECK(Is(Derive(r),"subType",0));
}
void SoulGem() {
    RawFields r; r.formType=52;
    // InventoryDataSetter.as:744-762: missing size, none, every size, empty/partial/full.
    CHECK(Is(Derive(r),"subType",Value{})); CHECK(Is(Derive(r),"status",0));
    r.soulSize=1; CHECK(Is(Derive(r),"status",0)); r.soulSize.reset();
    r.gemSize=0; CHECK(Is(Derive(r),"subType",Value{}));
    for (int size=1;size<=6;++size) {
        r.gemSize=size; r.soulSize.reset(); CHECK(Is(Derive(r),"subType",size)); CHECK(Is(Derive(r),"status",0));
        r.soulSize=0; CHECK(Is(Derive(r),"status",0));
        r.soulSize=size; CHECK(Is(Derive(r),"status",2));
        r.soulSize=size+1; CHECK(Is(Derive(r),"status",2));
        if(size>1) { r.soulSize=size-1; CHECK(Is(Derive(r),"status",1)); }
    }
    // :764-772: both Azura and Black Star; original status retained.
    for (auto id:{0x063B29u,0x063B27u}) { r.formId=id; CHECK(Is(Derive(r),"subType",6)); CHECK(Is(Derive(r),"status",1)); }
}
void Misc() {
    RawFields r; r.formType=32;
    // InventoryDataSetter.as:774-834: every keyword branch (each OR operand separately).
    constexpr std::pair<const char*,int> cases[]={
        {"BYOHAdoptionClothesKeyword",9},{"BYOHAdoptionToyKeyword",10},
        {"BYOHHouseCraftingCategoryWeaponRacks",18},{"BYOHHouseCraftingCategoryShelf",18},
        {"BYOHHouseCraftingCategoryFurniture",18},{"BYOHHouseCraftingCategoryExterior",18},
        {"BYOHHouseCraftingCategoryContainers",18},{"BYOHHouseCraftingCategoryBuilding",18},
        {"BYOHHouseCraftingCategorySmithing",18},{"VendorItemDaedricArtifact",2},
        {"VendorItemGem",0},{"VendorItemAnimalHide",5},{"VendorItemTool",8},
        {"VendorItemAnimalPart",6},{"VendorItemOreIngot",7},{"VendorItemClutter",19},{"VendorItemFirewood",11}
    };
    CHECK(Is(Derive(r),"subType",Value{}));
    r.keywords=Value::Keywords{}; CHECK(Is(Derive(r),"subType",Value{}));
    for(auto [key,expected]:cases) { r.keywords=Value::Keywords{key}; CHECK(Is(Derive(r),"subType",expected)); }
    for(unsigned i=0;i<std::size(cases);++i) for(unsigned j=i+1;j<std::size(cases);++j) {
        r.keywords=Value::Keywords{cases[i].first,cases[j].first}; CHECK(Is(Derive(r),"subType",cases[i].second));
    }
    // :836-882: every base ID independently overrides the keywords.
    constexpr std::pair<unsigned,int> bases[]={
        {0x06851E,0},{0x04B56C,1},{0x0AB7BB,1},{0x07C260,1},{0x05AF48,1},{0x0ED417,1},
        {0x0AB375,1},{0x08CDFA,1},{0x0B634C,1},{0x0999E7,1},{0x0663D7,1},{0x039647,1},
        {0xA,20},{0xF,21},{0x0DB5D2,3},{0x0800E4,4}
    };
    for(auto [id,expected]:bases) { r.formId=0x04000000|id; CHECK(Is(Derive(r),"subType",expected)); }
}
void MagicAndRaw() {
    RawFields r; r.magicMenu=true; r.formType=22; r.formId=0xFE123ABC;
    r.school=20; r.resistance=41; r.actorValue=24; r.archetype=1; r.deliveryType=2;
    r.castType=1; r.spellType=2; r.effectFlags=5; r.effectKeywords=Value::Keywords{"Fire"}; r.skillLevel=75;
    // MagicDataSetter.as:26: literal low-24-bit base ID, even for ESL.
    CHECK(Is(Derive(r),"baseId",0x123ABC));
    // :32,55,84,103-109: shout/effect/spell preserve skillLevel; power/default clear it.
    for (double type:{7.,10.,11.,13.,0.,99.}) {
        r.type=type; auto e=Derive(r);
        CHECK(Is(e,"skillLevel",type==7||type==10||type==11?Value(75):Value{}));
        CHECK(Is(e,"school",20)); CHECK(Is(e,"resistance",41)); CHECK(Is(e,"actorValue",24));
        CHECK(Is(e,"archetype",1)); CHECK(Is(e,"deliveryType",2)); CHECK(Is(e,"castType",1));
        CHECK(Is(e,"spellType",2)); CHECK(Is(e,"effectFlags",5)); CHECK(Is(e,"effectKeywords",Value::Keywords{"Fire"}));
        CHECK(!e.Get("subType")); CHECK(!e.Get("timeRemainingDisplay"));
    }
    r.type.reset(); CHECK(Is(Derive(r),"skillLevel",Value{}));
    r={}; r.formType=27; r.equipSlot=0x13F44; r.teachesSpell=0x12345; r.teachesSkill=18; r.flags=8;
    auto e=Derive(r); CHECK(Is(e,"equipSlot",0x13F44)); CHECK(Is(e,"teachesSpell",0x12345)); CHECK(Is(e,"teachesSkill",18)); CHECK(Is(e,"flags",8));
    // InventoryDataSetter.as:36-109: scroll/ingredient/light/default have no derivation
    // beyond excluded display fields. No engine access or rule resolution in Derive.
    for(unsigned t:{0u,23u,30u,31u,119u}) { r={}; r.formType=t; CHECK(!Derive(r).Get("subType")); CHECK(!Derive(r).Get("iconLabel")); }
    auto a=Derive(r), b=Derive(r); CHECK(a.fields==b.fields);
}
}
int main() { Armor(); Weapon(); BookAmmoKey(); Potion(); SoulGem(); Misc(); MagicAndRaw(); std::cout<<"I4EntryTests: "<<checks<<" checks passed\n"; }
