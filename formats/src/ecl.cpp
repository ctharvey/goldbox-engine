// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Goldbox Engine contributors
// Six-bit decoding adapted from Gold Box Explorer by Bil Simser (MIT).
// SSI Engine consulted for opcode/operand behavior; no Java code included.
// Attribution and preserved notices: THIRD_PARTY.md and licenses/.
#include "goldbox/formats/ecl.hpp"

#include <array>

namespace goldbox::formats {
namespace {

void require_available(std::span<const std::uint8_t> bytes, std::size_t offset,
                       std::size_t needed) {
    if (offset > bytes.size() || needed > bytes.size() - offset) {
        throw EclDecodeError("Truncated ECL operand/instruction");
    }
}

std::uint32_t read_le(std::span<const std::uint8_t> bytes,
                      std::size_t offset, std::size_t count) {
    require_available(bytes, offset, count);
    std::uint32_t result = 0;
    for (std::size_t i = 0; i < count; ++i) {
        result |= static_cast<std::uint32_t>(bytes[offset + i]) << (8 * i);
    }
    return result;
}

char sixbit_to_char(std::uint8_t value) {
    return static_cast<char>(value <= 31 ? value + 64 : value);
}

} // namespace

std::string decode_ecl_sixbit_text(std::span<const std::uint8_t> packed) {
    if (packed.size() > 65536) {
        throw EclDecodeError("ECL string exceeds maximum supported size");
    }
    std::string result;
    result.reserve((packed.size() * 4 + 2) / 3);
    std::uint32_t bits = 0;
    int available = 0;
    for (const std::uint8_t byte : packed) {
        bits = ((bits << 8) | byte) & 0xFFFFu;
        available += 8;
        while (available >= 6) {
            available -= 6;
            const auto code = static_cast<std::uint8_t>((bits >> available) & 0x3Fu);
            if (code != 0) result.push_back(sixbit_to_char(code));
        }
    }
    return result;
}

EclOperand decode_ecl_operand(std::span<const std::uint8_t> bytes,
                              std::size_t& offset) {
    const std::size_t start = offset;
    require_available(bytes, start, 1);
    const auto type = bytes[start];
    std::size_t cursor = start + 1;
    EclOperand operand;
    operand.type = type;
    operand.offset = start;
    switch (type) {
    case 0x00:
        operand.value = read_le(bytes, cursor, 1);
        cursor += 1;
        break;
    case 0x01:
    case 0x02:
    case 0x03:
    case 0x05:
    case 0x81:
        operand.value = read_le(bytes, cursor, 2);
        cursor += 2;
        break;
    case 0x04:
        operand.value = read_le(bytes, cursor, 4);
        cursor += 4;
        break;
    case 0x80: {
        const auto length = read_le(bytes, cursor, 1);
        ++cursor;
        require_available(bytes, cursor, length);
        operand.text = decode_ecl_sixbit_text(bytes.subspan(cursor, length));
        cursor += length;
        break;
    }
    default:
        throw EclDecodeError("Unsupported ECL operand type " + std::to_string(type));
    }
    operand.size = cursor - start;
    offset = cursor;
    return operand;
}

EclInstruction decode_ecl_instruction(
    std::span<const std::uint8_t> bytes,
    const std::size_t offset,
    std::span<const EclOpcodeSpec> dialect) {
    require_available(bytes, offset, 1);
    const auto opcode = bytes[offset];
    const EclOpcodeSpec* selected = nullptr;
    for (const auto& spec : dialect) {
        if (spec.opcode != opcode) continue;
        if (selected != nullptr) {
            throw EclDecodeError("Ambiguous ECL opcode for selected dialect");
        }
        selected = &spec;
    }
    if (selected == nullptr) {
        throw EclDecodeError("Unknown ECL opcode " + std::to_string(opcode));
    }
    if (selected->operand_count > 32 || selected->variable_count_operand >= selected->operand_count ||
        selected->variable_count_operand < -1) {
        throw EclDecodeError("Invalid ECL opcode specification");
    }
    EclInstruction instruction;
    instruction.opcode = opcode;
    instruction.offset = offset;
    instruction.mnemonic = selected->mnemonic;
    std::size_t cursor = offset + 1;
    for (int i = 0; i < selected->operand_count; ++i) {
        instruction.operands.push_back(decode_ecl_operand(bytes, cursor));
    }
    if (selected->variable_count_operand >= 0) {
        const auto& count = instruction.operands[static_cast<std::size_t>(selected->variable_count_operand)];
        if (count.type != 0x00 || count.value > 64) {
            throw EclDecodeError("Invalid/oversized dynamic ECL operand count");
        }
        for (std::uint32_t i = 0; i < count.value; ++i) {
            instruction.operands.push_back(decode_ecl_operand(bytes, cursor));
        }
    }
    instruction.size = cursor - offset;
    return instruction;
}

std::span<const EclOpcodeSpec> provisional_ecl_opcodes() {
    // OpCode tables from Gold Box Explorer / SSI Engine overlap, but there are
    // collisions between game generations (e.g. 0x1F, 0x21, 0x27). Leave
    // disputed opcodes out rather than guessing. Dialect-specific tables
    // will be required for real-game importers.
    static constexpr std::array<EclOpcodeSpec, 26> specs{{
        {0x00, "EXIT", 0}, {0x01, "GOTO", 1}, {0x02, "GOSUB", 1},
        {0x03, "COMPARE", 2}, {0x04, "ADD", 3}, {0x05, "SUBTRACT", 3},
        {0x06, "DIVIDE", 3}, {0x07, "MULTIPLY", 3}, {0x08, "RANDOM", 2},
        {0x09, "WRITE_MEM", 2}, {0x0A, "LOAD_CHAR", 1},
        {0x0B, "LOAD_MON", 3}, {0x0D, "SPRITE_ADVANCE", 0},
        {0x0E, "PICTURE", 1}, {0x0F, "INPUT_NUMBER", 2},
        {0x10, "INPUT_STRING", 2}, {0x11, "PRINT", 1},
        {0x12, "PRINT_CLEAR", 1}, {0x13, "RETURN", 0},
        {0x14, "COMPARE_AND", 4}, {0x16, "IF_EQUALS", 0},
        {0x17, "IF_NOT_EQUALS", 0}, {0x18, "IF_LESS", 0},
        {0x19, "IF_GREATER", 0}, {0x1A, "IF_LESS_EQUALS", 0},
        {0x1B, "IF_GREATER_EQUALS", 0}
    }};
    return specs;
}

} // namespace goldbox::formats
