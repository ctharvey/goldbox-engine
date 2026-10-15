// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "goldbox/ecl_vm.hpp"

namespace goldbox {

enum class Direction : std::uint8_t { north, east, south, west };

// Explicit imported map/ECL pairing. No record-ID guessing or title logic.
struct ExplorationSession {
    formats::GeoMap map;
    std::vector<std::uint8_t> ecl_record;
    formats::EclDispatchDialect dialect;
    std::vector<formats::EclOpcodeSpec> opcodes;
    EclState state;
    std::uint8_t row{};
    std::uint8_t column{};
    Direction facing{Direction::north};

    // Conservative pilot: nonzero walls block movement; doors are not modeled.
    bool move(Direction direction);
    EclExecution trigger(formats::EclEventKind kind, EclHost& host, EclLimits limits = {});
};

// Caller supplies source identity (including locally computed content hashes).
// Saves contain local game state; never commit them or extracted strings.
std::string serialize_exploration(const ExplorationSession& session, std::string_view source_identity);
void restore_exploration(ExplorationSession& session, std::string_view json, std::string_view source_identity);

} // namespace goldbox
