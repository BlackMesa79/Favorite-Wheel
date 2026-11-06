#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <array>
#include <atomic>
namespace Wheel {
// T cycles sorting; Shift+T toggles manual sorting. Other modifiers are not owned.
inline int SortingShortcut(std::uint32_t scan,int modifiers) {
  return scan==20 && (modifiers==0 || modifiers==1)?modifiers:-1;
}

// Physical controller state survives sparse input polls. Held duration and the
// device button map are not reliable substitutes for press/release events.
class ControllerChordState {
  std::atomic<std::uint16_t> held{0};
  std::atomic<const void*> device{nullptr};
public:
  void Attach(const void* current) {
    if(device.exchange(current)!=current)Reset();
  }
  void Observe(std::uint32_t code,bool pressed) {
    if(code<266 || code>281)return;
    const auto bit=static_cast<std::uint16_t>(1u<<(code-266));
    if(pressed)held.fetch_or(bit);else held.fetch_and(static_cast<std::uint16_t>(~bit));
  }
  bool Held(int code) const {return code>=266 && code<=281 && (held.load()&(1u<<(code-266)));}
  std::uint16_t Mask() const {return held.load();}
  void Reset(){held.store(0);}
};
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
struct WheelOpeningMemory {
  bool functions=false;
  bool Resolve(bool explicitActions,bool remember,bool human) const {
    return human && (explicitActions || (remember && functions));
  }
  void Observe(bool selectedFunctions,bool human) {if(human)functions=selectedFunctions;}
  void Reset(){functions=false;}
};
enum class PadWheelCommand { None, PreviousCategory, NextCategory, UseRight, UseLeft, Back, PreviousPage, NextPage };
inline PadWheelCommand ControllerCommand(std::uint32_t code,int categoryButtons) {
  if(code==266)return PadWheelCommand::PreviousPage;
  if(code==267)return PadWheelCommand::NextPage;
  if(code==268)return PadWheelCommand::PreviousCategory;
  if(code==269)return PadWheelCommand::NextCategory;
  if(code==276)return PadWheelCommand::UseRight;
  if(code==278)return PadWheelCommand::UseLeft;
  if(code==277)return PadWheelCommand::Back;
  const bool triggers=categoryButtons==1;
  if(code==(triggers?280u:274u))return PadWheelCommand::PreviousCategory;
  if(code==(triggers?281u:275u))return PadWheelCommand::NextCategory;
  if(code==(triggers?274u:280u))return PadWheelCommand::UseLeft;
  if(code==(triggers?275u:281u))return PadWheelCommand::UseRight;
  return PadWheelCommand::None;
}
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
inline std::uint16_t ControllerKeyBit(int code) {
  return code>=266 && code<=281 ? static_cast<std::uint16_t>(1u<<(code-266)) : 0;
}
struct ControllerSample {
  bool available=false;
  std::uint16_t held=0;
};
// Read from the game's current XInput packet, never from duration tables or a
// previous-frame grace period. Reserved XInput bits are not controller keys.
inline std::uint16_t ControllerPacketButtons(std::uint16_t buttons,std::uint8_t leftTrigger,std::uint8_t rightTrigger) {
  return static_cast<std::uint16_t>((buttons & 0x3FFu) | ((buttons & 0xF000u)>>2) |
      (leftTrigger>30?ControllerKeyBit(280):0) | (rightTrigger>30?ControllerKeyBit(281):0));
}
inline std::uint16_t ControllerOpeningHeld(std::uint16_t events,ControllerSample sample,int mainKey,std::uint16_t modifiers) {
  const auto mainBit=ControllerKeyBit(mainKey);
  // Native chord processing may release LB in the very packet that presses Y,
  // then resume LB with its original held duration. Repair only required
  // modifiers, only while that same main key is down in the current packet.
  // The persistent event tracker stays untouched (including real releases).
  if(sample.available && mainBit && (sample.held & mainBit))
    events|=static_cast<std::uint16_t>(sample.held & modifiers & ~mainBit);
  return events;
}
// ControlMap stores the primary button and its XInput modifier separately.
// 0/FFFF mean no modifier; 9/A are the game's trigger IDs, not bit masks.
inline std::uint16_t ControllerModifierMask(std::uint32_t raw) {
  if(raw==0 || raw==0xFFFF || raw==0xFF)return 0;
  if(raw==9)return ControllerKeyBit(280);
  if(raw==10)return ControllerKeyBit(281);
  if(raw & ~0xF3FFu)return 0xFFFF; // fail closed for an unknown modifier encoding
  std::uint16_t result=static_cast<std::uint16_t>(raw & 0x3FFu);
  return static_cast<std::uint16_t>(result | ((raw & 0xF000u)>>2));
}
inline bool ForeignGamepadChord(std::uint16_t held,int mainKey,std::uint16_t nativeRequired,
    bool followNative,int favoriteModifier,int actionModifier) {
  const auto allowed=ControllerKeyBit(mainKey) | (followNative?nativeRequired:0) |
      ControllerKeyBit(favoriteModifier) | ControllerKeyBit(actionModifier);
  return (held & ~allowed)!=0;
}
inline Opening ResolvedGamepadOpening(std::uint32_t code,int mainKey,
    std::uint16_t held,std::uint16_t nativeRequired,bool followNative,
    int favoriteModifier,int actionModifier) {
  if(mainKey<266 || mainKey>281 || code!=static_cast<std::uint32_t>(mainKey))return Opening::None;
  const auto mainBit=ControllerKeyBit(mainKey);
  if(ForeignGamepadChord(held,mainKey,nativeRequired,followNative,favoriteModifier,actionModifier))return Opening::None;
  const auto base=followNative?nativeRequired:std::uint16_t{0};
  if((base & mainBit) || (held & base)!=base)return Opening::None;
  const auto favoriteRequired=favoriteModifier<0?base:ControllerKeyBit(favoriteModifier);
  const bool favorite=favoriteModifier<0 || (favoriteRequired && !(favoriteRequired & mainBit) && (held & favoriteRequired)==favoriteRequired);
  const auto actionBit=ControllerKeyBit(actionModifier);
  const bool action=actionModifier<0 || (actionBit && !(actionBit & mainBit) && (held & actionBit));
  // An inherited LB+Y Favorites mapping must not turn into Actions just because
  // LB is also the default Actions modifier. A distinct extra modifier still works.
  if(favorite && favoriteRequired && actionBit && (favoriteRequired & actionBit))return Opening::Favorites;
  return action?Opening::Actions:favorite?Opening::Favorites:Opening::None;
}
// D-pad navigation follows physical edges, independent of HeldDuration().
// Observe outside the wheel too, so the held opening key cannot also page.
class ControllerPageButtons {
  std::array<bool, 2> held{}, blocked{};
  std::uint32_t first;
public:
  explicit ControllerPageButtons(std::uint32_t firstCode=266):first(firstCode){}
  bool Observe(std::uint32_t code, bool pressed, bool nativeDown) {
    if (code < first || code > first+1) return false;
    const auto at = code - first;
    if (!pressed || nativeDown) blocked[at] = false;
    const bool down = pressed && !held[at] && !blocked[at];
    held[at] = pressed;
    return down;
  }
  // A held key after focus/load/device reset needs release or a new native down.
  void Reset() { held.fill(false); blocked.fill(true); }
};
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
