#pragma once
#include <SKSE/SKSE.h>
#include <cstddef>
#include <cstdint>

namespace Wheel::RuntimeSupport
{
    // Keep new releases opt-in; 1.7.99/104 are test targets awaiting gameplay reports.
    constexpr bool Supported(REL::Version version)
    {
        return version == REL::Version{1, 5, 97, 0} || version == REL::Version{1, 6, 1170, 0} ||
               version == REL::Version{1, 7, 99, 0} || version == REL::Version{1, 7, 104, 0};
    }
    constexpr std::uint64_t inputDispatchSE = 67315, inputDispatchAE = 68617;
    constexpr std::ptrdiff_t inputDispatchCall = 0x7B;
    constexpr std::size_t playerUpdateSlot = 0xAD;
} // namespace Wheel::RuntimeSupport
