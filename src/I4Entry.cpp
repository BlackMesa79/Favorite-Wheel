// Field derivation port: schlangster/skyui@8354287.
// src/ItemMenus/InventoryDataSetter.as and MagicDataSetter.as;
// numeric constants: src/Common/skyui/defines/{Armor,Weapon,Item,Actor,Form,Inventory}.as.
// Display strings, material, prices, weights, durations and item-card-only data are
// intentionally outside the phase-2 EntryBuilder contract (plan section 4.1).
#include "I4Entry.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace Wheel::I4 {
namespace {
bool Has(const RawFields& r, std::string_view key) { return r.keywords && r.keywords->contains(key); }
void Armor(Entry& e, const RawFields& r) {
    // processArmorClass, InventoryDataSetter.as:115-142.
    auto weight = r.armorType;
    if (weight == 2) weight.reset();
    if (weight != 0 && weight != 1) {
        if (Has(r,"VendorItemClothing")) weight = 3;
        else if (Has(r,"VendorItemJewelry")) weight = 4;
    }
    // processArmorPartMask, :415-504. Unnamed slots retain the bitmask as subType.
    constexpr unsigned precedence[] = {2,1,3,4,7,8,9,5,6,11,13,0,12,10,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31};
    constexpr int types[] = {0,1,3,5,4,10,12,8,7,6,13,2,9,11};
    if (r.partMask) {
        for (auto bit : precedence) {
            auto mask = 1u << bit;
            if (!(*r.partMask & mask)) continue;
            e.Set("mainPartMask", mask);
            e.Set("subType", bit < 14 ? static_cast<double>(types[bit]) : static_cast<double>(mask));
            // processArmorOther, :506-534.
            if (!weight && bit < 14) weight = bit == 5 || bit == 6 || bit == 12 || bit == 13 ? 4 : 3;
            break;
        }
    }
    // processArmorBaseId, :536-548. SkyUI compares all plugins' low 24 bits.
    auto base = r.formId & 0x00FFFFFFu;
    if (base == 0x08895A) weight = 4;
    if (base == 0x011A84) e.Set("subType", 3);
    if (weight) e.Set("weightClass", *weight);
    else e.Set("weightClass", Value{});
}
void Weapon(Entry& e, const RawFields& r) {
    // processWeaponType, :325-396 (includes both animation enum ranges).
    e.Set("subType", Value{});
    if (r.weaponType && std::isfinite(*r.weaponType) && *r.weaponType >= 0 && *r.weaponType < 20 && std::trunc(*r.weaponType) == *r.weaponType) {
        auto anim = static_cast<unsigned>(*r.weaponType) % 10;
        constexpr unsigned types[] = {0,1,2,3,4,5,6,8,10,9};
        e.Set("subType", anim == 6 && Has(r,"WeapTypeWarhammer") ? 7 : types[anim]);
    }
    // processWeaponBaseId, :398-413.
    switch (r.formId & 0x00FFFFFFu) {
    case 0x0E3C16: case 0x06A707: case 0x1019D4: e.Set("subType",11); break;
    case 0x02F2F4: case 0x0AE086: e.Set("subType",12); break;
    }
}
void Book(Entry& e, const RawFields& r) {
    // processBookType, :550-572. Recipe takes precedence over tome and note.
    e.Set("subType", Value{});
    e.Set("isRead", (r.flags.value_or(0) & 8) != 0);
    if (r.bookType == 255) e.Set("subType",1);
    if (Has(r,"VendorItemRecipe")) e.Set("subType",2);
    else if (Has(r,"VendorItemSpellTome")) e.Set("subType",0);
}
void Potion(Entry& e, const RawFields& r) {
    // processPotionType, :670-742. Food/drink wins over poison and actorValue.
    unsigned sub = 12;
    if (r.flags.value_or(0) & 2) sub = r.useSoundFormId == 0x000B6435u ? 13 : 14;
    else if (r.flags.value_or(0) & 0x20000) sub = 15;
    else {
        constexpr int values[] = {24,27,155,25,28,156,26,29,157,41,42,43};
        for (unsigned i = 0; i < std::size(values); ++i) if (r.actorValue == values[i]) { sub = i; break; }
    }
    e.Set("subType", sub);
}
void SoulGem(Entry& e, const RawFields& r) {
    // processSoulGemType/Status/BaseId, :744-772.
    e.Set("subType", r.gemSize && *r.gemSize != 0 ? Value(*r.gemSize) : Value{});
    e.Set("status", !r.gemSize || !r.soulSize || *r.soulSize == 0 ? 0 : *r.soulSize >= *r.gemSize ? 2 : 1);
    const auto base = r.formId & 0x00FFFFFFu;
    if (base == 0x063B29 || base == 0x063B27) e.Set("subType",6);
}
void Misc(Entry& e, const RawFields& r) {
    // processMiscType, :774-834. Keep the ordered else-if chain.
    e.Set("subType",Value{});
    constexpr std::pair<std::string_view,int> keys[] = {
        {"BYOHAdoptionClothesKeyword",9},{"BYOHAdoptionToyKeyword",10},
        {"BYOHHouseCraftingCategoryWeaponRacks",18},{"BYOHHouseCraftingCategoryShelf",18},
        {"BYOHHouseCraftingCategoryFurniture",18},{"BYOHHouseCraftingCategoryExterior",18},
        {"BYOHHouseCraftingCategoryContainers",18},{"BYOHHouseCraftingCategoryBuilding",18},
        {"BYOHHouseCraftingCategorySmithing",18},{"VendorItemDaedricArtifact",2},
        {"VendorItemGem",0},{"VendorItemAnimalHide",5},{"VendorItemTool",8},
        {"VendorItemAnimalPart",6},{"VendorItemOreIngot",7},{"VendorItemClutter",19},{"VendorItemFirewood",11}
    };
    for (const auto& [key, sub] : keys) if (Has(r,key)) { e.Set("subType",sub); break; }
    // processMiscBaseId, :836-882.
    switch (r.formId & 0x00FFFFFFu) {
    case 0x06851E: e.Set("subType",0); break;
    case 0x04B56C: case 0x0AB7BB: case 0x07C260: case 0x05AF48: case 0x0ED417:
    case 0x0AB375: case 0x08CDFA: case 0x0B634C: case 0x0999E7: case 0x0663D7: case 0x039647:
        e.Set("subType",1); break;
    case 0x00000A: e.Set("subType",20); break;
    case 0x00000F: e.Set("subType",21); break;
    case 0x0DB5D2: e.Set("subType",3); break;
    case 0x0800E4: e.Set("subType",4); break;
    }
}
} // namespace
Entry Derive(const RawFields& raw) {
    Entry e;
    e.Set("formType",raw.formType); e.Set("formId",raw.formId);
    e.Set("baseId",raw.formId & 0x00FFFFFFu);
    auto copy = [&](std::string_view name, const auto& value) { if (value) e.Set(name,*value); };
    copy("keywords",raw.keywords); copy("effectKeywords",raw.effectKeywords);
    copy("partMask",raw.partMask); copy("weightClass",raw.armorType);
    copy("flags",raw.flags); copy("effectFlags",raw.effectFlags); copy("equipSlot",raw.equipSlot);
    copy("teachesSpell",raw.teachesSpell); copy("teachesSkill",raw.teachesSkill);
    copy("weaponType",raw.weaponType); copy("bookType",raw.bookType);
    copy("gemSize",raw.gemSize); copy("soulSize",raw.soulSize); copy("status",raw.status);
    copy("school",raw.school); copy("resistance",raw.resistance); copy("actorValue",raw.actorValue);
    copy("archetype",raw.archetype); copy("deliveryType",raw.deliveryType);
    copy("castType",raw.castType); copy("spellType",raw.spellType); copy("skillLevel",raw.skillLevel);
    copy("type",raw.type);
    if (raw.magicMenu) {
        // MagicDataSetter.as:24-113: shout/effect/spell only alter item-card display
        // fields; powers/default explicitly clear skillLevel. Other raw fields survive.
        if (raw.type != 10 && raw.type != 11 && raw.type != 7) e.Set("skillLevel",Value{});
        return e;
    }
    switch (raw.formType) {
    case 26: Armor(e,raw); break;
    case 27: Book(e,raw); break;
    case 32: Misc(e,raw); break;
    case 41: Weapon(e,raw); break;
    // processAmmoType, InventoryDataSetter.as:574-583.
    case 42: e.Set("subType", (raw.flags.value_or(0) & 4) ? 0 : 1); break;
    // processKeyType (:659-668) only changes display/price; no rule field to derive.
    case 45: break;
    case 46: Potion(e,raw); break;
    case 52: SoulGem(e,raw); break;
    // Scroll/ingredient/light likewise only change display fields in DataSetter.
    default: break;
    }
    return e;
}
} // namespace Wheel::I4
