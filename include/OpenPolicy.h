#pragma once
#include <cstdint>
#include <string_view>
namespace Wheel {
    // Race "playable" is a character-creation flag, not a test for beast form.
    // Vampire/custom human races may lack it and still use the normal favorites menu.
    inline bool PlayerEligible(bool active, bool present, bool has3D, bool dead, bool beast) {
        return active && present && has3D && !dead && !beast;
    }
    inline int ResolveFavoritesKey(int overrideKey,std::uint32_t mappedKey) {
        return overrideKey>=0?overrideKey:(mappedKey<255?static_cast<int>(mappedKey):-1);
    }
    inline bool FavoritesKeyMatches(int overrideKey, std::uint32_t mappedKey, std::uint32_t scanCode, std::string_view userEvent) {
        // Favorites is also used by hotkey/control-map integrations. A semantic
        // event must not override a different physical binding or an unbound key.
        (void)userEvent;
        const int key=ResolveFavoritesKey(overrideKey,mappedKey);
        return key>=0 && scanCode==static_cast<std::uint32_t>(key);
    }
}
