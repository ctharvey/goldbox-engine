// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "goldbox/exploration.hpp"
#include "goldbox/formats/graphics.hpp"
#include <optional>

namespace goldbox {
struct PortraitArchives { formats::DaxArchive heads, bodies; };
struct ImageArchives {
    formats::DaxArchive pictures, sprites;
    std::optional<PortraitArchives> portraits{};
    std::uint32_t picture_period_ms{}; // Explicit presentation policy; zero keeps pictures static.
};
// Presentation state only: never alters the decoded campaign or ECL memory.
// Picture timing is supplied explicitly; sprite frames remain distance views.
class ImagePresentation {
public:
    explicit ImagePresentation(const ImageArchives& archives) : archives_(archives) {}
    void picture(std::uint8_t id);
    void portrait(std::uint8_t head, std::uint8_t body, const formats::EgaPortraitLayout& layout);
    void sprite_start(EclSpriteRequest request, const ExplorationSession& session);
    void sprite_advance();
    bool advance_time(std::uint64_t elapsed_ms);
    std::uint32_t animation_wait_ms() const;
    void clear();
    const formats::EgaFrame* frame() const;
    formats::EgaPaletteKind palette() const { return sprite_ ? formats::EgaPaletteKind::sprite : formats::EgaPaletteKind::standard; }
private:
    const ImageArchives& archives_;
    std::vector<formats::EgaFrame> frames_;
    std::size_t index_{};
    bool sprite_{};
    std::uint64_t elapsed_ms_{};
};
} // namespace goldbox
