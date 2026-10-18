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
    std::uint8_t loaded_character{};
    // Immutable initial configuration used to validate saved RNG parameters.
    std::optional<HighWordLcg32> initial_random{};
    EclRandomDialect random_dialect{EclRandomDialect::disabled};
    std::optional<EclSoundCall> sound_call{};

    // Conservative pilot: nonzero walls block movement; doors are not modeled.
    bool move(Direction direction);
    void load_area(std::array<std::uint8_t,3> values);
    EclExecution trigger(formats::EclEventKind kind, EclHost& host, EclLimits limits = {});
};

// Caller supplies source identity (including locally computed content hashes).
// Saves contain local game state; never commit them or extracted strings.
std::string serialize_exploration(const ExplorationSession& session, std::string_view source_identity);
void restore_exploration(ExplorationSession& session, std::string_view json, std::string_view source_identity);

} // namespace goldbox
