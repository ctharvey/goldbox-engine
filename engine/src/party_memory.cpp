// SPDX-License-Identifier: GPL-3.0-or-later
// Independent read-only SSI adapter; research references in attribution/sources.json.
#include "goldbox/party_memory.hpp"
#include <algorithm>
#include <set>
#include <regex>
#include <stdexcept>

namespace goldbox {
namespace {
bool character_address(const PartyMemoryLayout& layout, std::uint16_t address) {
    return address >= layout.record_base && std::size_t(address) < std::size_t(layout.record_base)+layout.record_size;
}
const Character& member(const Party& party, std::size_t loaded) {
    if (loaded >= party.size()) throw std::runtime_error("No loaded party character");
    return party[loaded];
}
}
void validate_party_memory_layout(const PartyMemoryLayout& layout, const CreationRules& rules) {
    if (layout.dialect != "ssi-selected-character-v1") throw std::runtime_error("Unsupported party memory dialect");
    if (!layout.record_size || std::size_t(layout.record_base)+layout.record_size > 65536 ||
        character_address(layout,layout.count_address)) throw std::runtime_error("Invalid party memory range");
    if (layout.selected_address && party_memory_intersects(layout,*layout.selected_address,1))
        throw std::runtime_error("Selected character address overlaps party memory");
    if (layout.abilities.empty() || layout.abilities.size() > 64) throw std::runtime_error("Invalid party ability bindings");
    std::set<std::uint16_t> offsets;
    for (const auto& binding : layout.abilities) {
        const auto found = std::find_if(rules.abilities.begin(),rules.abilities.end(),
            [&](const auto& ability) { return ability.id == binding.ability; });
        if (found == rules.abilities.end() || binding.offset >= layout.record_size ||
            !offsets.insert(binding.offset).second) throw std::runtime_error("Invalid/conflicting party ability binding");
        if (binding.percentile && (!rules.percentile_ability ||
            *rules.percentile_ability != static_cast<std::size_t>(found-rules.abilities.begin())))
            throw std::runtime_error("Invalid party percentile binding");
    }
    if (layout.status) {
        const auto& binding = *layout.status;
        if (binding.offset >= layout.record_size || !offsets.insert(binding.offset).second ||
            binding.values.empty() || binding.values.size() > 64)
            throw std::runtime_error("Invalid/conflicting party status binding");
        static const std::regex pattern("[a-z0-9][a-z0-9.-]{0,63}");
        for (const auto& [name,value] : binding.values) {
            (void)value;
            if (!std::regex_match(name,pattern)) throw std::runtime_error("Invalid party status ID");
        }
    }
    if (layout.levels.size() > 64) throw std::runtime_error("Too many party level bindings");
    std::set<std::string> classes;
    for (const auto& binding : layout.levels) {
        const auto found = std::find_if(rules.classes.begin(),rules.classes.end(),
            [&](const auto& choice) { return choice.id == binding.character_class; });
        if (found == rules.classes.end() || !found->components.empty() || binding.offset >= layout.record_size ||
            !offsets.insert(binding.offset).second || !classes.insert(binding.character_class).second)
            throw std::runtime_error("Invalid/conflicting party level binding");
    }
}
bool party_memory_intersects(const PartyMemoryLayout& layout, std::uint16_t address, std::size_t width) {
    if (!width || width > 65536u-address) throw std::runtime_error("Invalid party memory access width");
    const auto end = std::size_t(address)+width;
    return (address < std::size_t(layout.record_base)+layout.record_size && end > layout.record_base) ||
        (address <= layout.count_address && end > layout.count_address);
}
std::optional<std::uint8_t> party_memory_byte(const PartyMemoryLayout& layout, const CreationRules& rules,
    const Party& party, std::size_t loaded, std::uint16_t address) {
    if (address == layout.count_address) {
        if (party.size() > 255) throw std::runtime_error("Party count exceeds byte range");
        return static_cast<std::uint8_t>(party.size());
    }
    if (!character_address(layout,address)) return std::nullopt;
    const auto& character = member(party,loaded);
    const auto offset = address-layout.record_base;
    const auto level = std::find_if(layout.levels.begin(),layout.levels.end(),
        [&](const auto& binding) { return binding.offset == offset; });
    if (level != layout.levels.end()) {
        if (character.levels.empty()) throw std::runtime_error("Character class levels are unknown");
        const auto found = character.levels.find(level->character_class);
        if (found == character.levels.end()) return 0;
        if (!found->second || found->second > 255) throw std::runtime_error("Character level exceeds byte range");
        return static_cast<std::uint8_t>(found->second);
    }
    if (layout.status && offset == layout.status->offset) {
        if (!character.status) throw std::runtime_error("Character status is unknown");
        const auto value = layout.status->values.find(*character.status);
        if (value == layout.status->values.end()) throw std::runtime_error("Unsupported character status ID");
        return value->second;
    }
    const auto field = std::find_if(layout.abilities.begin(),layout.abilities.end(),
        [&](const auto& b) { return b.offset == offset; });
    if (field == layout.abilities.end()) throw std::runtime_error("Unsupported character field at offset " + std::to_string(offset));
    const auto ability = std::find_if(rules.abilities.begin(),rules.abilities.end(),
        [&](const auto& a) { return a.id == field->ability; });
    if (ability == rules.abilities.end()) throw std::runtime_error("Unknown bound character ability");
    const auto index = static_cast<std::size_t>(ability-rules.abilities.begin());
    if (index >= character.abilities.size()) throw std::runtime_error("Character ability count mismatch");
    const auto value = field->percentile ? character.abilities[index].percentile : character.abilities[index].value;
    if (value < 0 || value > 255) throw std::runtime_error("Character ability exceeds byte range");
    return static_cast<std::uint8_t>(value);
}
std::optional<std::string> party_memory_string(const PartyMemoryLayout& layout, const Party& party,
    std::size_t loaded, std::uint16_t address) {
    // SSI's string operand in this window resolves to the loaded character name,
    // independently of the numeric field offset. No native name bytes fabricated.
    if (character_address(layout,address)) return member(party,loaded).name;
    if (address == layout.count_address) throw std::runtime_error("Party count is not a string");
    return std::nullopt;
}
} // namespace goldbox
