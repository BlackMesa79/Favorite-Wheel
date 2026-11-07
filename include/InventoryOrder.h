#pragma once
#include "Favorites.h"
#include "InventoryOrderModel.h"
#include <SKSE/SKSE.h>
namespace Wheel::Ordering {
    constexpr std::uint32_t record=0x46575352;
    Mode Current(bool all,Category category);
    void Cycle(bool all,Category category);
    void SetCustom(bool all,Category category,const std::vector<Item>& visibleOrder);
    void Apply(std::vector<Item>& items,bool all);
    bool MoveItem(bool all,Category category,const Item& selected,const Item& neighbor);
    bool InsertItem(bool all,Category category,const Item& selected,const Item& target);
    void Save(SKSE::SerializationInterface* api);
    void Load(SKSE::SerializationInterface* api,std::uint32_t version,std::uint32_t length);
    void Revert();
}
