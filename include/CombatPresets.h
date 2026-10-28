#pragma once
#include "Favorites.h"
#include <functional>
namespace SKSE {class SerializationInterface;}
namespace Wheel::Combat {
    constexpr std::uint32_t record=0x46574350;
    constexpr std::uint32_t bindingRecord=0x4657434b;
    std::vector<Item> Entries();
    std::string Names(); // Includes saved hand labels for directory-wide font prewarming.
    bool Capture(const std::string& name,std::uint32_t replace=0);
    std::uint32_t CaptureForCycle();
    bool Exists(std::uint32_t id);
    std::uint32_t FirstAvailable(const std::vector<std::uint32_t>& ids);
    bool Rename(std::uint32_t id,const std::string& name);
    bool Remove(std::uint32_t id);
    bool BindQuickSlot(std::uint32_t id,int slot,int* assignment=nullptr);
    void ClearQuickSlot(int slot,std::uint32_t expectedId=0);
    std::uint32_t PresetForQuickSlot(int slot);
    void Save(SKSE::SerializationInterface* api);
    void Load(SKSE::SerializationInterface* api,std::uint32_t version,std::uint32_t length);
    void LoadBindings(SKSE::SerializationInterface* api,std::uint32_t version,std::uint32_t length);
    void Revert();
    void Apply(std::uint32_t id,std::function<void(bool)> complete={});
    void Tick(bool valid);
    bool Busy();
    void CancelJob();
}
