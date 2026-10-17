// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "goldbox/party.hpp"
#include <cstdint>
#include <map>

namespace goldbox {
struct PartyAbilityBinding {
    std::uint16_t offset{};
    std::string ability;
    bool percentile{};
};
struct PartyStatusBinding {
    std::uint16_t offset{};
    std::map<std::string,std::uint8_t> values;
};
// Shared SSI dialect adapter. Native addresses/offsets belong to game profiles.
struct PartyMemoryLayout {
    std::string dialect;
    std::uint16_t record_base{}, record_size{}, count_address{};
    std::vector<PartyAbilityBinding> abilities;
    std::optional<std::uint16_t> selected_address{};
    std::optional<PartyStatusBinding> status{};
};
void validate_party_memory_layout(const PartyMemoryLayout& layout, const CreationRules& rules);
bool party_memory_intersects(const PartyMemoryLayout& layout, std::uint16_t address, std::size_t width);
std::optional<std::uint8_t> party_memory_byte(const PartyMemoryLayout& layout, const CreationRules& rules,
    const Party& party, std::size_t loaded, std::uint16_t address);
std::optional<std::string> party_memory_string(const PartyMemoryLayout& layout, const Party& party,
    std::size_t loaded, std::uint16_t address);
} // namespace goldbox
