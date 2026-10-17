// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "goldbox/formats/dax.hpp"
#include <array>

namespace goldbox::formats {
struct IndexedImage {
    std::uint16_t width{}, height{};
    std::vector<std::uint8_t> pixels;
};
struct EgaImages {
    std::uint16_t x_offset{}, y_offset{};
    std::array<std::uint8_t,8> cga_mapping{};
    std::vector<IndexedImage> images;
};
// Strict 17-byte SSI EGA header and packed high/low pixel nibbles. No heuristic
// count adjustments or VGA/combat palette selection. Dimensions <=320x200.
EgaImages decode_ega_images(std::span<const std::uint8_t> record);
using Rgba = std::array<std::uint8_t,4>;
std::array<Rgba,16> ega_palette();

// Original views: far front/left/right, medium front/left/right,
// close front/left/right, far filler. Each definition is exactly 156 indices.
using WallViews = std::array<IndexedImage,10>;
std::vector<WallViews> decode_wall_definitions(std::span<const std::uint8_t> record,
    std::span<const IndexedImage> tiles);
struct WallGraphics {
    std::array<WallViews,15> walls;
    std::array<bool,15> present{};
};
// Explicit bank records populate GEO wall IDs 1..5,6..10,11..15. 255 means
// absent; 127 selects the next five walls from the previous definition record
// (first bank 127 selects record 0). Shared tiles come from record 203.
// Caller must establish the map/bank relationship; never inferred from GEO ID.
WallGraphics load_wall_graphics(const DaxArchive& definitions, const DaxArchive& tiles,
    const DaxArchive& shared, std::array<std::uint8_t,3> banks);
} // namespace goldbox::formats
