#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace Wheel {
// DirectInput codes. Modifier bits: Shift=1, Ctrl=2, Alt=4.
inline int ModifierBit(std::uint32_t code) {
  switch (code) {
  case 42:
  case 54:
    return 1;
  case 29:
  case 157:
    return 2;
  case 56:
  case 184:
    return 4;
  default:
    return 0;
  }
}
enum class Opening { None, Favorites, Actions };
inline Opening KeyboardOpening(std::uint32_t code, int held, int favoriteKey,
                               int favoriteModifier, int actionKey,
                               int actionModifier) {
  // Exact chords avoid Shift+Q accidentally opening the unmodified Q wheel.
  // If both bindings coincide, actions take priority deterministically.
  if (actionKey >= 0 && code == static_cast<std::uint32_t>(actionKey) &&
      held == actionModifier)
    return Opening::Actions;
  if (favoriteKey >= 0 && code == static_cast<std::uint32_t>(favoriteKey) &&
      held == favoriteModifier)
    return Opening::Favorites;
  return Opening::None;
}
inline Opening GamepadOpening(std::uint32_t code, int favoriteKey,
                              bool favoriteModifier, bool actionModifier) {
  if (favoriteKey < 266 || favoriteKey > 281 ||
      code != static_cast<std::uint32_t>(favoriteKey))
    return Opening::None;
  if (actionModifier)
    return Opening::Actions;
  return favoriteModifier ? Opening::Favorites : Opening::None;
}
inline float StickAxis(float value) {
  constexpr float deadzone = .2f;
  return std::abs(value) <= deadzone
             ? 0.f
             : std::copysign((std::min(std::abs(value), 1.f) - deadzone) /
                                 (1.f - deadzone),
                             value);
}
inline void AimStick(float &x, float &y, float sx, float sy) {
  const float length = std::hypot(sx, sy);
  if (length <= .2f)
    return; // Keep the selected entry when releasing the stick to press
            // confirm.
  x = sx / length;
  y = -sy / length;
}
} // namespace Wheel
