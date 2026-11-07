#pragma once
#include "Favorites.h"
#include "OutfitModel.h"
#include <string>
namespace Wheel::Outfits {
    bool Install();
    std::string Names();
    std::vector<Item> Entries();
    bool Capture(const std::string& name,std::uint32_t replace=0);
    bool Rename(std::uint32_t id,const std::string& name);
    bool Remove(std::uint32_t id);
    bool Export(std::uint32_t id);
    bool Import();
    bool Busy();
    void CancelJob();
    void Tick(bool valid);
    void Apply(std::uint32_t id);
    bool CyclePart(std::uint32_t id,Part part);
    bool SetPartsPreset(std::uint32_t id,bool overlay);
    bool TogglePieceOptional(std::uint32_t id,std::size_t index);
    bool RemovePiece(std::uint32_t id,std::size_t index);
    bool AddWorn(std::uint32_t id);
    bool ToggleLoose(std::uint32_t id);
    std::vector<Piece> PieceEntries(std::uint32_t id);
    std::vector<Item> PieceItems(std::uint32_t id);
    const char* PartLabel(Part part);
    const char* PartModeLabel(PartMode mode);
    std::string ModeLabel(std::uint32_t id);
    std::string PartStateLabel(std::uint32_t id,Part part);
    bool IsLoose(std::uint32_t id);
    bool ToggleMode(std::uint32_t id);
}
