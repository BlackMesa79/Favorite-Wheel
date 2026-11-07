#pragma once
#include "WheelLogic.h"
#include "FaceLightingAPI.h"
#include "ItemInfo.h"
#include "EquipPolicy.h"
#include <cstdint>
#include <string>
#include <vector>
namespace Wheel {
    struct ItemKey {
        std::uint32_t form = 0;
        std::uintptr_t extra = 0;
        std::uint16_t unique = 0;
        std::uint32_t enchantment = 0;
        float health = 1.0f;
        bool operator==(const ItemKey&) const = default;
    };
    enum class ActionKind { Favorite, Outfit, SaveOutfit, ImportOutfits, FaceLightMenu, FaceLightFollowers, FunctionBack, FaceLightCommand, Combat, SaveCombat, Cycle, CycleMember, CycleCapture, OutfitPiece, AddWorn };
    enum class IconKind { Auto, Sword, Dagger, Axe, Mace, Bow, Crossbow, Staff, Arrow, Armor, Robe, Helmet, Gloves, Boots, Ring, Amulet, Shield, Potion, Food, Magic, Scroll, Torch, Other, LightPlayer, LightTarget, LightGroup, Back, Poison, Alteration, Conjuration, Destruction, Illusion, Restoration, Shout, Power, Book };
    struct Item {
        ItemKey key;
        Category category = Category::Other;
        std::string name;
        int count = 0;
        bool equipped = false;
        bool magic = false;
        bool usable = true;
        ActionKind action=ActionKind::Favorite;
        std::uint32_t actionId=0;
        IconKind icon=IconKind::Auto;
        std::string detail;
        FaceLightingAPI::Request lightRequest;
        ItemInfo info;
        int quickSlot=-1; // Native 1..8 index; unrelated to the radial sector index.
        bool favorited=true;
        bool infoReady=false;
        std::uint64_t infoReadAt=0; // Steady-clock milliseconds; live modes refresh hovered details at most twice/second.
        bool inventoryWide=false;
        HandEquip handEquip=HandEquip::None; // Snapshot data; never read engine slots in Draw.
        bool weaponStack=false; // Plain weapon pool; its native extra-list anchor can change on equip.
        int weaponStackHotkey=-1; // Raw marker distinguishes separate native/extended bindings in a plain pool.
        std::uint32_t actionIndex=0; // Cycle member preset ID; unrelated to item identity or quick slots.
        std::string centreNote; // Selected-item centre only; never added to sector labels.
        std::optional<float> sortNumber; // Active numeric sort only; refreshed with the directory.
        int sortType=999,sortSlot=999; // Lightweight game-thread classification; not persisted identity.
        bool scriptedInstrument=false; // Native use request after closing; no synthetic script events.
        bool book=false; // Reading opens native UI; never retain the wheel.
        std::string iconForm; // Stable plugin|local FormID snapshot for icon mappings.
        bool miscDisplayStack=false; // Display-only aggregate; native bindings remain per instance.
    };
    inline void SetQuickSlotSnapshot(Item& item,int slot) {
        if(item.weaponStack && item.favorited && item.quickSlot!=slot)item.weaponStackHotkey=slot>=0?slot:255;
        item.quickSlot=slot;
    }
    std::vector<Item> CollectFavorites(bool includeInfo=true); // Game thread only; never dereference engine data in Draw.
    std::vector<Item> CollectInventory(bool allInventory); // Lightweight directory; no property/effect calculations.
    bool ReadItemInfo(const Item& item,ItemInfo& info); // Resolve only this form/instance; game thread only.
    void UseFavorite(const Item& item, bool leftHand);
    bool BindFavoriteQuickSlot(const Item& item,int slot,int* assignment=nullptr); // Game-thread inventory mutation, never from Draw.
    bool ClearFavoriteQuickSlot(int slot); // Preserve favorite markers and extended slots.
    bool FavoriteQuickSlotOccupied(int slot); // Read only, on a physical shortcut press.
}
