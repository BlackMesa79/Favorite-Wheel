#pragma once
#include "I4Entry.h"
namespace RE { class TESForm; class InventoryEntryData; }
namespace Wheel::I4 {
    LoadReport LoadFromGame();
    Icon ResolveItem(Entry&, std::uint32_t formId, std::string_view name);
    RawFields Collect(const RE::TESForm*, const RE::InventoryEntryData*);
}
