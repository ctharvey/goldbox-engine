// SPDX-License-Identifier: GPL-3.0-or-later
// Independent host state; SSI Engine research references in attribution/sources.json.
#include "goldbox/image_presentation.hpp"
#include <stdexcept>

namespace goldbox {
namespace {
void validate(const std::vector<formats::EgaFrame>& frames) {
    for (const auto& frame : frames) {
        if (frame.image.width+frame.x_offset > 88 || frame.image.height+frame.y_offset > 88)
            throw std::runtime_error("Image exceeds supported 88x88 view");
    }
}
}
void ImagePresentation::picture(std::uint8_t id) {
    if (id == 255) { clear(); return; }
    auto decoded = formats::decode_ega_sequence(archives_.pictures.extract(id),formats::EgaFrameEncoding::xor_first);
    validate(decoded);
    frames_ = std::move(decoded);
    index_ = 0;
    sprite_ = false;
}
void ImagePresentation::sprite_start(EclSpriteRequest request, const ExplorationSession& session) {
    // Upstream schedules a timed replacement at nearest distance. Until timing
    // is verified, reject it instead of silently substituting a different flow.
    if (request.replacement_picture != 255) throw std::runtime_error("Timed sprite replacement unsupported");
    if (request.sprite == 255) { clear(); return; }
    if (request.distance > 2) throw std::runtime_error("Invalid sprite distance");
    (void)session.map.cell(session.row,session.column);
    const auto direction = static_cast<unsigned>(session.facing);
    if (direction > 3) throw std::runtime_error("Invalid sprite facing");
    int distance = request.distance;
    int row = session.row, column = session.column;
    constexpr int dr[]{-1,0,1,0}, dc[]{0,1,0,-1};
    // Clamp sprites to the nearest blocking plane, checking both sides of an
    // edge like exploration movement. Never change the session to probe walls.
    for (int plane = 0; plane < distance; ++plane) {
        const auto& cell = session.map.cell(row,column);
        const std::uint8_t walls[]{cell.north,cell.east,cell.south,cell.west};
        const int nr = row+dr[direction], nc = column+dc[direction];
        bool blocked = walls[direction] || nr < 0 || nr >= 16 || nc < 0 || nc >= 16;
        if (!blocked) {
            const auto& neighbor = session.map.cell(nr,nc);
            const std::uint8_t opposite[]{neighbor.south,neighbor.west,neighbor.north,neighbor.east};
            blocked = opposite[direction] != 0;
        }
        if (blocked) { distance = plane; break; }
        row = nr; column = nc;
    }
    auto decoded = formats::decode_ega_sequence(archives_.sprites.extract(request.sprite),formats::EgaFrameEncoding::independent);
    validate(decoded);
    if (std::size_t(distance) >= decoded.size()) throw std::runtime_error("Sprite distance frame missing");
    frames_ = std::move(decoded);
    index_ = distance;
    sprite_ = true;
}
void ImagePresentation::sprite_advance() {
    if (sprite_ && index_) --index_;
}
void ImagePresentation::clear() { frames_.clear(); index_ = 0; sprite_ = false; }
const formats::EgaFrame* ImagePresentation::frame() const { return frames_.empty() ? nullptr : &frames_[index_]; }
} // namespace goldbox
