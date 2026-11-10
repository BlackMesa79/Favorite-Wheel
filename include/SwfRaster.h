#pragma once
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Wheel::Swf {
    enum class Variant : std::uint8_t { Original, Mask };
    // Straight (non-premultiplied) RGBA, matching IconLibrary's WIC 32bppRGBA upload.
    struct Bitmap { std::uint32_t width=0, height=0; std::vector<std::uint8_t> rgba; };
    struct Result { std::optional<Bitmap> bitmap; std::vector<std::string> warnings; };
    struct Movie; // Immutable after Parse; shared ownership is safe across threads.
    std::shared_ptr<const Movie> Parse(std::span<const std::uint8_t> file, std::vector<std::string>& warnings);
    // No shared mutable render state. Empty/missing/unsupported frames return no bitmap.
    Result Render(const Movie&, std::string_view label, Variant, std::uint32_t edge);
}
