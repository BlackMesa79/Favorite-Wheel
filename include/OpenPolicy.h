#pragma once
#include <cstdint>
#include <string_view>
namespace Wheel {
    // Race "playable" is a character-creation flag, not a test for beast form.
    // Vampire/custom human races may lack it and still use the normal favorites menu.
    inline bool PlayerEligible(bool active, bool present, bool has3D, bool dead, bool beast) {
        return active && present && has3D && !dead && !beast;
    }
    inline bool FavoritesKeyMatches(int overrideKey, std::uint32_t mappedKey, std::uint32_t scanCode, std::string_view userEvent) {
        if (overrideKey >= 0) return scanCode == static_cast<std::uint32_t>(overrideKey);
        return userEvent == "Favorites" || (mappedKey < 256 && scanCode == mappedKey);
    }
}
