// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

namespace goldbox::formats {
// Native SSI template resource boundaries, independently observed. This does
// not decode combat rules or reinterpret serialized native pointers.
inline void validate_ssi_monster_resources(std::span<const std::uint8_t> character,
    std::span<const std::uint8_t> items, std::span<const std::uint8_t> effects) {
    if (character.size() != 285) throw std::runtime_error("Invalid SSI monster character record size");
    if (items.size()%63 != 0) throw std::runtime_error("Malformed SSI monster item records");
    if (effects.size()%9 != 0) throw std::runtime_error("Malformed SSI monster effect records");
}

// This adapter describes the observed 285/63/9-byte layout, not every SSI title.
// Unknown fields remain encoded. These local snapshots are neither save files
// nor a claim that inventory/effect rules or complete combat are implemented.
struct SsiMonsterStats {
    std::uint8_t hit_points{}, movement{};
    int armor_class{}, thac0{};
    bool operator==(const SsiMonsterStats&) const = default;
};
struct SsiMonsterItem {
    std::uint8_t type{}, quantity{};
    bool equipped{};
    std::uint16_t unit_weight{};
    std::uint16_t stack_weight() const {
        return static_cast<std::uint16_t>(std::uint32_t(unit_weight)*std::max(unsigned(quantity),1u));
    }
    bool operator==(const SsiMonsterItem&) const = default;
};
inline SsiMonsterItem decode_ssi_monster_item(std::span<const std::uint8_t> record) {
    if (record.size() != 63) throw std::runtime_error("Invalid SSI monster item record size");
    return {record[0x2e],record[0x39],record[0x34] != 0,
        static_cast<std::uint16_t>(record[0x37] | (unsigned(record[0x38]) << 8))};
}
struct SsiMonsterEffect {
    std::uint8_t type{};
    // Native timer word; interpretation of special effect payloads belongs to
    // their handler. Zero is skipped by the native timer-expiry traversal.
    std::uint16_t timer{};
    std::uint8_t parameter{};
    bool calls_handler_on_removal{};
    bool operator==(const SsiMonsterEffect&) const = default;
};
inline SsiMonsterEffect decode_ssi_monster_effect(std::span<const std::uint8_t> record) {
    if (record.size() != 9) throw std::runtime_error("Invalid SSI monster effect record size");
    return {record[0],static_cast<std::uint16_t>(record[1] | (unsigned(record[2]) << 8)),record[3],record[4] != 0};
}
struct SsiMonsterTemplate {
    std::array<std::uint8_t,285> character{};
    std::vector<std::array<std::uint8_t,63>> items;
    std::vector<std::array<std::uint8_t,9>> effects;
    SsiMonsterStats stats() const {
        return {character[0x11b],character[0x11c],60-int(character[0x111]),60-int(character[0x110])};
    }
    bool operator==(const SsiMonsterTemplate&) const = default;
};
// Per-creature input to the native encounter movement aggregator, not the full
// combat movement/effect handler. Presence of 27 takes precedence over 2A,
// including when its timer is zero; doubling wraps to one byte.
inline std::uint8_t ssi_encounter_movement(const SsiMonsterTemplate& source) {
    bool doubled = false, halved = false;
    for (const auto& record : source.effects) {
        const auto effect = decode_ssi_monster_effect(record);
        doubled |= effect.type == 0x27;
        halved |= effect.type == 0x2a;
    }
    const auto movement = source.stats().movement;
    return doubled ? static_cast<std::uint8_t>(movement*2) : (halved ? movement/2 : movement);
}
inline SsiMonsterTemplate decode_ssi_monster_resources(std::span<const std::uint8_t> character,
    std::span<const std::uint8_t> items, std::span<const std::uint8_t> effects) {
    validate_ssi_monster_resources(character,items,effects);
    SsiMonsterTemplate result;
    std::copy(character.begin(),character.end(),result.character.begin());
    // The native loader discards these serialized far pointers and constructs
    // its own lists. Vector order replaces the links; never follow file pointers.
    for (const auto offset : {0x7f,0xc8,0x104,0x108})
        std::fill_n(result.character.begin()+offset,4,0);
    for (std::size_t offset = 0; offset < items.size(); offset += 63) {
        auto& record = result.items.emplace_back();
        std::copy_n(items.begin()+offset,63,record.begin());
        std::fill_n(record.begin()+0x2a,4,0);
    }
    for (std::size_t offset = 0; offset < effects.size(); offset += 9) {
        auto& record = result.effects.emplace_back();
        std::copy_n(effects.begin()+offset,9,record.begin());
        std::fill_n(record.begin()+5,4,0);
    }
    return result;
}
// Five already-rolled d6 values belong to one template, not to each clone.
// The caller owns RNG selection/state. No game-specific RNG lives in formats.
inline std::vector<SsiMonsterTemplate> materialize_ssi_monster_group(SsiMonsterTemplate source,
    std::uint8_t count, std::uint8_t group, const std::array<std::uint8_t,5>& rolls) {
    if (!count || count > 63) throw std::runtime_error("Invalid SSI monster group count");
    for (const auto roll : rolls)
        if (roll < 1 || roll > 6) throw std::runtime_error("Invalid SSI monster initialization roll");
    auto& character = source.character;
    switch (character[0x2e]) {
        case 1: case 3: case 5: character[0xc0] = 1; break;
        case 2: case 4: case 6: case 7: character[0xc0] = 2; break;
        default: break;
    }
    const std::array<std::size_t,5> offsets{0xc1,0xc2,0xc3,0xc5,0xc6};
    for (std::size_t i = 0; i < offsets.size(); ++i) character[offsets[i]] = 128+17*rolls[i];
    character[0xc4] = 0xc4;
    character[0xbf] = group;
    return std::vector<SsiMonsterTemplate>(count,source);
}
} // namespace goldbox::formats
