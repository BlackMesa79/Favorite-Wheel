#pragma once
#include <cstdint>
namespace Wheel {
    constexpr bool FreshComposition(std::uint64_t tick,std::uint64_t now) {
        return tick && now>=tick && now-tick<500;
    }
    constexpr bool AcceptIMECharacter(bool composing,std::uint32_t code) {
        return !composing || code>=128;
    }
}
