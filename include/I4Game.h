#pragma once
#include "I4Entry.h"
namespace RE { class TESForm; class InventoryEntryData; }
namespace Wheel::I4 {
    LoadReport LoadFromGame();
    RawFields Collect(const RE::TESForm*, const RE::InventoryEntryData*);
}
