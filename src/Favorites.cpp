#include "Favorites.h"
#include "Settings.h"
#include "ActorRuntime.h"
#include "UIResources.h"
#include "EquipPolicy.h"
#include "ItemInfoCapture.h"
#include <chrono>

namespace Wheel {
    namespace {
        ItemKey Key(RE::TESBoundObject* object, RE::ExtraDataList* extra) {
            ItemKey key{object->GetFormID(), reinterpret_cast<std::uintptr_t>(extra)};
            if (extra) {
                if (const auto uid = extra->GetByType<RE::ExtraUniqueID>()) key.unique = uid->uniqueID;
                if (const auto ench = extra->GetByType<RE::ExtraEnchantment>(); ench && ench->enchantment) key.enchantment = ench->enchantment->GetFormID();
                if (const auto health = extra->GetByType<RE::ExtraHealth>()) key.health = health->health;
            }
            return key;
        }
        Category Classify(RE::TESBoundObject* form) {
            if (form->As<RE::TESObjectWEAP>() || form->As<RE::TESAmmo>()) return Category::Weapons;
            if (form->As<RE::TESObjectARMO>()) return Category::Armor;
            if (const auto potion = form->As<RE::AlchemyItem>()) {
                if (potion->IsPoison()) return Category::Other;
                return potion->IsFood() ? Category::Food : Category::Potions;
            }
            return Category::Other;
        }
        IconKind ClassifyIcon(RE::TESBoundObject* form) {
            if(auto weapon=form->As<RE::TESObjectWEAP>()) {
                switch(weapon->GetWeaponType()) {
                case RE::WEAPON_TYPE::kOneHandDagger:return IconKind::Dagger;
                case RE::WEAPON_TYPE::kOneHandAxe:case RE::WEAPON_TYPE::kTwoHandAxe:return IconKind::Axe;
                case RE::WEAPON_TYPE::kOneHandMace:return IconKind::Mace;
                case RE::WEAPON_TYPE::kBow:return IconKind::Bow;
                case RE::WEAPON_TYPE::kCrossbow:return IconKind::Crossbow;
                case RE::WEAPON_TYPE::kStaff:return IconKind::Staff;
                default:return IconKind::Sword;
                }
            }
            if(form->As<RE::TESAmmo>())return IconKind::Arrow;
            if(auto armor=form->As<RE::TESObjectARMO>()) {
                using Slot=RE::BGSBipedObjectForm::BipedObjectSlot;
                if(armor->IsShield())return IconKind::Shield;
                if(armor->IsChestpiece())return armor->IsClothing()?IconKind::Robe:IconKind::Armor;
                if(armor->IsHelmet() || armor->IsCirclet())return IconKind::Helmet;
                if(armor->IsGauntlets())return IconKind::Gloves;
                if(armor->IsBoots())return IconKind::Boots;
                if(armor->HasPartOf(Slot::kRing))return IconKind::Ring;
                if(armor->HasPartOf(Slot::kAmulet))return IconKind::Amulet;
                return IconKind::Armor;
            }
            if(auto potion=form->As<RE::AlchemyItem>())return potion->IsFood()?IconKind::Food:IconKind::Potion;
            if(form->As<RE::ScrollItem>())return IconKind::Scroll;
            if(form->As<RE::TESObjectLIGH>())return IconKind::Torch;
            return IconKind::Other;
        }
        bool Supported(RE::TESBoundObject* form) {
            if (form->As<RE::TESObjectWEAP>() || form->As<RE::TESAmmo>() || form->As<RE::TESObjectARMO>() || form->As<RE::ScrollItem>() || form->As<RE::TESObjectLIGH>()) return true;
            if (auto potion = form->As<RE::AlchemyItem>()) return !potion->IsPoison();
            return false;
        }
        bool Plain(RE::ExtraDataList* extra) {
            return !extra->HasType<RE::ExtraEnchantment>() && !extra->HasType<RE::ExtraHealth>() &&
                !extra->HasType<RE::ExtraTextDisplayData>() && !extra->HasType<RE::ExtraUniqueID>();
        }
        void Notify(const char* key) { RE::SendHUDMessage::ShowHUDMessage(Tr(Config(),key).c_str()); }
        RE::BGSEquipSlot* Hand(bool left) {
            const auto defaults = RE::BGSDefaultObjectManager::GetSingleton();
            return defaults ? defaults->GetObject<RE::BGSEquipSlot>(left ? RE::DEFAULT_OBJECT::kLeftHandEquip : RE::DEFAULT_OBJECT::kRightHandEquip) : nullptr;
        }
    }

    std::vector<Item> CollectFavorites(bool includeInfo) {
        const auto started=std::chrono::steady_clock::now();
        std::vector<Item> result;
        const auto player = RE::PlayerCharacter::GetSingleton();
        if (!player) return result;
        auto inventory = player->GetInventory();
        for (auto& [object, data] : inventory) {
            const auto& [count, entry] = data;
            if (!object || count <= 0 || !entry || !entry->IsFavorited() || !entry->extraLists) continue;
            int represented = 0;
            for (auto extra : *entry->extraLists) if (extra) represented += std::max(0, extra->GetCount());
            int untracked = std::max(0, count - represented);
            int remaining = count;
            for (auto extra : *entry->extraLists) {
                if (!extra || !extra->HasType<RE::ExtraHotkey>() || remaining <= 0) continue;
                int amount = std::max(1, extra->GetCount());
                if (Plain(extra)) { amount += untracked; untracked = 0; }
                amount = std::min(remaining, amount);
                remaining -= amount;
                const char* name = extra->GetDisplayName(object);
                if (!name || !*name) name = object->GetName();
                result.push_back({Key(object, extra), Classify(object), name && *name ? name : "?", amount,
                    extra->HasType<RE::ExtraWorn>() || extra->HasType<RE::ExtraWornLeft>(), false, Supported(object), ActionKind::Favorite, 0, ClassifyIcon(object)});
                if(includeInfo)result.back().info=CaptureItemInfo(object,extra,player);
            }
        }
        if (auto favorites = RE::MagicFavorites::GetSingleton()) {
            for (auto form : favorites->spells) {
                if (!form) continue;
                const auto spell = form->As<RE::SpellItem>();
                const auto shout = form->As<RE::TESShout>();
                if ((!spell || !player->HasSpell(spell)) && (!shout || !player->HasShout(shout))) continue;
                const char* name = form->GetName();
                bool equipped = player->GetEquippedObject(false) == form || player->GetEquippedObject(true) == form;
                if (auto defaults = RE::BGSDefaultObjectManager::GetSingleton()) {
                    if (auto slot = defaults->GetObject<RE::BGSEquipSlot>(RE::DEFAULT_OBJECT::kVoiceEquip))
                        equipped |= player->GetEquippedObjectInSlot(slot) == form;
                }
                result.push_back({{form->GetFormID()}, Category::Magic, name && *name ? name : "?", 1, equipped, true, true});
                if(includeInfo && spell)result.back().info=CaptureSpellInfo(spell,player);
            }
        }
        std::sort(result.begin(), result.end(), [](const Item& a, const Item& b) {
            if (a.category != b.category) return a.category < b.category;
            if (a.name != b.name) return a.name < b.name;
            if (a.key.form != b.key.form) return a.key.form < b.key.form;
            if (a.key.unique != b.key.unique) return a.key.unique < b.key.unique;
            return a.key.extra < b.key.extra;
        });
        const auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
        if(elapsed>8)SKSE::log::info("Favorites snapshot: items={} details={} total_ms={:.2f}",result.size(),includeInfo,elapsed);
        return result;
    }

    void UseFavorite(const Item& requested, bool leftHand) {
        const auto player = RE::PlayerCharacter::GetSingleton();
        const auto manager = RE::ActorEquipManager::GetSingleton();
        if (!player || !manager || ActorRuntime::IsDead(player)) return;
        // Resolve again at execution time. A queued action must never use an expired extra-list pointer.
        const auto current = CollectFavorites(false); // Execution only needs identity; do not repeat UI calculations.
        const auto found = std::find_if(current.begin(), current.end(), [&](const Item& item) { return item.key == requested.key; });
        if (found == current.end()) {
            Notify("changed");
            SKSE::log::warn("Favorite no longer matches: {:08X}", requested.key.form);
            return;
        }
        if (!found->usable) { Notify("unsupported"); return; }
        const auto form = RE::TESForm::LookupByID(requested.key.form);
        if (!form) return;
        if (auto shout = form->As<RE::TESShout>()) { manager->EquipShout(player, shout); return; }
        if (auto spell = form->As<RE::SpellItem>()) {
            auto slot = spell->GetEquipSlot();
            const auto type = spell->GetSpellType();
            if (type == RE::MagicSystem::SpellType::kSpell && !spell->IsTwoHanded()) slot = Hand(leftHand);
            if (slot) manager->EquipSpell(player, spell, slot);
            return;
        }
        auto object = form->As<RE::TESBoundObject>();
        if (!object) return;
        // This address was matched against a fresh inventory walk above, on this same game-thread task.
        auto extra = reinterpret_cast<RE::ExtraDataList*>(found->key.extra);
        if (auto potion = object->As<RE::AlchemyItem>()) {
            if (potion->IsPoison()) return;
            // Use the engine equip entry point: UAPNG hooks this path, and EAS
            // listens for OnObjectEquipped. Never also call DrinkPotion or send
            // a synthetic equip event: animation mods may defer consumption.
            manager->EquipObject(player, object, extra, 1);
            SKSE::log::info("Submitted consumable {:08X} through EquipObject (food={}, count=1)", found->key.form, potion->IsFood());
            return;
        }
        RE::BGSEquipSlot* slot = nullptr;
        bool unequipWeapon=false;
        if (auto weapon = object->As<RE::TESObjectWEAP>()) {
            const bool twoHands = weapon->IsTwoHandedSword() || weapon->IsTwoHandedAxe() || weapon->IsBow() || weapon->IsCrossbow();
            slot = twoHands ? weapon->GetEquipSlot() : Hand(leftHand);
            unequipWeapon=UnequipWeapon(twoHands,leftHand,extra && extra->HasType<RE::ExtraWorn>(),
                extra && extra->HasType<RE::ExtraWornLeft>());
        } else if (auto scroll = object->As<RE::ScrollItem>()) {
            slot = scroll->IsTwoHanded() ? scroll->GetEquipSlot() : Hand(leftHand);
        }
        // Match the freshly resolved instance and requested hand, not just the base FormID.
        if (unequipWeapon || (object->As<RE::TESObjectARMO>() && found->equipped))
            manager->UnequipObject(player, object, extra, 1, unequipWeapon?slot:nullptr, false, false, true, true);
        else
            manager->EquipObject(player, object, extra, object->As<RE::TESAmmo>() ? found->count : 1, slot, false, false, true, true);
        SKSE::log::info("Used favorite {:08X}, left={}, extra={:X}", found->key.form, leftHand, found->key.extra);
    }
}
