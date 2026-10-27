// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

namespace goldbox::formats {
inline int ssi_signed_byte(std::uint8_t value) { return value < 128 ? int(value) : int(value)-256; }
struct SsiDamageDice {
    std::uint8_t count{}, sides{};
    int bonus{};
    bool operator==(const SsiDamageDice&) const = default;
};
struct SsiBaseItem {
    std::uint8_t slot{}, hands{};
    SsiDamageDice large, small_medium;
    std::uint8_t rate{}, armor_descriptor{}, weapon_type{}, unknown8{}, range{}, class_mask{}, weapon_flags{}, unknown15{};
    bool operator==(const SsiBaseItem&) const = default;
};
inline SsiBaseItem decode_ssi_base_item(std::span<const std::uint8_t> record) {
    if (record.size() != 16) throw std::runtime_error("Invalid SSI base item record size");
    return {record[0],record[1],{record[2],record[3],ssi_signed_byte(record[4])},
        {record[9],record[10],ssi_signed_byte(record[11])},record[5],record[6],record[7],record[8],
        record[12],record[13],record[14],record[15]};
}
// Explicit two-byte-prefix layout. The header is not a trusted record count.
// Other title/file layouts must select their own adapter.
inline std::vector<SsiBaseItem> decode_ssi_base_items(std::span<const std::uint8_t> bytes) {
    if (bytes.size() < 18 || (bytes.size()-2)%16 || (bytes.size()-2)/16 > 256)
        throw std::runtime_error("Invalid SSI base item table size");
    std::vector<SsiBaseItem> result;
    for (std::size_t offset = 2; offset < bytes.size(); offset += 16)
        result.push_back(decode_ssi_base_item(bytes.subspan(offset,16)));
    return result;
}
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
    int magical_bonus{};
    std::uint16_t stack_weight() const {
        return static_cast<std::uint16_t>(std::uint32_t(unit_weight)*std::max(unsigned(quantity),1u));
    }
    bool operator==(const SsiMonsterItem&) const = default;
};
inline SsiMonsterItem decode_ssi_monster_item(std::span<const std::uint8_t> record) {
    if (record.size() != 63) throw std::runtime_error("Invalid SSI monster item record size");
    return {record[0x2e],record[0x39],record[0x34] != 0,
        static_cast<std::uint16_t>(record[0x37] | (unsigned(record[0x38]) << 8)),ssi_signed_byte(record[0x32])};
}
struct SsiWeaponContext {
    std::uint8_t base_thac0_code{}, race_code{};
    // Explicit resolved ability inputs; missing required inputs fail. This
    // adapter does not guess modifiers from unverified character/effect state.
    std::optional<int> dexterity_to_hit, strength_to_hit, strength_to_damage;
    std::optional<int> arrow_bonus, bolt_bonus; // Absent means no equipped ammunition.
};
struct SsiWeaponStats {
    std::uint8_t thac0_code{}; // Displayed THAC0 is 60 minus this unsigned byte.
    SsiDamageDice small_medium;
    bool operator==(const SsiWeaponStats&) const = default;
};
inline SsiWeaponStats ssi_weapon_stats(std::span<const SsiBaseItem> definitions,
    const SsiMonsterItem& weapon, const SsiWeaponContext& context) {
    if (weapon.type >= definitions.size()) throw std::runtime_error("SSI base item reference out of range");
    const auto checked = [](int value) {
        if (value < -128 || value > 127) throw std::runtime_error("SSI weapon modifier out of byte range");
        return value;
    };
    const auto required = [&](const std::optional<int>& value) {
        if (!value) throw std::runtime_error("SSI weapon ability modifier unavailable");
        return checked(*value);
    };
    const auto wrap = [](int value) { return ssi_signed_byte(static_cast<std::uint8_t>(value)); };
    const auto& definition = definitions[weapon.type];
    int accuracy = context.base_thac0_code;
    int damage = checked(definition.small_medium.bonus);
    if (definition.weapon_flags & 2) accuracy += required(context.dexterity_to_hit);
    if (definition.weapon_flags & 4) {
        accuracy += required(context.strength_to_hit);
        damage += required(context.strength_to_damage);
    }
    int bonus = checked(weapon.magical_bonus);
    if ((definition.weapon_flags & 128) && context.bolt_bonus) bonus += checked(*context.bolt_bonus);
    if ((definition.weapon_flags & 1) && context.arrow_bonus) bonus += checked(*context.arrow_bonus);
    bonus = wrap(bonus);
    damage = wrap(damage+bonus);
    if (context.race_code == 2 && (weapon.type == 36 || weapon.type == 37 || (weapon.type > 40 && weapon.type < 45)))
        bonus = wrap(bonus+1); // Native racial bonus affects accuracy after damage is assigned.
    return {static_cast<std::uint8_t>(accuracy+bonus),
        {definition.small_medium.count,definition.small_medium.sides,damage}};
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
inline int ssi_dexterity_accuracy(std::uint8_t dexterity) {
    if (dexterity <= 2) return -4;
    if (dexterity <= 5) return int(dexterity)-6;
    if (dexterity >= 16 && dexterity <= 18) return int(dexterity)-15;
    if (dexterity >= 19 && dexterity <= 20) return 3;
    if (dexterity >= 21 && dexterity <= 23) return 4;
    if (dexterity >= 24 && dexterity <= 25) return 5;
    return 0;
}
inline std::array<int,2> ssi_strength_weapon_bonuses(std::uint8_t strength,
    std::uint8_t exceptional, bool enabled) {
    if (!enabled) return {0,0};
    if (strength > 25 || (strength == 18 && exceptional > 100))
        throw std::runtime_error("Undefined SSI weapon Strength encoding");
    unsigned value = strength;
    if (strength >= 19) value += 5;
    else if (strength == 18 && exceptional) {
        value = exceptional <= 50 ? 19 : (exceptional <= 75 ? 20 :
            (exceptional <= 90 ? 21 : (exceptional <= 99 ? 22 : 23)));
    }
    int accuracy = 0, damage = 0;
    if (value >= 1 && value <= 3) accuracy = -3;
    else if (value >= 4 && value <= 5) accuracy = -2;
    else if (value >= 6 && value <= 7) accuracy = -1;
    else if (value >= 17 && value <= 19) accuracy = 1;
    else if (value >= 20 && value <= 22) accuracy = 2;
    else if (value >= 23 && value <= 25) accuracy = 3;
    else if (value >= 26 && value <= 27) accuracy = 4;
    else if (value >= 28) accuracy = int(value)-23;
    if (value >= 1 && value <= 2) damage = -2;
    else if (value >= 3 && value <= 5) damage = -1;
    else if (value == 16) damage = 1;
    else if (value >= 17 && value <= 19) damage = int(value)-16;
    else if (value >= 20 && value <= 29) damage = int(value)-17;
    else if (value == 30) damage = 14;
    return {accuracy,damage};
}
// Rebuild existing readied associations, not eligibility checks or AI equipment
// selection. Indices replace native pointers; no serialized pointer is followed.
inline std::array<std::optional<std::size_t>,13> resolve_ssi_readied_equipment(
    const SsiMonsterTemplate& source, std::span<const SsiBaseItem> definitions) {
    std::array<std::optional<std::size_t>,13> slots{};
    for (std::size_t i = 0; i < source.items.size(); ++i) {
        const auto item = decode_ssi_monster_item(source.items[i]);
        if (!item.equipped) continue;
        if (item.type >= definitions.size()) throw std::runtime_error("SSI readied item reference out of range");
        const auto slot = definitions[item.type].slot;
        if (slot <= 8) slots[slot] = i; // Last readied item wins for ordinary slots.
        else if (slot == 9) {
            if (!slots[9]) slots[9] = i;
            else if (!slots[10]) slots[10] = i;
        }
        if (item.type == 73) slots[11] = i;
        if (item.type == 28) slots[12] = i;
    }
    return slots;
}
inline std::optional<SsiWeaponStats> prepare_ssi_weapon_stats(const SsiMonsterTemplate& source,
    std::span<const SsiBaseItem> definitions) {
    const auto slots = resolve_ssi_readied_equipment(source,definitions);
    if (!slots[0]) return std::nullopt; // Native no-weapon path preserves existing stats.
    const auto weapon = decode_ssi_monster_item(source.items[*slots[0]]);
    const auto flags = definitions[weapon.type].weapon_flags;
    SsiWeaponContext context;
    context.base_thac0_code = source.character[0x2d]; context.race_code = source.character[0x2e];
    if (flags & 2) context.dexterity_to_hit = ssi_dexterity_accuracy(source.character[0x13]);
    if (flags & 4) {
        const auto strength = ssi_strength_weapon_bonuses(source.character[0x10],source.character[0x16],
            source.character[0xaa] != 0);
        context.strength_to_hit = strength[0]; context.strength_to_damage = strength[1];
    }
    if (slots[11]) context.arrow_bonus = decode_ssi_monster_item(source.items[*slots[11]]).magical_bonus;
    if (slots[12]) context.bolt_bonus = decode_ssi_monster_item(source.items[*slots[12]]).magical_bonus;
    return ssi_weapon_stats(definitions,weapon,context);
}
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
