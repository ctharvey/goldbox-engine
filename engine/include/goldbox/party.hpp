// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <filesystem>
#include <map>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace goldbox {
struct CreationChoice { std::string id, label; };
struct CreationRace {
    std::string id, label;
    std::vector<int> minimum;
    std::map<std::string,std::vector<int>> maximum;
    std::map<std::string,int> percentile_maximum;
    std::vector<std::string> classes;
};
struct CreationClass {
    std::string id, label;
    std::vector<int> minimum;
    std::vector<std::string> alignments;
    bool percentile{};
};
struct CreationRules {
    std::string id;
    std::size_t party_limit{}, name_limit{};
    int dice_count{}, dice_sides{}, drop_lowest{};
    std::vector<CreationChoice> abilities, genders, alignments;
    std::vector<CreationRace> races;
    std::vector<CreationClass> classes;
    std::optional<std::size_t> percentile_ability;
    int percentile_at{};
};
struct AbilityValue { int value{}, percentile{}; };
struct Character {
    std::string name, race, gender, character_class, alignment;
    std::vector<AbilityValue> abilities;
    // Symbolic condition; absent means unknown (including legacy engine saves).
    std::optional<std::string> status{};
};
using Party = std::vector<Character>;
CreationRules read_creation_rules(const std::filesystem::path& path);
// Explicit generator, bounded rejection and fixed dice mapping: reproducible across STL implementations.
std::vector<AbilityValue> roll_abilities(const CreationRules& rules, std::string_view race,
    std::string_view gender, std::mt19937& random);
std::vector<std::size_t> eligible_classes(const CreationRules& rules, const Character& draft);
Character finish_character(const CreationRules& rules, Character draft, std::mt19937& random);
void validate_character(const CreationRules& rules, const Character& character);
void add_character(Party& party, const CreationRules& rules, Character character);
std::string serialize_party(const Party& party, const CreationRules& rules);
Party read_party(std::string_view json, const CreationRules& rules);
} // namespace goldbox
