// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "goldbox/exploration.hpp"
#include "goldbox/formats/graphics.hpp"
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
// Native 88x88 perspective view using preprojected wall pieces. Palette index
// 13 is transparent in wall artwork. Backdrop is a placeholder, not imported.
formats::IndexedImage compose_wall_view(const ExplorationSession& session, const formats::WallGraphics& graphics);
int run_exploration_window(ExplorationSession& session, std::string_view source_identity,
    const std::filesystem::path& save, const formats::WallGraphics* graphics = nullptr);
} // namespace goldbox
