// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "goldbox/exploration.hpp"
#include "goldbox/formats/graphics.hpp"
#include <optional>

namespace goldbox {
struct ImageArchives {
    formats::DaxArchive pictures, sprites;
};
// Presentation state only: never alters the decoded campaign or ECL memory.
// Single static picture frame; explicit sprite distances, no invented timers.
class ImagePresentation {
public:
    explicit ImagePresentation(const ImageArchives& archives) : archives_(archives) {}
    void picture(std::uint8_t id);
    void sprite_start(EclSpriteRequest request, const ExplorationSession& session);
    void sprite_advance();
    void clear();
    const formats::EgaFrame* frame() const;
    formats::EgaPaletteKind palette() const { return sprite_ ? formats::EgaPaletteKind::sprite : formats::EgaPaletteKind::standard; }
private:
    const ImageArchives& archives_;
    std::vector<formats::EgaFrame> frames_;
    std::size_t index_{};
    bool sprite_{};
};
} // namespace goldbox
