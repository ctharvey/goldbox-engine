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
ImageKey normalize_image_key(ImageKey source) {
    if (source.game.empty() ||
        (source.kind != "picture" && source.kind != "sprite" && source.kind != "portrait") ||
        source.record > 254 || source.frame > 255)
        throw std::runtime_error("Invalid replacement image identity");
    source.archive = image_archive_id(source.archive);
    if (source.kind == "portrait") {
        if (!source.body_record || *source.body_record > 254 || source.frame != 0)
            throw std::runtime_error("Portrait requires a body record and frame zero");
        source.body_archive = image_archive_id(source.body_archive);
    } else if (!source.body_archive.empty() || source.body_record) {
        throw std::runtime_error("Body identity is only supported for portraits");
    }
    return source;
}
void ImageLibrary::add(ImageReplacement replacement) {
    replacement.source = normalize_image_key(std::move(replacement.source));
    const auto key = replacement.source;
    entries_.insert_or_assign(key,std::move(replacement));
}
const ImageReplacement* ImageLibrary::find(ImageKey source, bool enhanced) const {
    if (!enhanced || source.archive.empty()) return nullptr;
    source = normalize_image_key(std::move(source));
    const auto found = entries_.find(source);
    return found == entries_.end() ? nullptr : &found->second;
}
} // namespace goldbox
