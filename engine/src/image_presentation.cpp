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
    elapsed_ms_ = 0;
    replacement_.clear();
    record_ = id; replacement_record_.reset();
}
void ImagePresentation::sprite_start(EclSpriteRequest request, const ExplorationSession& session,
    std::optional<formats::EgaFrame> replacement_portrait) {
    if (request.replacement_picture != 255 && !archives_.sprite_replacement_ms)
        throw std::runtime_error("Timed sprite replacement policy unavailable");
    if (request.sprite == 255) {
        if (request.replacement_picture != 255) throw std::runtime_error("Replacement without sprite unsupported");
        clear(); return;
    }
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
    std::vector<formats::EgaFrame> replacement;
    if (request.replacement_picture != 255) {
        if (replacement_portrait) replacement.push_back(std::move(*replacement_portrait));
        else replacement = formats::decode_ega_sequence(archives_.pictures.extract(request.replacement_picture),
            formats::EgaFrameEncoding::xor_first);
        validate(replacement);
    }
    frames_ = std::move(decoded);
    replacement_ = std::move(replacement);
    index_ = distance;
    sprite_ = true;
    elapsed_ms_ = 0;
    record_ = request.sprite;
    replacement_record_ = replacement_portrait ? std::nullopt :
        std::optional<std::uint8_t>(request.replacement_picture);
}
void ImagePresentation::portrait(std::uint8_t head, std::uint8_t body, const formats::EgaPortraitLayout& layout) {
    if (!archives_.portraits) throw std::runtime_error("No portrait archives selected");
    auto image = formats::compose_ega_portrait(
        formats::decode_ega_images(archives_.portraits->heads.extract(head)),
        formats::decode_ega_images(archives_.portraits->bodies.extract(body)),layout);
    formats::EgaFrame frame;
    frame.image = std::move(image);
    std::vector<formats::EgaFrame> decoded{std::move(frame)};
    validate(decoded);
    frames_ = std::move(decoded); index_ = 0; sprite_ = false;
    elapsed_ms_ = 0;
    replacement_.clear();
    record_.reset(); replacement_record_.reset();
}
void ImagePresentation::sprite_advance() {
    if (sprite_ && index_) { --index_; elapsed_ms_ = 0; }
}
std::uint32_t ImagePresentation::animation_wait_ms() const {
    if (sprite_ && !index_ && !replacement_.empty())
        return archives_.sprite_replacement_ms - static_cast<std::uint32_t>(elapsed_ms_);
    if (sprite_ || frames_.size() < 2 || !archives_.picture_period_ms) return 0;
    return archives_.picture_period_ms - static_cast<std::uint32_t>(elapsed_ms_);
}
bool ImagePresentation::advance_time(std::uint64_t elapsed_ms) {
    if (!animation_wait_ms()) return false;
    bool replaced = false;
    if (sprite_) {
        const auto wait = animation_wait_ms();
        if (elapsed_ms < wait) { elapsed_ms_ += elapsed_ms; return false; }
        elapsed_ms -= wait;
        frames_ = std::move(replacement_); replacement_.clear();
        sprite_ = false; index_ = 0; elapsed_ms_ = 0; replaced = true;
        record_ = replacement_record_; replacement_record_.reset();
        if (!animation_wait_ms()) return true;
    }
    const auto period = archives_.picture_period_ms;
    // Reduce before adding: even an arbitrarily large elapsed interval cannot overflow.
    const auto cycle = std::uint64_t(period) * frames_.size();
    const auto total = elapsed_ms_ + elapsed_ms % cycle;
    const auto previous = index_;
    index_ = (index_ + total / period) % frames_.size();
    elapsed_ms_ = total % period;
    return replaced || index_ != previous;
}
void ImagePresentation::clear() {
    frames_.clear(); replacement_.clear(); index_ = 0; sprite_ = false; elapsed_ms_ = 0;
    record_.reset(); replacement_record_.reset();
}
const formats::EgaFrame* ImagePresentation::frame() const { return frames_.empty() ? nullptr : &frames_[index_]; }
std::optional<ImageKey> ImagePresentation::source() const {
    const auto& archive = sprite_ ? archives_.sprite_archive : archives_.picture_archive;
    if (!record_ || frames_.empty() || archive.empty() || archives_.game_id.empty()) return std::nullopt;
    return ImageKey{archives_.game_id,image_archive_id(archive),sprite_ ? "sprite" : "picture",
        *record_,static_cast<std::uint16_t>(index_)};
}
const ImageReplacement* ImagePresentation::replacement(bool enhanced) const {
    const auto key = source();
    return key && archives_.library ? archives_.library->find(*key,enhanced) : nullptr;
}
} // namespace goldbox
