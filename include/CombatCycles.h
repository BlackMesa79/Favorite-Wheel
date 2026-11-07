#pragma once
#include "Favorites.h"
namespace SKSE {class SerializationInterface;}
namespace Wheel::Cycles {
    constexpr std::uint32_t record=0x46574359;
    std::vector<Item> Entries(std::uint32_t group=0);
    std::string Names();
    std::string Binding(std::uint32_t group);
    void AnnotateMembership(std::vector<Item>& items);
    std::uint32_t Match(int key,int modifiers,bool insideWheel);
    bool Append(std::uint32_t group,std::uint32_t preset=0); // Zero captures current hands.
    bool Rename(std::uint32_t group,const std::string& name);
    bool Bind(std::uint32_t group,int key,int modifiers);
    bool Clear(std::uint32_t group);
    bool Remove(std::uint32_t group,std::uint32_t preset);
    bool Move(std::uint32_t group,std::uint32_t preset,int direction);
    void PresetRemoved(std::uint32_t preset);
    void Apply(std::uint32_t group,std::uint32_t preset=0);
    void Save(SKSE::SerializationInterface* api);
    void Load(SKSE::SerializationInterface* api,std::uint32_t version,std::uint32_t length);
    void Revert();
    void FinalizeLoad();
}
