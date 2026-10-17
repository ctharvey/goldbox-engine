// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "goldbox/exploration.hpp"
#include "goldbox/formats/graphics.hpp"
#include "goldbox/image_presentation.hpp"
#include "goldbox/game_profile.hpp"
#include <filesystem>

namespace goldbox {
struct WallHit {
    double distance{}; // Ray parameter; unnormalized camera rays avoid fisheye distortion.
    std::uint8_t wall{};
    bool east_west{};
};
// Cell-center ray through GEO edges. Map bounds and either side of a wall block,
// exactly as in ExplorationSession::move. Throws on invalid origin/direction.
WallHit cast_wall(const formats::GeoMap& map, int row, int column, double dx, double dy);
// SSI's 64 printable glyphs: @.._ map to 0..31, space..? to 32..63.
// Lowercase UI text folds to uppercase; unsupported bytes use '?'.
std::uint8_t font_glyph(unsigned char ascii);
formats::IndexedImage compose_font_atlas(std::span<const formats::IndexedImage> tiles);
// Native 88x88 perspective view using preprojected wall pieces. Palette index
// 13 is transparent in wall artwork. Without color configuration, the backdrop
// remains a placeholder. Color rendering reads state but never changes it.
formats::IndexedImage compose_wall_view(const ExplorationSession& session, const formats::WallGraphics& graphics,
    const GameProfile::ColorBackdrop* backdrop = nullptr);
int run_exploration_window(ExplorationSession& session, std::string_view source_identity,
    const std::filesystem::path& save, const formats::WallGraphics* graphics = nullptr,
    const ImageArchives* images = nullptr, const GameProfile::ColorBackdrop* backdrop = nullptr,
    std::span<const formats::IndexedImage> font = {}, const GameProfile::Portrait* portrait = nullptr);
} // namespace goldbox
