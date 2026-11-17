#pragma once
#include "OutfitModel.h"
namespace Wheel::Outfits {
    // Freeze this run's available targets; scripts may recreate excluded items later.
    template<class Candidate> Preset ExecutionPreset(const Preset& saved,const std::vector<Candidate>& inventory,
        const std::vector<int>& matches) {
        if(saved.missingPolicy==MissingPolicy::Required)return saved;
        auto selected=saved;selected.pieces.clear();
        for(int i:matches)selected.pieces.push_back(inventory[i].piece);
        return selected;
    }
    template<class Candidate> bool RemovalComplete(const Preset& p,const std::vector<Candidate>& inventory) {
        if(p.missingPolicy==MissingPolicy::Required)
            return std::none_of(inventory.begin(),inventory.end(),[](const auto& c){return c.worn;});
        for(const auto& piece:p.pieces) {
            if(MissingFromInventory(piece,inventory))continue;
            const int match=Match(piece,inventory);
            if(match<0 || inventory[match].worn)return false;
        }
        return true;
    }
}
