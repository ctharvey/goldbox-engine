// SPDX-License-Identifier: GPL-3.0-or-later
#include "view.hpp"
#include <cmath>
#include <limits>
#include <stdexcept>

namespace goldbox {
formats::IndexedImage compose_wall_view(const ExplorationSession& session, const formats::WallGraphics& graphics,
    const GameProfile::ColorBackdrop* backdrop) {
    (void)session.map.cell(session.row,session.column);
    const auto direction = static_cast<unsigned>(session.facing);
    if (direction > 3) throw std::runtime_error("Invalid graphics view direction");
    formats::IndexedImage frame{88,88,std::vector<std::uint8_t>(88*88,0)};
    std::fill(frame.pixels.begin()+44*88,frame.pixels.end(),6);
    if (backdrop) {
        // Independently implemented from SSI COLOR-mode format observations;
        // research-only references are recorded in attribution/sources.json.
        constexpr std::uint8_t sky_colors[]{0,15,4,11,13,2,9,14,0,15,4,11,13,2,9,14};
        const auto indoor = session.map.cell(session.row,session.column).event_high_bit();
        const auto sky = session.state.memory[backdrop->sky_color_addresses[indoor ? 1 : 0]];
        if (sky >= std::size(sky_colors)) throw std::runtime_error("Unsupported backdrop sky color selector");
        std::fill(frame.pixels.begin(),frame.pixels.begin()+43*88,sky_colors[sky]);
        std::fill(frame.pixels.begin()+43*88,frame.pixels.begin()+44*88,15);
        std::fill(frame.pixels.begin()+44*88,frame.pixels.begin()+46*88,7);
        std::fill(frame.pixels.begin()+46*88,frame.pixels.end(),6);
    }
    constexpr int dx[]{0,1,0,-1}, dy[]{-1,0,1,0};
    const auto wall_at = [&](int forward, int lateral, unsigned side) -> unsigned {
        const int row = session.row + dy[direction]*forward + dx[direction]*lateral;
        const int col = session.column + dx[direction]*forward - dy[direction]*lateral;
        if (row < 0 || row >= 16 || col < 0 || col >= 16) return 0;
        const auto& cell = session.map.cell(row,col);
        const unsigned facing = (direction+side)%4;
        const std::uint8_t values[]{cell.north,cell.east,cell.south,cell.west};
        if (values[facing]) return values[facing];
        const int nr = row+dy[facing], nc = col+dx[facing];
        if (nr < 0 || nr >= 16 || nc < 0 || nc >= 16) return 0;
        const auto& next = session.map.cell(nr,nc);
        const std::uint8_t opposite[]{next.south,next.west,next.north,next.east};
        return opposite[facing];
    };
    const auto paint = [&](unsigned id, unsigned view, int x, int y) {
        if (!id) return;
        if (id > 15 || !graphics.present[id-1]) throw std::runtime_error("GEO wall has no selected graphics bank");
        const auto& image = graphics.walls[id-1][view];
        if (image.pixels.size() != std::size_t(image.width)*image.height)
            throw std::runtime_error("Malformed composed wall image");
        for (int row = 0; row < image.height; ++row) for (int col = 0; col < image.width; ++col) {
            const auto pixel = image.pixels[row*image.width+col];
            const int tx = x+col, ty = y+row;
            if (pixel > 15) throw std::runtime_error("Invalid wall pixel");
            if (pixel != 13 && tx >= 0 && tx < 88 && ty >= 0 && ty < 88) frame.pixels[ty*88+tx] = pixel;
        }
    };
    // Painter's order follows the three perspective planes. Dimensions are
    // format geometry, not ray-cast textures: near fronts are 56x64, then
    // 24x32 and 8x16. Side pieces bridge those planes and are already skewed.
    constexpr int spacing[]{56,24,16}, front_x[]{16,32,40}, front_y[]{8,24,32};
    constexpr int left_x[]{0,16,32}, right_x[]{72,56,48}, side_y[]{0,8,24};
    for (int depth = 2; depth >= 0; --depth) {
        const unsigned view = (2-depth)*3;
        for (int lateral = -depth-1; lateral <= depth+1; ++lateral) {
            const auto id = wall_at(depth,lateral,0);
            paint(id,view,front_x[depth]+lateral*spacing[depth],front_y[depth]);
            if (depth == 2 && wall_at(depth,lateral+1,0))
                paint(id,9,front_x[depth]+lateral*spacing[depth]+8,front_y[depth]);
        }
        for (int lateral = -depth; lateral <= 0; ++lateral)
            paint(wall_at(depth,lateral,3),view+1,left_x[depth]+lateral*spacing[depth],side_y[depth]);
        for (int lateral = 0; lateral <= depth; ++lateral)
            paint(wall_at(depth,lateral,1),view+2,right_x[depth]+lateral*spacing[depth],side_y[depth]);
    }
    return frame;
}

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
