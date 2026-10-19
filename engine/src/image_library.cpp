// SPDX-License-Identifier: GPL-3.0-or-later
#include "goldbox/image_library.hpp"
#include <stdexcept>

namespace goldbox {
std::string image_archive_id(std::string name) {
    if (name.empty() || name == "." || name == "..") throw std::runtime_error("Invalid image archive identity");
    for (auto& c : name) {
        if (c == '/' || c == '\\' || c == ':' || static_cast<unsigned char>(c) < 32)
            throw std::runtime_error("Image archive identity must be a basename");
        if (c >= 'a' && c <= 'z') c -= 'a'-'A';
    }
    return name;
}
void ImageLibrary::add(ImageReplacement replacement) {
    if (replacement.source.game.empty() ||
        (replacement.source.kind != "picture" && replacement.source.kind != "sprite") ||
        replacement.source.record > 254 || replacement.source.frame > 255)
        throw std::runtime_error("Invalid replacement image identity");
    replacement.source.archive = image_archive_id(replacement.source.archive);
    const auto key = replacement.source;
    entries_.insert_or_assign(key,std::move(replacement));
}
const ImageReplacement* ImageLibrary::find(ImageKey source, bool enhanced) const {
    if (!enhanced || source.archive.empty()) return nullptr;
    source.archive = image_archive_id(source.archive);
    const auto found = entries_.find(source);
    return found == entries_.end() ? nullptr : &found->second;
}
} // namespace goldbox
