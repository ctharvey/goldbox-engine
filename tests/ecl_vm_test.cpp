// SPDX-License-Identifier: GPL-3.0-or-later
// All programs and text independently authored for these tests.
#include "goldbox/ecl_vm.hpp"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

using namespace goldbox;
using namespace goldbox::formats;
namespace {
struct Host : EclHost {
    std::vector<std::string> output;
    std::size_t choice{1};
    bool cleared{};
    int combats{};
    std::vector<std::uint16_t> calls;
    void native_call(std::uint16_t value) override { calls.push_back(value); }
    void text(std::string_view value, bool clear) override { output.emplace_back(value); cleared = clear; }
    std::size_t menu(std::span<const std::string> choices, std::string_view, bool) override {
        assert(choices.size() == 2); return choice;
    }
    void combat(EclState& state) override { ++combats; state.memory[0x4a10] = 1; }
};

std::vector<std::uint8_t> program(std::initializer_list<std::uint8_t> body) {
    std::vector<std::uint8_t> bytes{1,1,0x14,0x99};
    for (int i = 1; i < 5; ++i) bytes.insert(bytes.end(), {1,1,0x14,0x99});
    bytes.insert(bytes.end(), body);
    return bytes;
}

EclExecution run(const std::vector<std::uint8_t>& bytes, EclState& state, Host& host, EclLimits limits = {}) {
    return execute_ecl(bytes, EclEventKind::move, {"ssi-five-entry-v1", 0x9900, 0xc04f},
        minimal_vm_opcodes(), state, host, limits);
}
} // namespace

int main() {
    EclState state;
    Host host;
    for (const auto type : {1,2,3}) {
        const auto call = program({0x2d,static_cast<std::uint8_t>(type),3,0xba,0});
        assert(run(call,state,host).exited);
        assert(host.calls.back() == 0xba03); // Raw word token, never a memory dereference.
    }
    assert(host.calls.size() == 3);
    assert(!run(program({0x2d,0x80,1,4,0}),state,host).exited);
    assert(!run(program({0x2d,5,3,0xba,0}),state,host).exited);
    assert(!run(program({0x2d,1,3}),state,host).exited);
    assert(host.calls.size() == 3);
    // Write a quest flag, compare it, skip a text output, call a subroutine,
    // output independently authored six-bit A, then return and exit.
    const auto bytes = program({
        9,0,7,1,0,0x4a,             // 20: WRITE byte -> 0x4a00
        3,1,0,0x4a,0,8,             // 26: COMPARE 7,8
        0x16,                        // 32: IF_EQUALS (false)
        0x11,0x80,1,4,              // 33: skipped PRINT A
        2,1,0x2a,0x99,              // 37: GOSUB 42
        0,                           // 41: EXIT
        0x12,0x80,1,4,              // 42: PRINT_CLEAR A
        9,2,0x34,0x12,3,1,0x4a,     // 46: WRITE word -> 0x4a01
        0x13                         // 53: RETURN
    });
    auto result = run(bytes, state, host);
    assert(result.exited && result.diagnostic.empty());
    assert(host.output == std::vector<std::string>{"A"} && host.cleared);
    assert(state.memory[0x4a00] == 7 && state.memory[0x4a01] == 0x34 && state.memory[0x4a02] == 0x12);
    auto state2 = EclState{}; Host host2;
    const auto repeat = run(bytes, state2, host2);
    assert(repeat.steps == result.steps && state.memory == state2.memory && host.output == host2.output);
    result = run(program({1,1,20,0x99}), state, host, {12, 4});
    assert(!result.exited && result.steps == 12 && result.diagnostic == "ECL step limit exceeded");
    result = run(program({2,1,20,0x99}), state, host, {100, 3});
    assert(!result.exited && result.diagnostic == "ECL stack depth limit exceeded");
    assert(run(program({0x13}), state, host).diagnostic == "ECL RETURN with empty stack");
    assert(!run(program({1,1,1,0x99}), state, host).exited); // target in header
    assert(!run(program({9,0,7,0,1,0}), state, host).exited); // literal destination
    assert(!run(program({9,0,7,3,255,255,0}), state, host).exited); // word overflow
    assert(!run(program({0x11,0x81,0,0x4a,0}), state, host).exited); // unresolved string
    assert(!run(program({9,0}), state, host).exited); // truncated
    assert(run(program({0xff}), state, host).diagnostic == "Unknown ECL opcode 255");
    assert(run(program({0x0d,0}), state, host).diagnostic == "Unsupported VM opcode 13");
    state.memory[0x4a20] = 42;
    assert(run(program({0x2a,1,0x1f,0x4a,0,1,1,0x21,0x4a,0}), state, host).exited);
    assert(state.memory[0x4a21] == 42);
    // Code-address reads refer to the imported program, not zero-initialized flags.
    assert(run(program({0x2a,1,0,0x99,0,0,1,0x22,0x4a,0}), state, host).exited);
    assert(state.memory[0x4a22] == 1);
    assert(!run(program({9,0,42,1,20,0x99,0}), state, host).exited);
    assert(!run(program({0x2a,3,255,255,0,1,1,0,0x4a,0}), state, host).exited);
    // String memory and menu writes persist in supplied state, combat is a host callback.
    result = run(program({9,0x80,1,4,0x81,0,0x4b, 0x11,0x81,0,0x4b,
        0x2b,1,3,0x4a,0,2,0x80,1,4,0x80,1,8, 0x24,0}), state, host);
    assert(result.exited && state.strings[0x4b00] == "A" && state.memory[0x4a03] == 1 && host.combats == 1);
    host.choice = 2;
    assert(!run(program({0x2b,1,3,0x4a,0,2,0x80,1,4,0x80,1,8,0}), state, host).exited);
    // All six conditions execute the immediately following instruction only when true.
    for (std::uint8_t opcode = 0x16; opcode <= 0x1b; ++opcode) {
        Host conditional;
        auto script = program({3,0,1,0,2, opcode,0x11,0x80,1,4,0});
        assert(run(script, state, conditional).exited);
        const bool expected = opcode == 0x17 || opcode == 0x18 || opcode == 0x1a;
        assert(conditional.output.size() == (expected ? 1u : 0u));
    }
    // Indexed subroutine returns after the table, skipping no bytes by heuristic.
    state.memory[0xc04f] = 1;
    result = run(program({0x26,1,0x4f,0xc0,0,2,1,33,0x99,1,34,0x99,0,
        0x13,0x11,0x80,1,4,0x13}), state, host);
    assert(result.exited);
    assert(result.dispatches.size() == 1 && result.dispatches[0].selector == 1 &&
        result.dispatches[0].target_offset == 34 && result.dispatches[0].subroutine);
    auto conflict = minimal_vm_opcodes();
    for (auto& spec : conflict) if (spec.opcode == 0x11) spec.operand_count = 0;
    result = execute_ecl(program({0x11,0}), EclEventKind::move,
        {"ssi-five-entry-v1",0x9900,0xc04f}, conflict, state, host);
    assert(!result.exited && result.diagnostic == "Conflicting VM opcode definition");
    // Jump into an already decoded operand must fail, rather than decode it as EXIT.
    result = run(program({9,0,7,1,0,0x4a,1,1,21,0x99}), state, host);
    assert(!result.exited && result.diagnostic == "ECL branch inside operand data");
    // AND replaces stale comparison flags with zero/nonzero, after storing its result.
    Host bit_host; state = {};
    result = run(program({3,0,0,0,0,0x2f,0,1,0,1,1,0,0x4a,
        0x17,0x11,0x80,1,4,0}),state,bit_host);
    assert(result.exited && state.memory[0x4a00] == 1 && bit_host.output == std::vector<std::string>{"A"});
    bit_host.output.clear();
    result = run(program({3,0,1,0,0,0x2f,0,1,0,0,1,0,0x4a,
        0x16,0x11,0x80,1,4,0}),state,bit_host);
    assert(result.exited && state.memory[0x4a00] == 0 && bit_host.output == std::vector<std::string>{"A"});
    return 0;
}
