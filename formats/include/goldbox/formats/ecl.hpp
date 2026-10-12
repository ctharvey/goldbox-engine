// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Goldbox Engine contributors
// Six-bit text/typed operand decoding adapted with attribution from the
// MIT-licensed Gold Box Explorer. SSI Engine was a behavior reference only.
// See THIRD_PARTY.md and licenses/.
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace goldbox::formats {

class EclDecodeError final : public std::runtime_error {
public:
    explicit EclDecodeError(const std::string& message) : std::runtime_error(message) {}
};

struct EclOperand {
    std::uint8_t type{};
    std::size_t offset{};
    std::size_t size{};
    std::uint32_t value{};    // Only for numeric and memory-reference operands.
    std::string text;        // Only for an inline six-bit string (type 0x80).
};

struct EclOpcodeSpec {
    std::uint8_t opcode{};
    std::string_view mnemonic;
    std::uint8_t operand_count{};
    // Index of the operand holding an additional operand count, or -1.
    int variable_count_operand{-1};
};

struct EclInstruction {
    std::uint8_t opcode{};
    std::size_t offset{};
    std::size_t size{};
    std::string_view mnemonic;
    std::vector<EclOperand> operands;
};

// Decodes packed 6-bit ECL text; 0 is a pad/empty character. Output is
// printable ASCII only for the historical 6-bit alphabet, not localization.
std::string decode_ecl_sixbit_text(std::span<const std::uint8_t> packed);

// Decode a typed operand at byte offset; update offset only on success.
// Types: 0x00=byte, 0x01/02/03/05/81=LE word,
// 0x04=LE dword, 0x80=[byte length][packed six-bit text].
EclOperand decode_ecl_operand(std::span<const std::uint8_t> bytes, std::size_t& offset);

// Decode exactly one instruction from a *known instruction boundary* using
// a caller-selected dialect's opcode table. Does not execute or follow jumps.
// Unknown/ambiguous opcodes, invalid operands, or truncation fail closed.
EclInstruction decode_ecl_instruction(
    std::span<const std::uint8_t> bytes,
    std::size_t offset,
    std::span<const EclOpcodeSpec> dialect);

// Conservative cross-game starter; NOT verified against original SSI binaries.
// Title-specific ECL dialects may replace or extend this table later.
std::span<const EclOpcodeSpec> provisional_ecl_opcodes();

} // namespace goldbox::formats
