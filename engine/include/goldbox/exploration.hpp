// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "goldbox/ecl_vm.hpp"
#include "goldbox/party.hpp"
#include "goldbox/party_memory.hpp"
#include <functional>

namespace goldbox {

enum class Direction : std::uint8_t { north, east, south, west };
struct MapResources {
    // Read-only resolver returning normalized GEO16 content. Archive/group
    // selection belongs to the caller's adapter, never to the VM.
    std::function<formats::GeoMap(std::uint8_t)> resolve;
    std::string source_identity;
};
struct ProgramResource {
    std::uint8_t record_id{};
    std::vector<std::uint8_t> bytes;
};
struct ProgramResources {
    std::function<ProgramResource(std::uint8_t)> resolve;
    std::string source_identity;
};
struct MonsterResources {
    // Resolve/validate an immutable template bundle by program and resource ID.
    std::function<void(std::uint8_t,std::uint8_t)> validate;
    std::string source_identity;
};
struct PreparedMonsterGroup {
    std::uint8_t program{}, resource{}, count{}, group{}, selector{};
    std::uint32_t template_seed{};
};
struct MonsterSetup {
    // Deterministic preparation recipes, not combat-ready normalized actors.
    std::vector<PreparedMonsterGroup> groups;
    std::uint8_t next_group{8};
};

struct ExplorationMove {
    bool moved{};
    std::optional<EclExecution> execution;
};

// Explicit initial map/ECL pairing and optional normalized-map resolver.
// Map loading does not replace the ECL program or guess archive groups.
struct ExplorationSession {
    formats::GeoMap map;
    std::vector<std::uint8_t> ecl_record;
    formats::EclDispatchDialect dialect;
    std::vector<formats::EclOpcodeSpec> opcodes;
    EclState state;
    std::uint8_t row{};
    std::uint8_t column{};
    Direction facing{Direction::north};
    std::optional<std::array<std::uint8_t,3>> wall_banks{};
    std::optional<MapResources> maps{};
    std::optional<std::array<std::uint8_t,3>> area{};
    std::optional<std::uint8_t> initial_map_record{};
    std::optional<CreationRules> character_rules{};
    Party party;
    std::optional<PartyMemoryLayout> party_memory{};
    std::optional<PartyStrengthBinding> party_strength{};
    std::uint8_t loaded_character{};
    // Immutable initial configuration used to validate saved RNG parameters.
    std::optional<HighWordLcg32> initial_random{};
    EclRandomDialect random_dialect{EclRandomDialect::disabled};
    std::optional<EclSoundCall> sound_call{};
    std::optional<ProgramResources> programs{};
    std::optional<std::uint8_t> current_program_record{};
    std::optional<std::uint16_t> position_refresh_call{};
    std::optional<EclPositionAdvanceCall> position_advance_call{};
    // Normalized map/program IDs selected by the campaign adapter.
    std::optional<std::array<std::uint8_t,2>> campaign_start{};
    std::uint32_t script_delay_ms{};
    // Profile-selected block, exit attempt and previous X/Y registers.
    std::optional<std::array<std::uint16_t,4>> movement_addresses{};
    std::vector<std::array<std::uint16_t,2>> transfer_clear_ranges;
    EclArithmeticDialect arithmetic_dialect{EclArithmeticDialect::disabled};
    std::optional<PartyClassCheckBinding> party_class_check{};
    std::optional<EclSurpriseBinding> surprise{};
    std::optional<EclEncounterBinding> encounter{};
    EclMonsterDialect monster_dialect{EclMonsterDialect::disabled};
    std::optional<MonsterResources> monster_resources{};
    std::optional<MonsterSetup> monster_setup{}; // Unknown until CLEAR_MON, including legacy saves.

    // Both GEO16 edges must permit traversal; boundaries remain solid.
    // Door state 1 is passable. Opening/bashing states 2/3 is not modeled.
    bool move(Direction direction);
    // A no-resource area retains its cached map for later rebinding, never traversal.
    bool has_dungeon_map() const {
        return !area || *area != std::array<std::uint8_t,3>{255,255,127};
    }
    // Configured lifecycle: source MOVE, permitted step, destination SEARCH.
    // Unconfigured sessions preserve the prototype move-then-event behavior.
    ExplorationMove move(Direction direction, EclHost& host, EclLimits limits = {});
    void load_area(std::array<std::uint8_t,3> values);
    EclExecution trigger(formats::EclEventKind kind, EclHost& host, EclLimits limits = {});
    EclExecution begin_campaign(EclHost& host, EclLimits limits = {});
};

// Caller supplies source identity (including locally computed content hashes).
// Saves contain local game state; never commit them or extracted strings.
std::string serialize_exploration(const ExplorationSession& session, std::string_view source_identity);
void restore_exploration(ExplorationSession& session, std::string_view json, std::string_view source_identity);

} // namespace goldbox
