// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <span>
#include <stdexcept>

namespace goldbox::formats {
// Native SSI template resource boundaries, independently observed. This does
// not decode combat rules or reinterpret serialized native pointers.
inline void validate_ssi_monster_resources(std::span<const std::uint8_t> character,
    std::span<const std::uint8_t> items, std::span<const std::uint8_t> effects) {
    if (character.size() != 285) throw std::runtime_error("Invalid SSI monster character record size");
    if (items.size()%63 != 0) throw std::runtime_error("Malformed SSI monster item records");
    if (effects.size()%9 != 0) throw std::runtime_error("Malformed SSI monster effect records");
}
} // namespace goldbox::formats
