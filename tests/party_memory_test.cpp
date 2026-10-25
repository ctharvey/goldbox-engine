// SPDX-License-Identifier: GPL-3.0-or-later
#include "goldbox/game_profile.hpp"
#include "goldbox/exploration.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <functional>

using namespace goldbox;
using namespace goldbox::formats;
namespace {
struct Host : EclHost {
    std::string output;
    std::vector<std::string> choices;
    std::string prompt;
    std::size_t selected{}, menus{};
    void text(std::string_view value, bool) override { output += value; }
    std::size_t menu(std::span<const std::string> values, std::string_view description, bool vertical) override {
        assert(vertical); choices.assign(values.begin(),values.end()); prompt = description; ++menus; return selected;
    }
};
void rejects(const std::function<void()>& action) {
    bool failed = false;
    try { action(); } catch (const std::exception&) { failed = true; }
    assert(failed);
}
std::vector<std::uint8_t> program(std::initializer_list<std::uint8_t> body) {
    std::vector<std::uint8_t> bytes;
    for (int i = 0; i < 5; ++i) bytes.insert(bytes.end(),{1,1,20,0x99});
    bytes.insert(bytes.end(),body); return bytes;
}
}
int main(int argc, char** argv) {
    assert(argc == 2);
    const auto profile = read_game_profile(argv[1]);
    assert(profile.character_rules && profile.party_memory);
    const auto& rules = *profile.character_rules;
    const auto& layout = *profile.party_memory;
    assert(layout.record_base == 0x6b00 && layout.record_size == 0x1ff && layout.count_address == 0x6e3e);
    ExplorationSession session;
    session.character_rules = rules; session.party_memory = layout;
    Character ada{"Ada","human","female","cleric","lawful-good",std::vector<AbilityValue>(6,{15,0})};
    auto borin = ada; borin.name = "Borin"; borin.abilities[0].value = 17; borin.abilities[1].value = 14;
    add_character(session.party,rules,ada); add_character(session.party,rules,borin);
    session.dialect = {"ssi-five-entry-v1",0x9900,0xc04f};
    session.opcodes = minimal_vm_opcodes(EclImageDialect::disabled,EclAreaDialect::disabled,EclPartyDialect::load_character);
    Host host;
    const auto run = [&](std::initializer_list<std::uint8_t> bytes) {
        session.ecl_record = program(bytes); return session.trigger(EclEventKind::move,host);
    };
    session.sound_call = EclSoundCall{0xba03,0x6b00,{{8,10}}};
    assert(!run({9,0,8,1,0,0x6b,0}).exited);
    assert(session.state.memory[0x6b00] == 0 && session.party[0].name == "Ada");
    session.sound_call->selector_address = 0x6aff;
    assert(!run({9,0,8,1,0xff,0x6a,0}).exited);
    assert(session.state.memory[0x6aff] == 0);
    session.sound_call.reset();
    assert(layout.levels.size() == 4);
    assert(!run({9,1,0x96,0x6b,1,0x10,0x4a,0}).exited);
    rejects([&] { (void)party_memory_byte(layout,rules,session.party,0,0x6b96); });
    session.party[0].levels = {{"cleric",1}};
    session.party[1].levels = {{"cleric",3}};
    assert(run({9,1,0x96,0x6b,1,0x10,0x4a,9,1,0x9b,0x6b,1,0x11,0x4a,0}).exited);
    assert(session.state.memory[0x4a10] == 1 && session.state.memory[0x4a11] == 0);
    assert(run({0x0a,0,1,9,1,0x96,0x6b,1,0x10,0x4a,0}).exited && session.state.memory[0x4a10] == 3);
    session.party[1].levels["cleric"] = 256;
    assert(!run({9,1,0x96,0x6b,1,0x10,0x4a,0}).exited);
    session.party[1].levels["cleric"] = 3;
    assert(!run({9,0,2,1,0x96,0x6b,0}).exited && session.party[1].levels.at("cleric") == 3);
    for (int i = 0; i < 5; ++i) {
        auto bad = layout;
        if (i == 0) bad.levels.push_back(bad.levels.front());
        if (i == 1) bad.levels[0].offset = bad.abilities[0].offset;
        if (i == 2) bad.levels[0].character_class = "unknown";
        if (i == 3) bad.levels[0].character_class = "cleric-fighter";
        if (i == 4) bad.levels[0].offset = bad.record_size;
        rejects([&] { validate_party_memory_layout(bad,rules); });
    }
    assert(run({0x0a,0,0,0}).exited);
    session.state.memory[0x6e3e] = 255;
    assert(run({9,1,0x3e,0x6e,1,0,0x4a,0x0a,0,1,0x11,0x81,0,0x6b,
        9,1,0x10,0x6b,1,1,0x4a,9,3,0x10,0x6b,3,2,0x4a,
        0x2a,1,0,0x6b,0,0x11,1,4,0x4a,9,0x81,0,0x6b,0x81,0,0x4b,0x11,0x81,0,0x4b,0}).exited);
    assert(host.output == "BorinBorin" && session.loaded_character == 1);
    assert(session.state.memory[0x4a00] == 2 && session.state.memory[0x4a01] == 17);
    assert(session.state.memory[0x4a02] == 17 && session.state.memory[0x4a03] == 14);
    assert(session.state.memory[0x4a04] == 14 && session.state.memory[0x6db1] == 0);
    assert(layout.status && layout.status->offset == 256);
    assert(!run({9,1,0,0x6c,1,5,0x4a,0}).exited); // Unknown legacy state cannot become healthy silently.
    session.party[0].status = "okay"; session.party[1].status = "unconscious";
    assert(run({9,1,0,0x6c,1,5,0x4a,0}).exited && session.state.memory[0x4a05] == 0);
    assert(run({0x0a,0,0,9,1,0,0x6c,1,5,0x4a,0}).exited && session.state.memory[0x4a05] == 1);
    session.party[0].status = "other-condition";
    assert(!run({9,1,0,0x6c,1,5,0x4a,0}).exited);
    session.party[0].status = "okay";
    assert(!run({9,0,135,1,0,0x6c,0}).exited && session.party[0].status == "okay");
    assert(run({0x0a,0,1,0}).exited);
    assert(layout.selected_address == 0x6db1);
    host.selected = 1; host.output.clear();
    assert(run({0x39,0x80,1,4,0x11,0x81,0,0x6b,0}).exited);
    assert(host.choices == std::vector<std::string>({"Ada","Borin"}) && host.prompt == "A");
    assert(host.output == "Borin" && session.loaded_character == 1 && session.state.memory[0x6db1] == 1);
    host.output.clear();
    assert(run({0x0a,0,0,0x11,0x81,0,0x6b,0}).exited && host.output == "Ada");
    assert(session.loaded_character == 0 && session.state.memory[0x6db1] == 1);
    host.selected = 2;
    assert(!run({0x39,0x80,1,4,0}).exited);
    assert(session.loaded_character == 0 && session.state.memory[0x6db1] == 1);
    const auto menus = host.menus;
    assert(!run({0x39,0,1,0}).exited && host.menus == menus); // Numeric prompt is invalid.
    session.party_memory->selected_address.reset();
    assert(!run({0x39,0x80,1,4,0}).exited && host.menus == menus);
    session.party_memory = layout;
    assert(run({0x0a,0,1,0}).exited);
    host.output.clear(); assert(run({0x11,0x81,0x10,0x6b,0}).exited && host.output == "Borin");
    assert(!run({0x0a,0,2,0}).exited && session.loaded_character == 1);
    assert(!run({0x0a,2,0,1,0}).exited && session.loaded_character == 1);
    session.state.memory[0x6b32] = 99;
    assert(!run({9,1,0x32,0x6b,1,0,0x4a,0}).exited); // Unknown fields cannot fall back to RAM.
    assert(!run({9,1,0xfe,0x6c,1,0,0x4a,0}).exited);
    session.state.memory[0x6cff] = 42;
    assert(run({9,1,0xff,0x6c,1,0,0x4a,0}).exited && session.state.memory[0x4a00] == 42);
    const auto party_before = serialize_party(session.party,rules);
    session.state.memory[0x6aff] = 77; session.state.memory[0x6e3d] = 88;
    assert(!run({9,0,1,1,0x10,0x6b,0}).exited);
    assert(!run({9,2,1,1,3,0xff,0x6a,0}).exited); // A crossing word write is rejected atomically.
    assert(!run({9,2,1,1,3,0x3d,0x6e,0}).exited);
    assert(!run({9,0x81,0,0x4b,0x81,0,0x6b,0}).exited);
    assert(session.state.memory[0x6aff] == 77 && session.state.memory[0x6e3d] == 88);
    assert(serialize_party(session.party,rules) == party_before);
    const auto save = serialize_exploration(session,"synthetic-party");
    auto restored = session; restored.loaded_character = 0;
    restore_exploration(restored,save,"synthetic-party"); assert(restored.loaded_character == 1);
    assert(restored.state.memory[0x6db1] == 1);
    auto invalid = save; auto at = invalid.find("\"loadedCharacter\":1"); assert(at != std::string::npos);
    invalid.replace(at,19,"\"loadedCharacter\":2");
    rejects([&] { restore_exploration(restored,invalid,"synthetic-party"); });
    assert(serialize_exploration(restored,"synthetic-party") == save);
    auto legacy = session; legacy.party_memory.reset(); legacy.loaded_character = 0;
    rejects([&] { restore_exploration(legacy,save,"synthetic-party"); });
    restore_exploration(restored,serialize_exploration(legacy,"synthetic-party"),"synthetic-party");
    assert(restored.loaded_character == 0);
    session.party.clear(); session.loaded_character = 0;
    const auto empty_menus = host.menus;
    assert(!run({0x39,0x80,1,4,0}).exited && host.menus == empty_menus);
    assert(run({9,1,0x3e,0x6e,1,0,0x4a,0}).exited && session.state.memory[0x4a00] == 0);
    assert(!run({0x0a,0,0,0}).exited && !run({0x11,0x81,0,0x6b,0}).exited);
    auto other = layout; other.record_base = 0x5000; other.record_size = 256; other.count_address = 0x4fff;
    other.selected_address = 0x4ffe;
    other.status = PartyStatusBinding{0x45,{{"okay",7},{"unconscious",9}}};
    other.abilities = {{0x44,"intelligence",false}};
    validate_party_memory_layout(other,rules);
    assert(party_memory_byte(other,rules,Party{borin},0,0x5044) == 14);
    borin.status = "unconscious";
    assert(party_memory_byte(other,rules,Party{borin},0,0x5045) == 9);
    auto bad_status = layout; bad_status.status->offset = bad_status.abilities.front().offset;
    rejects([&] { validate_party_memory_layout(bad_status,rules); });
    bad_status = layout; bad_status.status->values.clear();
    rejects([&] { validate_party_memory_layout(bad_status,rules); });
    bad_status = layout; bad_status.status->values.emplace("Invalid ID",1);
    rejects([&] { validate_party_memory_layout(bad_status,rules); });
    auto alternate = session; alternate.party = {ada,borin}; alternate.party_memory = other;
    alternate.ecl_record = program({0x39,0x80,1,4,0}); host.selected = 1;
    assert(alternate.trigger(EclEventKind::move,host).exited && alternate.loaded_character == 1);
    assert(alternate.state.memory[0x4ffe] == 1);
    auto collision = layout; collision.selected_address = layout.count_address;
    rejects([&] { validate_party_memory_layout(collision,rules); });
    collision.selected_address = layout.record_base;
    rejects([&] { validate_party_memory_layout(collision,rules); });
    for (int i = 0; i < 6; ++i) {
        auto bad = layout;
        if (i == 0) bad.dialect = "unknown";
        if (i == 1) bad.record_base = 0xffff;
        if (i == 2) bad.count_address = bad.record_base;
        if (i == 3) bad.abilities.push_back(bad.abilities.front());
        if (i == 4) bad.abilities[0].ability = "unknown";
        if (i == 5) bad.abilities[1].percentile = true;
        rejects([&] { validate_party_memory_layout(bad,rules); });
    }
    session.party_memory->count_address = 0xc04f;
    assert(!run({0}).exited);
    session.party_memory = layout; session.party_memory->record_base = 0x9900;
    assert(!run({0}).exited);
    EclState state; const auto bytes = program({0x0a,0,0,0});
    const auto execute = [&](const auto& ops) { return execute_ecl(bytes,EclEventKind::move,session.dialect,ops,state,host); };
    assert(!execute(minimal_vm_opcodes()).exited);
    assert(!execute(session.opcodes).exited); // Missing host adapter.
    auto conflicting = session.opcodes; conflicting.push_back({0x0a,"CONFLICT",1});
    assert(!execute(conflicting).exited);
    conflicting = session.opcodes;
    for (auto& op : conflicting) if (op.opcode == 0x0a) op.operand_count = 2;
    assert(!execute(conflicting).exited);
    const auto who = program({0x39,0x80,1,4,0});
    assert(!execute_ecl(who,EclEventKind::move,session.dialect,minimal_vm_opcodes(),state,host).exited);
    assert(!execute_ecl(who,EclEventKind::move,session.dialect,session.opcodes,state,host).exited);
    conflicting = session.opcodes; conflicting.push_back({0x39,"CONFLICT",1});
    assert(!execute_ecl(who,EclEventKind::move,session.dialect,conflicting,state,host).exited);
    conflicting = session.opcodes;
    for (auto& op : conflicting) if (op.opcode == 0x39) op.operand_count = 2;
    assert(!execute_ecl(who,EclEventKind::move,session.dialect,conflicting,state,host).exited);
}
