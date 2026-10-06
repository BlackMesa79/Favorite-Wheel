#pragma once
#include <cstdint>
#include <unordered_set>
namespace Wheel {
    // Tracks physical buttons across open/close. Wheel clicks must not become game
    // attacks on the next held/release event, and pre-existing movement gets one release.
    class InputGate {
    public:
        enum class Result { Pass, Release, Suppress };
        bool Swallowed(std::uint64_t id) const { return swallowed.contains(id); }
        Result Filter(std::uint64_t id, bool pressed, bool up, bool capture, bool impulse = false) {
            if (capture || swallowed.contains(id)) {
                const bool release = forwarded.erase(id) != 0;
                if (pressed && !impulse) swallowed.insert(id);
                if (up) swallowed.erase(id);
                return release ? Result::Release : Result::Suppress;
            }
            if (pressed && !impulse) forwarded.insert(id);
            if (up) forwarded.erase(id);
            return Result::Pass;
        }
        void Reset() { forwarded.clear(); swallowed.clear(); }
    private:
        std::unordered_set<std::uint64_t> swallowed;
        std::unordered_set<std::uint64_t> forwarded;
    };
}
