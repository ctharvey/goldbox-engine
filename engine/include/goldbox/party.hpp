// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "goldbox/random.hpp"
#include <filesystem>
#include <map>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace goldbox {
struct CreationChoice { std::string id, label; };
struct StartingAgeDice { int base{}, count{}, sides{}; };
struct CreationAgeAdjustment {
    int minimum{}, maximum{};
    std::vector<int> values;
    std::vector<bool> positive_only;
    bool wrap_byte{};
};
struct CreationRace {
    std::string id, label;
    std::vector<int> minimum;
    std::map<std::string,std::vector<int>> maximum;
    std::map<std::string,int> percentile_maximum;
    std::vector<std::string> classes;
    // Empty preserves legacy unadjusted rolling; otherwise one signed value per ability.
    std::vector<int> adjustments;
    std::map<std::string,StartingAgeDice> starting_ages{};
    std::vector<CreationAgeAdjustment> age_adjustments{};
};
struct CreationHitPoints {
    int count{}, sides{}, minimum{};
    std::size_t ability{};
    int ability_minimum{};
    std::vector<int> modifiers;
};
struct CreationMoney { std::string currency; int count{}, sides{}, multiplier{}; };
struct CreationClass {
    std::string id, label;
    std::vector<int> minimum;
    std::vector<std::string> alignments;
    bool percentile{};
    std::vector<int> creation_minimum; // Optional creation-only floors; saved values remain unchanged.
    std::optional<CreationHitPoints> hit_points{};
    std::optional<CreationMoney> starting_money{};
};
enum class CreationBounds { reject, clamp };
struct CreationRules {
    std::string id;
    std::size_t party_limit{}, name_limit{};
    int dice_count{}, dice_sides{}, drop_lowest{};
    std::vector<CreationChoice> abilities, genders, alignments;
    std::vector<CreationRace> races;
    std::vector<CreationClass> classes;
    std::optional<std::size_t> percentile_ability;
    int percentile_at{};
    int keep_best_of{1};
    CreationBounds bounds{CreationBounds::reject};
    std::optional<int> percentile_roll_maximum;
    std::vector<std::uint16_t> initial_random_bounds{};
};
struct AbilityValue { int value{}, percentile{}; };
struct HitPoints { int base{}, current{}, maximum{}; };
struct Character {
    std::string name, race, gender, character_class, alignment;
    std::vector<AbilityValue> abilities;
    // Symbolic condition; absent means unknown (including legacy engine saves).
    std::optional<std::string> status{};
    std::optional<std::uint16_t> age{}; // Unknown for legacy parties; no automatic inference on load.
    std::optional<HitPoints> hit_points{};
    std::map<std::string,std::uint16_t> money{}; // Missing currency means unknown, rather than an inferred balance.
};
using Party = std::vector<Character>;
CreationRules read_creation_rules(const std::filesystem::path& path);
std::optional<std::uint16_t> roll_starting_age(const CreationRules& rules, std::string_view race,
    std::string_view character_class, std::mt19937& random);
std::optional<std::uint16_t> roll_starting_age(const CreationRules& rules, std::string_view race,
    std::string_view character_class, HighWordLcg32& random);
// Prepare age/abilities with class known. Existing age means a score reroll;
// name/alignment are validated separately after the UI supplies them.
Character roll_character(const CreationRules& rules, Character draft, std::mt19937& random);
Character roll_character(const CreationRules& rules, Character draft, HighWordLcg32& random);
// Explicit generator, bounded rejection and fixed dice mapping: reproducible across STL implementations.
std::vector<AbilityValue> roll_abilities(const CreationRules& rules, std::string_view race,
    std::string_view gender, std::mt19937& random);
std::vector<std::size_t> eligible_classes(const CreationRules& rules, const Character& draft);
std::vector<std::size_t> creation_classes(const CreationRules& rules, const Character& draft);
Character finish_character(const CreationRules& rules, Character draft, std::mt19937& random);
std::vector<AbilityValue> roll_abilities(const CreationRules& rules, std::string_view race,
    std::string_view gender, HighWordLcg32& random);
Character finish_character(const CreationRules& rules, Character draft, HighWordLcg32& random);
void validate_character(const CreationRules& rules, const Character& character);
void add_character(Party& party, const CreationRules& rules, Character character);
std::string serialize_party(const Party& party, const CreationRules& rules);
Party read_party(std::string_view json, const CreationRules& rules);
} // namespace goldbox
