// SPDX-License-Identifier: GPL-3.0-or-later
#include "view.hpp"
#include <cmath>
#include <limits>
#include <stdexcept>

namespace goldbox {
WallHit cast_wall(const formats::GeoMap& map, int row, int column, double dx, double dy) {
    if (row < 0 || row >= 16 || column < 0 || column >= 16 ||
        !std::isfinite(dx) || !std::isfinite(dy) || std::hypot(dx, dy) < 0.000001) {
        throw std::runtime_error("Invalid view ray");
    }
    const double infinity = std::numeric_limits<double>::infinity();
    const double step_x = dx == 0 ? infinity : 1 / std::abs(dx);
    const double step_y = dy == 0 ? infinity : 1 / std::abs(dy);
    double next_x = step_x / 2, next_y = step_y / 2;
    // A ray exits a 16x16 grid within 32 edge crossings, including corner ties.
    for (int edge = 0; edge < 33; ++edge) {
        const bool x_edge = next_x < next_y;
        const double distance = x_edge ? next_x : next_y;
        const int nr = row + (x_edge ? 0 : (dy < 0 ? -1 : 1));
        const int nc = column + (x_edge ? (dx < 0 ? -1 : 1) : 0);
        const auto& cell = map.cell(row, column);
        auto wall = x_edge ? (dx < 0 ? cell.west : cell.east) : (dy < 0 ? cell.north : cell.south);
        if (nr < 0 || nr >= 16 || nc < 0 || nc >= 16) return {distance, wall, x_edge};
        const auto& neighbor = map.cell(nr, nc);
        const auto opposite = x_edge ? (dx < 0 ? neighbor.east : neighbor.west) :
            (dy < 0 ? neighbor.south : neighbor.north);
        if (wall || opposite) return {distance, wall ? wall : opposite, x_edge};
        row = nr;
        column = nc;
        if (x_edge) next_x += step_x;
        else next_y += step_y;
    }
    throw std::runtime_error("View ray exceeded map bounds");
}
} // namespace goldbox
