// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Goldbox Engine contributors
// SSI Engine consulted as a behavior reference; no Java source translated.
#include "goldbox/formats/ecl_dispatch.hpp"

#include <map>
#include <set>

namespace goldbox::formats {
namespace {
void validate(const EclDispatchDialect& dialect, std::span<const EclOpcodeSpec> opcodes) {
    if (dialect.id != "ssi-five-entry-v1") throw EclDecodeError("Unsupported ECL dispatch dialect: " + dialect.id);
    std::set<std::uint8_t> seen;
    for (const auto& spec : opcodes) {
        if (!seen.insert(spec.opcode).second) throw EclDecodeError("Conflicting ECL opcode definitions");
        if (spec.operand_count > 32 || spec.variable_count_operand < -1 ||
            spec.variable_count_operand >= spec.operand_count) throw EclDecodeError("Invalid ECL opcode definition");
        // Routing opcodes must have their exact arity, regardless of mnemonic.
        const int expected = spec.opcode == 0 ? 0 :
            (spec.opcode == 1 || spec.opcode == 2) ? 1 :
            spec.opcode == 9 ? 2 : spec.opcode == 0x2f ? 3 :
            (spec.opcode == 0x25 || spec.opcode == 0x26) ? 2 : -1;
        if (expected >= 0 && (spec.operand_count != expected || spec.variable_count_operand !=
            ((spec.opcode == 0x25 || spec.opcode == 0x26) ? 1 : -1))) {
            throw EclDecodeError("Conflicting routing opcode definition");
        }
    }
}

std::size_t target(const EclOperand& operand, std::span<const std::uint8_t> code,
                   const EclDispatchDialect& dialect, std::size_t minimum) {
    if ((operand.type != 1 && operand.type != 3 && operand.type != 5) || operand.value < dialect.code_base) {
        throw EclDecodeError("Invalid ECL branch address operand");
    }
    const auto offset = static_cast<std::size_t>(operand.value - dialect.code_base);
    if (offset < minimum || offset >= code.size()) throw EclDecodeError("ECL branch address outside code body");
    return offset;
}

// Only known numeric values may participate in static selector routing.
std::uint32_t numeric(const EclOperand& operand, const std::map<std::uint32_t, std::uint8_t>& memory) {
    if (operand.type == 0 || operand.type == 2 || operand.type == 4) return operand.value;
    if (operand.type == 1 || operand.type == 3 || operand.type == 5) {
        const auto low = memory.find(operand.value);
        if (low == memory.end()) throw EclDecodeError("Dispatch depends on unknown game state");
        if (operand.type != 3) return low->second;
        const auto high = memory.find(operand.value + 1);
        if (high == memory.end()) throw EclDecodeError("Dispatch depends on unknown game state");
        return low->second | (static_cast<std::uint32_t>(high->second) << 8);
    }
    throw EclDecodeError("Non-numeric dispatch operand");
}

void write(const EclOperand& operand, std::uint32_t value, std::map<std::uint32_t, std::uint8_t>& memory) {
    if (operand.type != 1 && operand.type != 3 && operand.type != 5) throw EclDecodeError("Invalid routing destination");
    if (operand.type == 3 && operand.value == 65535) throw EclDecodeError("Routing word write out of bounds");
    memory[operand.value] = static_cast<std::uint8_t>(value);
    if (operand.type == 3) memory[operand.value + 1] = static_cast<std::uint8_t>(value >> 8);
}
} // namespace

std::vector<EclOpcodeSpec> routing_ecl_opcodes() {
    const auto base = provisional_ecl_opcodes();
    std::vector<EclOpcodeSpec> result(base.begin(), base.end());
    result.push_back({0x25, "ON_GOTO", 2, 1});
    result.push_back({0x26, "ON_GOSUB", 2, 1});
    result.push_back({0x2f, "AND", 3});
    return result;
}

EclProgramEntries decode_ecl_program_entries(std::span<const std::uint8_t> record,
    const EclDispatchDialect& dialect, std::span<const EclOpcodeSpec> opcodes) {
    validate(dialect, opcodes);
    EclProgramEntries result;
    if (dialect.optional_5000_prefix && record.size() >= 2 && record[0] == 0x88 && record[1] == 0x13) {
        result.record_prefix_size = 2;
    }
    const auto code = record.subspan(result.record_prefix_size);
    if (code.size() > 65536u - dialect.code_base) throw EclDecodeError("ECL program exceeds address space");
    std::size_t cursor = 0;
    for (auto& offset : result.instruction_offsets) {
        offset = cursor;
        const auto instruction = decode_ecl_instruction(code, cursor, opcodes);
        if (instruction.opcode > 2) throw EclDecodeError("Unsupported ECL lifecycle entry instruction");
        cursor += instruction.size;
    }
    result.header_size = cursor;
    for (const auto offset : result.instruction_offsets) {
        const auto instruction = decode_ecl_instruction(code, offset, opcodes);
        if (instruction.opcode != 0) (void)target(instruction.operands[0], code, dialect, cursor);
    }
    return result;
}

EclDispatchTable decode_ecl_dispatch_table(std::span<const std::uint8_t> code,
    std::size_t offset, const EclDispatchDialect& dialect, std::span<const EclOpcodeSpec> opcodes,
    std::size_t minimum_target_offset) {
    validate(dialect, opcodes);
    const auto instruction = decode_ecl_instruction(code, offset, opcodes);
    if (instruction.opcode != 0x25 && instruction.opcode != 0x26) throw EclDecodeError("Expected ON_GOTO/ON_GOSUB table");
    EclDispatchTable result{offset, instruction.opcode == 0x26, instruction.operands[0], {}};
    for (std::size_t i = 2; i < instruction.operands.size(); ++i) {
        const auto address = target(instruction.operands[i], code, dialect, minimum_target_offset);
        if (address >= offset && address < offset + instruction.size) throw EclDecodeError("ECL target inside dispatch table");
        result.target_offsets.push_back(address);
    }
    return result;
}

std::vector<ResolvedGeoEvent> resolve_geo_events(const GeoMap& map, const DaxArchive& ecl,
    std::uint8_t ecl_record, EclEventKind kind, const EclDispatchDialect& dialect,
    std::span<const EclOpcodeSpec> opcodes) {
    const auto refs = bind_geo_events_to_ecl(map, ecl, ecl_record);
    const auto record = ecl.extract(ecl_record);
    const auto entries = decode_ecl_program_entries(record, dialect, opcodes);
    const auto code = std::span(record).subspan(entries.record_prefix_size);
    const auto kind_index = static_cast<std::size_t>(kind);
    if (kind_index >= 5) throw EclDecodeError("Invalid ECL lifecycle kind");
    std::vector<ResolvedGeoEvent> result;
    for (const auto& ref : refs) {
        ResolvedGeoEvent resolved{ref, kind, dialect.id, {}, {}, {}};
        std::map<std::uint32_t, std::uint8_t> memory{{dialect.square_info_address, ref.raw_event}};
        std::size_t offset = entries.instruction_offsets[kind_index];
        try {
            for (std::size_t step = 0; ; ++step) {
                if (step == 128) throw EclDecodeError("Dispatch routing step limit exceeded");
                const auto instruction = decode_ecl_instruction(code, offset, opcodes);
                if (instruction.opcode == 0) { resolved.diagnostic = "Lifecycle exits without selector dispatch"; break; }
                if (instruction.opcode == 1) {
                    offset = target(instruction.operands[0], code, dialect, entries.header_size);
                    continue;
                }
                if (instruction.opcode == 0x25 || instruction.opcode == 0x26) {
                    const auto table = decode_ecl_dispatch_table(code, offset, dialect, opcodes, entries.header_size);
                    const auto index = numeric(table.selector, memory);
                    if (index >= table.target_offsets.size()) throw EclDecodeError("GEO selector outside dispatch table");
                    resolved.dispatch_offset = offset;
                    resolved.bytecode_offset = table.target_offsets[index];
                    // Decode at the declared target; never scan intervening data.
                    (void)decode_ecl_instruction(code, *resolved.bytecode_offset, opcodes);
                    break;
                }
                if (instruction.opcode == 9) {
                    write(instruction.operands[1], numeric(instruction.operands[0], memory), memory);
                } else if (instruction.opcode == 0x2f) {
                    write(instruction.operands[2], numeric(instruction.operands[0], memory) &
                        numeric(instruction.operands[1], memory), memory);
                } else throw EclDecodeError("Unsupported/state-dependent ECL dispatch route at offset " + std::to_string(offset));
                offset += instruction.size;
            }
        } catch (const EclDecodeError& error) {
            resolved.bytecode_offset.reset();
            resolved.diagnostic = error.what();
        }
        result.push_back(std::move(resolved));
    }
    return result;
}

std::string geo_event_id(std::string_view game, std::string_view archive, const GeoEventReference& ref) {
    if (game.empty() || archive.empty() || game.find(':') != game.npos || archive.find(':') != archive.npos) {
        throw EclDecodeError("Invalid game/archive event identity");
    }
    return std::string(game) + ":geo:" + std::string(archive) + ":" + std::to_string(ref.geo_record_id) +
        ":" + std::to_string(ref.row) + ":" + std::to_string(ref.column);
}
} // namespace goldbox::formats
