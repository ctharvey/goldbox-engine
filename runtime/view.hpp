// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "goldbox/exploration.hpp"
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
int run_exploration_window(ExplorationSession& session, std::string_view source_identity,
    const std::filesystem::path& save);
} // namespace goldbox
