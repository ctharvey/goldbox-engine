// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Goldbox Engine contributors
#pragma once

#include "goldbox/formats/ecl.hpp"
#include "goldbox/formats/geo.hpp"

#include <array>
#include <string>

namespace goldbox::formats {

// Configuration describes a format dialect, never a title check in the core.
struct EclDispatchDialect {
    std::string id;
    std::uint16_t code_base{};
    std::uint16_t square_info_address{};
    bool optional_5000_prefix{true};
    std::optional<std::array<std::uint16_t, 3>> position_addresses; // X, Y, direction
};

enum class EclEventKind : std::size_t { move, search, rest, interrupted_rest, initialize };

struct EclProgramEntries {
    std::size_t record_prefix_size{};
    std::size_t header_size{};
    // Offsets relative to code after the optional record prefix, not addresses.
    std::array<std::size_t, 5> instruction_offsets{};
};

struct EclDispatchTable {
    std::size_t instruction_offset{};
    bool subroutine{};
    EclOperand selector;
    std::vector<std::size_t> target_offsets;
};

struct ResolvedGeoEvent {
    GeoEventReference source;
    EclEventKind kind{};
    std::string dialect;
    std::optional<std::size_t> dispatch_offset;
    std::optional<std::size_t> bytecode_offset;
    std::string diagnostic;
};

// SSI-style lifecycle header: five instructions from byte zero. Never searches
// for opcode patterns. Unsupported dialects and duplicate specs fail closed.
EclProgramEntries decode_ecl_program_entries(std::span<const std::uint8_t> record,
    const EclDispatchDialect& dialect, std::span<const EclOpcodeSpec> opcodes);

// Caller must supply an offset reached by decoding control flow, not a guess.
EclDispatchTable decode_ecl_dispatch_table(std::span<const std::uint8_t> code,
    std::size_t offset, const EclDispatchDialect& dialect, std::span<const EclOpcodeSpec> opcodes,
    std::size_t minimum_target_offset);

// Resolve a selector only through constant GOTO/AND/WRITE_MEM routing. A route
// depending on other game state is explicitly unresolved, never guessed.
std::vector<ResolvedGeoEvent> resolve_geo_events(const GeoMap& map, const DaxArchive& ecl,
    std::uint8_t ecl_record, EclEventKind kind, const EclDispatchDialect& dialect,
    std::span<const EclOpcodeSpec> opcodes);

// Namespace with the caller's game + archive identity, not a hardcoded map name.
std::string geo_event_id(std::string_view game, std::string_view archive, const GeoEventReference& ref);

std::vector<EclOpcodeSpec> routing_ecl_opcodes();

} // namespace goldbox::formats
