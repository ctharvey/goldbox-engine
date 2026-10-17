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
struct EgaPortraitLayout {
    std::uint16_t width{}, height{};
    std::array<std::uint16_t,2> head_offset{}, body_offset{};
};
// Explicit SSI composition adapter, separate from decoding and rendering.
// Single-frame parts; if either header has an offset, use both header offsets.
// Otherwise use the caller's layout offsets. Opaque layers, head then body.
// Invalid sizes/pixels or parts outside the supplied canvas fail, never clip.
IndexedImage compose_ega_portrait(const EgaImages& head, const EgaImages& body,
    const EgaPortraitLayout& layout);
using Rgba = std::array<std::uint8_t,4>;
enum class EgaPaletteKind { standard, sprite, combat };
std::array<Rgba,16> ega_palette(EgaPaletteKind kind = EgaPaletteKind::standard);

// Explicit formats: no filename/length-based fallback between unrelated codecs.
std::vector<IndexedImage> decode_mono8_tiles(std::span<const std::uint8_t> record);
enum class EgaFrameEncoding { independent, xor_first };
struct EgaFrame {
    std::uint32_t delay{}; // Raw engine timing value; no milliseconds claim.
    std::uint16_t x_offset{}, y_offset{};
    std::uint8_t header_byte{};
    std::array<std::uint8_t,8> cga_mapping{};
    IndexedImage image;
};
// 1-byte count, then a 21-byte header and packed pixels per frame. PIC-style
// dependent frames XOR against the original first frame, not the previous one.
std::vector<EgaFrame> decode_ega_sequence(std::span<const std::uint8_t> record, EgaFrameEncoding encoding);

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
