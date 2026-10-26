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
    int delays{};
    void delay() override { ++delays; }
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
    {
        struct EncounterHost : Host {
            std::vector<std::size_t> actions;
            std::vector<EclSpriteRequest> sprites;
            std::vector<std::string> fourth_choices;
            std::uint8_t visible{2};
            std::array<std::uint8_t,2> rates{6,12};
            bool movement_available{true}, endless_wait{};
            std::uint8_t encounter_distance() override { return visible; }
            std::array<std::uint8_t,2> party_movement() override {
                if (!movement_available) return EclHost::party_movement();
                return rates;
            }
            void sprite_start(EclSpriteRequest request) override { sprites.push_back(request); }
            std::size_t menu(std::span<const std::string> choices, std::string_view, bool vertical) override {
                assert(!vertical);
                if (choices.size() == 1) return 0;
                assert(choices.size() == 4); fourth_choices.push_back(choices[3]);
                if (endless_wait) return 1;
                if (actions.empty()) return 0;
                const auto choice = actions.front(); actions.erase(actions.begin()); return choice;
            }
        } host;
        const auto ops = minimal_vm_opcodes(EclImageDialect::disabled,EclAreaDialect::disabled,
            EclPartyDialect::disabled,EclPartyCheckDialect::disabled,EclSurpriseDialect::disabled,EclEncounterDialect::menu14);
        const EclEncounterBinding binding{EclEncounterDialect::menu14,0x4a32,{0x4a30,0x4a31}};
        const auto bytes = [](std::uint8_t behavior, std::uint8_t distance, unsigned description_mask = 0) {
            auto code = program({3,0,1,0,2,0x29,0,7,0,distance,0,8,1,0x20,0x4a});
            for (unsigned i = 0; i < 5; ++i) code.insert(code.end(),{0,behavior});
            for (unsigned i = 0; i < 3; ++i) {
                code.push_back(0x80);
                if (description_mask & (1u<<i)) code.insert(code.end(),{1,static_cast<std::uint8_t>(4*(i+1))});
                else code.push_back(0);
            }
            code.insert(code.end(),{0,6,0,12,0});
            return code;
        };
        EclState state;
        const auto invoke = [&](const auto& code, const auto& selected, std::optional<EclEncounterBinding> adapter,
            EclLimits limits = {}) {
            return execute_ecl(code,EclEventKind::move,{"ssi-five-entry-v1",0x9900,0xc04f},selected,state,host,limits,
                EclRandomDialect::disabled,EclArithmeticDialect::disabled,std::nullopt,adapter);
        };
        // Native-handler golden tuples: behavior, first choice, distance, result, final distance, sprite calls.
        const unsigned cases[][6]{
            {0,0,0,1,0,1},{0,0,1,1,1,1},{0,0,2,1,2,1},{0,1,0,1,0,1},{0,1,1,1,1,1},{0,1,2,1,2,1},
            {0,2,0,2,0,1},{0,2,1,2,1,1},{0,2,2,2,2,1},{0,3,0,1,0,1},{0,3,1,1,1,1},{0,3,2,1,2,1},
            {1,0,0,1,0,1},{1,0,1,1,1,1},{1,0,2,1,2,1},{1,1,0,1,0,1},{1,1,1,1,1,1},{1,1,2,1,2,1},
            {1,2,0,2,0,1},{1,2,1,2,1,1},{1,2,2,2,2,1},{1,3,0,3,0,1},{1,3,1,1,0,2},{1,3,2,1,1,2},
            {2,0,0,1,0,1},{2,0,1,1,1,1},{2,0,2,1,2,1},{2,1,0,0,0,1},{2,1,1,0,1,1},{2,1,2,0,2,1},
            {2,2,0,0,0,1},{2,2,1,0,1,1},{2,2,2,0,2,1},{2,3,0,0,0,1},{2,3,1,0,1,1},{2,3,2,0,2,1},
            {3,0,0,1,0,1},{3,0,1,1,1,1},{3,0,2,1,2,1},{3,1,0,1,0,1},{3,1,1,1,0,2},{3,1,2,1,1,2},
            {3,2,0,2,0,1},{3,2,1,2,1,1},{3,2,2,2,2,1},{3,3,0,3,0,1},{3,3,1,1,0,2},{3,3,2,1,1,2},
            {4,0,0,1,0,1},{4,0,1,1,1,1},{4,0,2,1,2,1},{4,1,0,3,0,1},{4,1,1,1,0,2},{4,1,2,1,1,2},
            {4,2,0,2,0,1},{4,2,1,2,1,1},{4,2,2,2,2,1},{4,3,0,3,0,1},{4,3,1,1,0,2},{4,3,2,1,1,2}};
        for (const auto& test : cases) {
            state = {}; state.random = HighWordLcg32(19,0x08088405,1); state.memory[0x4a32] = 1;
            state.memory[0x4a21] = 0x5a;
            host = {}; host.actions = {test[1],0};
            assert(invoke(bytes(test[0],test[2]),ops,binding).exited);
            assert(state.memory[0x4a20] == test[3] && state.memory[0x4a31] == test[4]);
            assert(state.memory[0x4a30] == test[2] && state.memory[0x4a21] == 0x5a);
            assert(host.sprites.size() == test[5] && host.sprites.back().distance == test[4]);
            assert(host.sprites.front().sprite == 7 && host.sprites.front().replacement_picture == 8);
            assert(state.random->state() == 19 && state.comparison == -1);
            assert(host.fourth_choices.front() == (test[2] ? "ADVANCE" : "PARLAY"));
        }
        // Native descriptions start at current distance and wrap through the other two.
        const char expected[][4]{{'A','B','C','A'},{'A','B','C','B'},{'A','B','C','C'}};
        for (unsigned distance = 0; distance < 3; ++distance) {
            unsigned index = 0;
            for (const auto mask : {1u,2u,4u,7u}) {
                host = {};
                assert(invoke(bytes(0,distance,mask),ops,binding).exited);
                assert(host.output.front() == std::string(1,expected[distance][index++]));
            }
        }
        host = {}; host.visible = 1;
        assert(invoke(bytes(0,2),ops,binding).exited && state.memory[0x4a31] == 1);
        state.memory[0x4a32] = 0; host = {};
        assert(invoke(bytes(0,2),ops,binding).exited && host.fourth_choices.front() == "PARLAY");
        state.memory[0x4a32] = 1; host = {}; host.rates = {5,11}; host.actions = {2};
        assert(invoke(bytes(0,2),ops,binding).exited && state.memory[0x4a20] == 1);
        host = {}; host.rates = {5,11};
        assert(invoke(bytes(2,2),ops,binding).exited && state.memory[0x4a20] == 0);
        host = {}; host.movement_available = false; host.actions = {2};
        assert(invoke(bytes(0,2),ops,binding).diagnostic == "Current party movement and effects unavailable");
        host = {}; host.endless_wait = true;
        assert(invoke(bytes(1,2),ops,binding,{8,64}).diagnostic == "ECL step limit exceeded");
        host = {}; host.actions = {4};
        assert(!invoke(bytes(0,2),ops,binding).exited);
        host = {}; host.visible = 3;
        assert(!invoke(bytes(0,2),ops,binding).exited);
        host = {};
        assert(!invoke(bytes(5,2),ops,binding).exited);
        assert(!invoke(bytes(0,3),ops,binding).exited);
        assert(!invoke(bytes(0,2),ops,std::nullopt).exited);
        assert(!invoke(bytes(0,2),minimal_vm_opcodes(),binding).exited);
        assert(!invoke(bytes(0,2),ops,EclEncounterBinding{}).exited);
        auto conflict = ops; conflict.push_back({0x29,"INPUT_RETURN_29",0});
        assert(!invoke(bytes(0,2),conflict,binding).exited);
        conflict = ops;
        for (auto& op : conflict) if (op.opcode == 0x29) op.operand_count = 13;
        assert(!invoke(bytes(0,2),conflict,binding).exited);
    }
    {
        // Golden observations from the unmodified native handler/dice helper/RNG.
        struct Case { std::array<unsigned,4> args; std::uint32_t seed, result, final_seed; };
        const Case cases[]{
            {{0,0,0,0},0,1,134775814}, {{0,0,0,255},19,0,3414851041u},
            {{0,6,0,6},0x12345678,2,2046801342}, // Both checks succeed; second wins.
            {{0,128,0,128},0xffffffff,0,866343917}, // Signed thresholds wrap below zero.
            {{6,0,0,0},0xffffffff,2,866343917}, {{6,6,6,6},1,0,3698175007u},
            {{128,0,0,0},10,0,1409029376}, {{255,255,255,255},19,1,3414851041u},
            {{4,12,20,28},10,1,1409029376}, {{127,125,123,121},0x12345678,2,2046801342},
            {{144,176,208,240},0xffffffff,1,866343917}, {{244,220,196,172},10,2,1409029376}};
        EclState state; Host host;
        const auto ops = minimal_vm_opcodes(EclImageDialect::disabled,EclAreaDialect::disabled,
            EclPartyDialect::disabled,EclPartyCheckDialect::disabled,EclSurpriseDialect::signed_threshold2);
        const EclSurpriseBinding binding{EclSurpriseDialect::signed_threshold2,0x4a20};
        const auto invoke = [&](const auto& bytes, const auto& selected, std::optional<EclSurpriseBinding> adapter) {
            return execute_ecl(bytes,EclEventKind::move,{"ssi-five-entry-v1",0x9900,0xc04f},selected,state,host,{},
                EclRandomDialect::disabled,EclArithmeticDialect::disabled,adapter);
        };
        for (const auto& test : cases) for (const auto type : {0,1,2,3,4,5}) {
            auto bytes = program({3,0,1,0,2,0x23});
            for (std::size_t i = 0; i < test.args.size(); ++i) {
                const auto address = 0x4b00+2*i;
                state.memory[address] = test.args[i]; state.memory[address+1] = 0xa5;
                bytes.push_back(type);
                bytes.push_back(type == 1 || type == 3 || type == 5 ? address&255 : test.args[i]);
                if (type != 0) bytes.push_back(type == 1 || type == 3 || type == 5 ? address>>8 : 0xa5);
                if (type == 4) bytes.insert(bytes.end(),{0x12,0x34});
            }
            bytes.push_back(0);
            state.random = HighWordLcg32(test.seed,0x08088405,1);
            state.memory[0x4a20] = 0xff; state.memory[0x4a21] = 0x5a;
            assert(invoke(bytes,ops,binding).exited);
            assert(state.memory[0x4a20] == test.result && state.memory[0x4a21] == 0x5a);
            assert(state.random->state() == test.final_seed && state.comparison == -1);
        }
        const auto valid = program({0x23,0,0,0,6,0,0,0,6,0});
        const auto seed = state.random->state();
        assert(!invoke(valid,ops,std::nullopt).exited && state.random->state() == seed);
        assert(!invoke(valid,minimal_vm_opcodes(),binding).exited && state.random->state() == seed);
        assert(!invoke(program({0x23,0x80,0,0,6,0,0,0,6,0}),ops,binding).exited);
        assert(state.random->state() == seed);
        assert(!invoke(valid,ops,EclSurpriseBinding{EclSurpriseDialect::signed_threshold2,0x9914}).exited);
        assert(state.random->state() == seed); // Protected result address does not consume the stream.
        auto conflict = ops; conflict.push_back({0x23,"OTHER",4});
        assert(!invoke(valid,conflict,binding).exited);
        conflict = ops;
        for (auto& spec : conflict) if (spec.opcode == 0x23) spec.operand_count = 3;
        assert(!invoke(valid,conflict,binding).exited);
        conflict = ops;
        for (auto& spec : conflict) if (spec.opcode == 0x23) spec.mnemonic = "STOP_MOVE_23";
        assert(!invoke(valid,conflict,binding).exited);
        assert(!invoke(valid,ops,EclSurpriseBinding{}).exited);
        state.random.reset();
        assert(!invoke(valid,ops,binding).exited);
    }
    {
        struct ClassHost : Host {
            bool present{}; unsigned queries{};
            bool party_class_present() override { ++queries; return present; }
        } selected;
        EclState state;
        const auto ops = minimal_vm_opcodes(EclImageDialect::disabled,EclAreaDialect::disabled,
            EclPartyDialect::disabled,EclPartyCheckDialect::class_presence2);
        const auto invoke = [&](const auto& bytes, const auto& dialect) {
            return execute_ecl(bytes,EclEventKind::move,{"ssi-five-entry-v1",0x9900,0xc04f},dialect,state,selected);
        };
        for (const auto present : {false,true}) {
            selected.present = present;
            state.memory[0x4a21] = 0xaa; state.memory[0x4a22] = 0xbb;
            assert(invoke(program({3,0,1,0,2,0x22,3,0x20,0x4a,1,0x22,0x4a,0}),ops).exited);
            assert(state.memory[0x4a20] == present && state.memory[0x4a21] == 0 && state.memory[0x4a22] == 0);
            assert(state.comparison == -1);
        }
        assert(invoke(program({0x22,1,0x20,0x4a,1,0x20,0x4a,0}),ops).exited);
        assert(state.memory[0x4a20] == 0); // The second output wins on aliased destinations.
        const auto queries = selected.queries;
        assert(!invoke(program({0x22,0,7,1,0x20,0x4a,0}),ops).exited && selected.queries == queries);
        assert(!invoke(program({0x22,1,0x20,0x4a,0,7,0}),ops).exited && selected.queries == queries);
        assert(!invoke(program({0x22,1,0x20,0x4a,1,0x21,0x4a,0}),minimal_vm_opcodes()).exited);
        auto conflict = ops;
        conflict.push_back({0x22,"INPUT_YES_NO",0});
        assert(!invoke(program({0}),conflict).exited);
        conflict = ops;
        for (auto& spec : conflict) if (spec.opcode == 0x22) spec.mnemonic = "PARTY_SKILL_CHECK3";
        assert(!invoke(program({0}),conflict).exited);
    }
    {
        // Expected results independently checked against the original unsigned handler.
        const unsigned cases[][5]{{0,1,1,0,0},{1,2,1,0,2},{17,5,65524,3,85},
            {255,2,65283,127,510},{256,3,65283,85,768},{32767,7,32776,4681,32761},
            {32768,255,33023,128,32768},{65535,1,2,65535,65535},
            {65535,255,256,257,65281},{65535,65535,0,1,1}};
        EclState arithmetic_state; Host arithmetic_host;
        const auto invoke = [&](const auto& bytes, const auto& ops, EclArithmeticDialect dialect) {
            return execute_ecl(bytes,EclEventKind::move,{"ssi-five-entry-v1",0x9900,0xc04f},
                ops,arithmetic_state,arithmetic_host,{},EclRandomDialect::disabled,dialect);
        };
        const auto ops = minimal_vm_opcodes();
        for (const auto& values : cases) for (unsigned opcode = 5; opcode <= 7; ++opcode)
            for (const auto type : {1,3}) {
                auto bytes = program({3,0,1,0,2}); // Establish comparison within this invocation.
                bytes.insert(bytes.end(),{static_cast<std::uint8_t>(opcode),2,
                    static_cast<std::uint8_t>(values[0]),static_cast<std::uint8_t>(values[0]>>8),2,
                    static_cast<std::uint8_t>(values[1]),static_cast<std::uint8_t>(values[1]>>8),
                    static_cast<std::uint8_t>(type),0x20,0x4a,0});
                arithmetic_state.comparison = -1; arithmetic_state.memory[0x6e3f] = 0xa5;
                arithmetic_state.memory[0x4a21] = 0xcc;
                assert(invoke(bytes,ops,EclArithmeticDialect::unsigned16).exited);
                const auto expected = values[opcode-3];
                assert(arithmetic_state.memory[0x4a20] == (expected&255));
                assert(arithmetic_state.memory[0x4a21] == (type == 3 ? expected>>8 : 0xcc));
                assert(arithmetic_state.memory[0x6e3f] == 0xa5 && arithmetic_state.comparison == -1);
            }
        const auto valid = program({6,0,17,0,5,1,0x20,0x4a,0});
        assert(!invoke(valid,ops,EclArithmeticDialect::disabled).exited);
        arithmetic_state.memory[0x4a20] = 99;
        assert(!invoke(program({6,0,17,0,0,1,0x20,0x4a,0}),ops,EclArithmeticDialect::unsigned16).exited);
        assert(arithmetic_state.memory[0x4a20] == 99);
        assert(!invoke(program({6,0,17,0,5,0,20,0}),ops,EclArithmeticDialect::unsigned16).exited);
        assert(!invoke(program({6,0,17,0,5,1,0x14,0x99,0}),ops,EclArithmeticDialect::unsigned16).exited);
        assert(!invoke(program({6,0,17,0,5,3,0xff,0xff,0}),ops,EclArithmeticDialect::unsigned16).exited);
        auto conflict = ops;
        for (auto& spec : conflict) if (spec.opcode == 6) spec.operand_count = 2;
        assert(!invoke(valid,conflict,EclArithmeticDialect::unsigned16).exited);
    }
    {
        struct StrengthHost : Host {
            unsigned queries{};
            std::uint8_t party_strength() override { ++queries; return 42; }
        } rating_host;
        EclState rating_state;
        rating_state.memory[0x4a21] = 255;
        auto ops = minimal_vm_opcodes(EclImageDialect::disabled,EclAreaDialect::disabled,EclPartyDialect::weighted_strength);
        const auto invoke = [&](const auto& bytes, const auto& selected) {
            return execute_ecl(bytes,EclEventKind::move,{"ssi-five-entry-v1",0x9900,0xc04f},
                selected,rating_state,rating_host);
        };
        assert(invoke(program({0x1d,3,0x20,0x4a,0}),ops).exited);
        assert(rating_state.memory[0x4a20] == 42 && rating_state.memory[0x4a21] == 0 && rating_host.queries == 1);
        assert(!invoke(program({0x1d,0,0,0}),ops).exited && rating_host.queries == 1);
        assert(!invoke(program({0x1d,1,0x20,0x4a,0}),minimal_vm_opcodes()).exited);
        auto conflict = ops; conflict.push_back({0x1d,"OTHER",1});
        assert(!invoke(program({0x1d,1,0x20,0x4a,0}),conflict).exited);
        for (auto& spec : ops) if (spec.opcode == 0x1d) spec.operand_count = 0;
        assert(!invoke(program({0x1d,0}),ops).exited);
    }
    {
        EclState transfer_state; Host transfer_host;
        const auto transfer = run(program({0x20,0,7,9,0,1,1,0,0x4a,0}),transfer_state,transfer_host);
        assert(transfer.exited && transfer.next_program == 7 && transfer_state.memory[0x4a00] == 0);
        assert(!run(program({0x20,2,0,1}),transfer_state,transfer_host).exited);
    }
    EclState state;
    Host host;
    assert(run(program({4,0,255,0,2,1,0,0x4a,0x3a,0}),state,host).exited);
    assert(state.memory[0x4a00] == 1 && host.delays == 1);
    assert(run(program({4,2,255,255,0,2,3,0,0x4a,0}),state,host).exited);
    assert(state.memory[0x4a00] == 1 && state.memory[0x4a01] == 0);
    assert(!run(program({4,0,1,0,2,0,0,0}),state,host).exited);
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
