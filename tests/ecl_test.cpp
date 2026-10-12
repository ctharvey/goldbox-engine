// SPDX-License-Identifier: GPL-3.0-or-later
// Synthetic ECL fixtures only. No SSI game files or original game text.
#include "goldbox/formats/ecl.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <vector>

using namespace goldbox::formats;

namespace {
void rejects(const std::function<void()>& action) {
    bool rejected = false;
    try { action(); } catch (const EclDecodeError&) { rejected = true; }
    assert(rejected);
}
}

int main() {
    // Packed six-bit ABCD: 000001 000010 000011 000100.
    const std::array<std::uint8_t, 3> abcd{0x04, 0x20, 0xC4};
    assert(decode_ecl_sixbit_text(abcd) == "ABCD");
    const std::array<std::uint8_t, 2> aa{0x04, 0x10}; // A A + padding
    assert(decode_ecl_sixbit_text(aa) == "AA");
    const std::array<std::uint8_t, 0> empty{};
    assert(decode_ecl_sixbit_text(empty).empty());

    const std::array<std::uint8_t, 13> operand_stream{
        0x00, 0xFE,             // literal 254
        0x01, 0x34, 0x12,       // address 0x1234
        0x04, 0x78, 0x56, 0x34, 0x12, // extended 32-bit form
        0x80, 0x01, 0x04       // 1 packed byte -> A
    };
    std::size_t off=0;
    const auto op1=decode_ecl_operand(operand_stream, off);
    assert(op1.type==0 && op1.value==254 && op1.offset==0 && off==2);
    const auto op2=decode_ecl_operand(operand_stream, off);
    assert(op2.type==1 && op2.value==0x1234 && op2.size==3 && off==5);
    const auto op3=decode_ecl_operand(operand_stream, off);
    assert(op3.type==4 && op3.value==0x12345678 && op3.size==5 && off==10);
    const auto op4=decode_ecl_operand(operand_stream, off);
    assert(op4.type==0x80 && op4.text=="A" && off==13);

    // Offset on failure remains unchanged.
    off=12;
    rejects([&]{ decode_ecl_operand(operand_stream,off); });
    assert(off==12);
    const std::array<std::uint8_t, 2> unknown{0xF0,0};
    off=0;
    rejects([&]{ decode_ecl_operand(unknown, off); });
    assert(off==0);
    const std::array<std::uint8_t, 4> truncated_text{0x80,0x03,0x04,0x20};
    rejects([&]{ decode_ecl_operand(truncated_text,off); });
    const std::array<std::uint8_t, 2> zero_text{0x80,0x00};
    const auto zero=decode_ecl_operand(zero_text,off);
    assert(zero.text.empty() && off==2);

    // The selected dialect controls instruction argument interpretation.
    const std::array<std::uint8_t, 4> print_bytes{0x11, 0x80, 0x01, 0x04};
    auto print=decode_ecl_instruction(print_bytes,0,provisional_ecl_opcodes());
    assert(print.opcode==0x11 && print.mnemonic=="PRINT" && print.size==4);
    assert(print.operands.size()==1 && print.operands.front().text=="A");
    const std::array<std::uint8_t, 3> print_offset{0xFF, 0x00, 0xFE};
    auto exit=decode_ecl_instruction(print_offset,1,provisional_ecl_opcodes());
    assert(exit.offset==1 && exit.size==1 && exit.mnemonic=="EXIT");
    rejects([&]{ decode_ecl_instruction(print_offset,0,provisional_ecl_opcodes()); });
    rejects([&]{ decode_ecl_instruction(print_bytes,4,provisional_ecl_opcodes()); });
    rejects([&]{ decode_ecl_instruction(std::span(print_bytes).first(3),0,provisional_ecl_opcodes()); });

    const std::array<EclOpcodeSpec, 1> custom{{{0xA0,"CUSTOM",1}}};
    const std::array<std::uint8_t, 3> custom_code{0xA0,0x00,0x2A};
    auto custom_result=decode_ecl_instruction(custom_code,0,custom);
    assert(custom_result.operands.front().value==42);
    const std::array<EclOpcodeSpec, 2> dup{{{0xA0,"CUSTOM",1},{0xA0,"ALT",1}}};
    rejects([&]{ decode_ecl_instruction(custom_code,0,dup); });

    const std::array<EclOpcodeSpec, 1> varspec{{{0xA1,"VAR",1,0}}};
    const std::array<std::uint8_t, 7> dynamic_code{0xA1,0x00,0x02, 0x00,0x07, 0x00,0x09};
    auto dynamic=decode_ecl_instruction(dynamic_code,0,varspec);
    assert(dynamic.operands.size()==3 && dynamic.operands[1].value==7 &&
           dynamic.operands[2].value==9 && dynamic.size==7);
    const std::array<std::uint8_t, 3> too_many{0xA1,0x00,0xFF};
    rejects([&]{decode_ecl_instruction(too_many,0,varspec);});
    const std::array<std::uint8_t, 3> bad_count{0xA1,0x01,0x00};
    rejects([&]{decode_ecl_instruction(bad_count,0,varspec);});
    const std::array<EclOpcodeSpec, 1> invalid_spec{{{0xA0,"BAD",0,0}}};
    rejects([&]{decode_ecl_instruction(custom_code,0,invalid_spec);});
    return 0;
}
