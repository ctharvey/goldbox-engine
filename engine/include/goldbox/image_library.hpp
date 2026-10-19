// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <compare>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>

namespace goldbox {
// Adapters supply source identity; the library has no title-specific knowledge.
struct ImageKey {
    std::string game, archive, kind;
    std::uint16_t record{}, frame{};
    // A composed portrait's primary archive/record identifies its head.
    std::string body_archive{};
    std::optional<std::uint16_t> body_record{};
    auto operator<=>(const ImageKey&) const = default;
};
struct ImageReplacement { ImageKey source; std::filesystem::path file; std::string mod; };
// Archive basenames use ASCII case folding, independent of the host filesystem.
std::string image_archive_id(std::string name);
ImageKey normalize_image_key(ImageKey source);
class ImageLibrary {
public:
    void add(ImageReplacement replacement);
    const ImageReplacement* find(ImageKey source, bool enhanced) const;
private:
    std::map<ImageKey,ImageReplacement> entries_;
};
} // namespace goldbox
