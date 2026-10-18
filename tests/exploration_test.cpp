// SPDX-License-Identifier: GPL-3.0-or-later
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
    std::vector<std::uint16_t> sounds;
    void sound(std::uint16_t id) override { sounds.push_back(id); }
    void text(std::string_view value, bool) override { output += value; }
};
void rejects(const std::function<void()>& fn) {
    bool failed = false;
    try { fn(); } catch (const std::exception&) { failed = true; }
    assert(failed);
}
} // namespace

int main() {
    {
        const auto script = [](std::initializer_list<std::uint8_t> body) {
            std::vector<std::uint8_t> bytes;
            for (int i = 0; i < 5; ++i) bytes.insert(bytes.end(),{1,1,20,0x99});
            bytes.insert(bytes.end(),body); return bytes;
        };
        const auto boot = script({0x20,0,42});
        const auto destination = script({9,0,3,1,0,0x4a,0x11,0x80,1,4,0});
        ExplorationSession campaign;
        campaign.dialect = {"ssi-five-entry-v1",0x9900,0xc04f,true,
            std::array<std::uint16_t,3>{0xc04b,0xc04c,0xc04d}};
        campaign.opcodes = minimal_vm_opcodes(); campaign.ecl_record = boot;
        campaign.current_program_record = 0;
        campaign.programs = ProgramResources{[&](std::uint8_t id) {
            if (id != 0 && id != 42) throw std::runtime_error("Missing synthetic program");
            return ProgramResource{id,id == 0 ? boot : destination};
        },"synthetic-programs-v1"};
        campaign.campaign_start = std::array<std::uint8_t,2>{0,0};
        campaign.maps = MapResources{[](std::uint8_t id) { GeoMap map; map.dax_record_id = id; return map; },
            "synthetic-maps-v1"};
        Host begin_output;
        rejects([&] { campaign.begin_campaign(begin_output); });
        campaign.party.emplace_back(); campaign.party[0].name = "Synthetic";
        campaign.state.random = HighWordLcg32(123,134775813,1);
        campaign.state.memory[0x4a01] = 99;
        auto begun = campaign;
        assert(begun.begin_campaign(begin_output).exited && begin_output.output == "AAA");
        assert(begun.state.memory[0x4a01] == 0 && begun.party[0].name == "Synthetic");
        assert(begun.state.random->state() == 123);
        campaign.party.clear(); campaign.state.random.reset();
        auto initial = campaign; Host output;
        const auto run = campaign.trigger(EclEventKind::initialize,output);
        assert(run.exited && run.steps == 14 && !run.next_program && output.output == "AAA");
        assert(campaign.current_program_record == 42 && campaign.state.memory[0x4a00] == 3);
        const auto saved = serialize_exploration(campaign,"synthetic-campaign");
        restore_exploration(initial,saved,"synthetic-campaign");
        assert(initial.current_program_record == 42 && initial.ecl_record == destination);
        assert(initial.state.memory == campaign.state.memory);
        auto invalid = saved;
        const auto location = invalid.find("synthetic-programs-v1");
        assert(location != std::string::npos);
        invalid.replace(location,21,"synthetic-programs-v2");
        rejects([&] { restore_exploration(initial,invalid,"synthetic-campaign"); });
        assert(initial.current_program_record == 42 && initial.ecl_record == destination);
        campaign.ecl_record = boot; campaign.current_program_record = 0;
        auto missing = campaign; missing.programs.reset();
        assert(!missing.trigger(EclEventKind::initialize,output).exited && missing.current_program_record == 0);
        auto wrong = campaign;
        wrong.programs->resolve = [&](std::uint8_t) { return ProgramResource{43,destination}; };
        assert(!wrong.trigger(EclEventKind::initialize,output).exited && wrong.ecl_record == boot);
        auto malformed = campaign;
        malformed.programs->resolve = [](std::uint8_t id) { return ProgramResource{id,{0xff}}; };
        assert(!malformed.trigger(EclEventKind::initialize,output).exited && malformed.ecl_record == boot);
        auto endless = campaign;
        endless.programs->resolve = [&](std::uint8_t id) { return ProgramResource{id,boot}; };
        assert(!endless.trigger(EclEventKind::initialize,output).exited);
        assert(!campaign.trigger(EclEventKind::initialize,output,{5,64}).exited);
        campaign.programs.reset(); campaign.current_program_record.reset();
        campaign.position_refresh_call = 0x2c90;
        campaign.map.cells[31].raw_event = 0x82;
        campaign.ecl_record = script({9,0,15,1,0x4b,0xc0,9,0,1,1,0x4c,0xc0,
            9,0,3,1,0x4d,0xc0,0x2d,1,0x90,0x2c,0});
        assert(campaign.trigger(EclEventKind::initialize,output).exited);
        assert(campaign.row == 1 && campaign.column == 15 && campaign.facing == Direction::west);
        assert(campaign.state.memory[0xc04f] == 0x82);
        campaign.sound_call = EclSoundCall{0x2c90,0x03de,{}};
        assert(!campaign.trigger(EclEventKind::initialize,output).exited);
    }
    {
        ExplorationSession audio;
        audio.dialect = {"ssi-five-entry-v1",0x9900,0xc04f};
        audio.opcodes = minimal_vm_opcodes();
        for (int i = 0; i < 5; ++i) audio.ecl_record.insert(audio.ecl_record.end(),{1,1,20,0x99});
        audio.ecl_record.insert(audio.ecl_record.end(),{0x2d,1,3,0xba,0});
        audio.sound_call = EclSoundCall{0xba03,0x03de,{{8,10},{10,11}}};
        Host sink;
        for (const std::uint16_t selector : {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,264,266}) {
            auto script = std::vector<std::uint8_t>(audio.ecl_record.begin(),audio.ecl_record.begin()+20);
            script.insert(script.end(),{9,2,static_cast<std::uint8_t>(selector),
                static_cast<std::uint8_t>(selector >> 8),1,0xde,3,0x2d,1,3,0xba,0});
            audio.ecl_record = script;
            sink.sounds.clear();
            assert(audio.trigger(EclEventKind::move,sink).exited);
            assert(audio.state.memory[0x03de] == (selector & 255));
            assert(audio.state.memory[0x03df] == (selector >> 8));
            assert(sink.sounds == (selector == 8 ? std::vector<std::uint16_t>{10} :
                selector == 10 ? std::vector<std::uint16_t>{11} : std::vector<std::uint16_t>{}));
        }
        // A byte WRITE clears the previous high byte of this native word register.
        audio.ecl_record.resize(20);
        audio.ecl_record.insert(audio.ecl_record.end(),{9,0,8,1,0xde,3,0x2d,1,3,0xba,0});
        assert(audio.trigger(EclEventKind::move,sink).exited && sink.sounds.back() == 10);
        assert(audio.state.memory[0x03df] == 0);
        const auto saved = serialize_exploration(audio,"synthetic-audio");
        audio.state.memory[0x03de] = 0;
        restore_exploration(audio,saved,"synthetic-audio");
        assert(audio.state.memory[0x03de] == 8);
        audio.ecl_record.resize(20);
        audio.ecl_record.insert(audio.ecl_record.end(),{0x2d,1,3,0xba,0});
        audio.sound_call->function = 0xba04;
        assert(!audio.trigger(EclEventKind::move,sink).exited);
        audio.sound_call->function = 0xba03;
        audio.sound_call->selector_address = 65535;
        assert(!audio.trigger(EclEventKind::move,sink).exited);
        audio.sound_call->selector_address = 0x03de;
        audio.ecl_record.resize(20);
        audio.ecl_record.insert(audio.ecl_record.end(),{9,0,0,1,0xdf,3,0});
        assert(!audio.trigger(EclEventKind::move,sink).exited);

    }
    ExplorationSession session;
    session.map = decode_geo16_record(std::vector<std::uint8_t>(1026), 7);
    session.map.cells[1].raw_event = 0x81;
    session.dialect = {"ssi-five-entry-v1",0x9900,0xc04f,true, std::array<std::uint16_t,3>{0xc04b,0xc04c,0xc04d}};
    session.opcodes = minimal_vm_opcodes();
    for (int i = 0; i < 5; ++i) session.ecl_record.insert(session.ecl_record.end(), {1,1,20,0x99});
    session.ecl_record.insert(session.ecl_record.end(), {9,1,0x4f,0xc0,1,0,0x4a,0x11,0x80,1,4,0});
    Host host;
    assert(!session.move(Direction::north));
    assert(session.move(Direction::east));
    assert(session.trigger(EclEventKind::move, host).exited);
    assert(host.output == "A" && session.state.memory[0x4a00] == 0x81);
    assert(session.state.memory[0xc04b] == 1 && session.state.memory[0xc04c] == 0 && session.state.memory[0xc04d] == 1);
    session.map.cells[1].south = 2;
    assert(!session.move(Direction::south));
    session.map.cells[2].west = 1;
    assert(!session.move(Direction::east)); // opposing wall blocks inconsistent map cells
    const auto identity = "synthetic.game:GEO1:7:ECL1:3:synthetic-fingerprint";
    session.state.strings[0x4b00] = "Synthetic flag description";
    const auto save = serialize_exploration(session, identity);
    auto restored = session;
    restored.state = {};
    restored.row = 4;
    restored.column = 5;
    restore_exploration(restored, save, identity);
    assert(restored.row == session.row && restored.column == session.column && restored.facing == session.facing);
    assert(restored.state.memory == session.state.memory && restored.state.strings == session.state.strings);
    session.wall_banks = std::array<std::uint8_t,3>{7,127,255};
    const auto banked = serialize_exploration(session,identity);
    restore_exploration(restored,banked,identity);
    assert(restored.wall_banks == session.wall_banks);
    auto bad_banks = save;
    bad_banks.insert(bad_banks.size()-1,",\"wallBanks\":[7,256,255]");
    rejects([&] { restore_exploration(restored,bad_banks,identity); });
    assert(restored.wall_banks == session.wall_banks);
    restore_exploration(restored,save,identity);
    assert(!restored.wall_banks); // Existing saves remain readable by the engine.
    rejects([&] { restore_exploration(restored, save, "different-source"); });
    assert(restored.state.memory == session.state.memory); // invalid loads publish no partial state
    auto invalid = save;
    const auto coordinate = invalid.find("\"row\":0");
    assert(coordinate != std::string::npos);
    invalid.replace(coordinate, 7, "\"row\":99");
    rejects([&] { restore_exploration(restored, invalid, identity); });
    assert(restored.row == session.row);
    rejects([&] { restore_exploration(restored, "{}", identity); });
    session.ecl_record[20] = 0xff;
    assert(!session.trigger(EclEventKind::move, host).exited);
    assert(session.row == 0 && session.column == 1); // errors retain explicit partial progress
    ExplorationSession loading;
    loading.map.dax_record_id = 7;
    loading.row = 3; loading.column = 4;
    loading.dialect = session.dialect;
    loading.opcodes = minimal_vm_opcodes(EclImageDialect::disabled,EclAreaDialect::map3);
    unsigned lookups = 0;
    loading.maps = MapResources{[&](std::uint8_t id) {
        ++lookups;
        if (id != 0 && id != 7 && id != 8) throw std::runtime_error("Missing synthetic map");
        GeoMap map; map.dax_record_id = id;
        if (id == 8) { map.cells[3*16+4].north = 5; map.cells[3*16+4].raw_event = 0x82; }
        return map;
    },"synthetic-map-archive-v1"};
    const auto legacy = serialize_exploration(loading,identity);
    for (int i = 0; i < 5; ++i) loading.ecl_record.insert(loading.ecl_record.end(),{1,1,20,0x99});
    loading.ecl_record.insert(loading.ecl_record.end(),{0x21,0,8,0,2,0,99,9,0,42,1,0,0x4a,0});
    const auto script = loading.ecl_record;
    assert(loading.trigger(EclEventKind::initialize,host).exited);
    assert(loading.map.dax_record_id == 8 && loading.map.cells[3*16+4].north == 5);
    assert(loading.row == 3 && loading.column == 4 && loading.initial_map_record == 7);
    assert(loading.ecl_record == script && loading.state.memory[0x4a00] == 42);
    assert((loading.area == std::array<std::uint8_t,3>{8,2,99}));
    const auto before_reuse = lookups;
    loading.load_area({255,2,44});
    assert(lookups == before_reuse && loading.map.dax_record_id == 8);
    const auto bound = serialize_exploration(loading,identity);
    restore_exploration(loading,legacy,identity);
    assert(loading.map.dax_record_id == 7 && !loading.area);
    restore_exploration(loading,bound,identity);
    assert(loading.map.dax_record_id == 8 && loading.area);
    const auto good_area = loading.area;
    for (const auto mode : {2,127,255}) loading.load_area({8,static_cast<std::uint8_t>(mode),1});
    rejects([&] { loading.load_area({9,2,0}); });
    rejects([&] { loading.load_area({127,2,0}); });
    rejects([&] { loading.load_area({255,16,0}); });
    assert(loading.map.dax_record_id == 8 && loading.row == 3);
    loading.state.memory[0xc04b] = 255;
    rejects([&] { loading.load_area({7,2,0}); });
    assert(loading.map.dax_record_id == 8 && loading.column == 4);
    loading.state.memory[0xc04b] = 4;
    loading.maps->source_identity = "changed-archive";
    rejects([&] { restore_exploration(loading,bound,identity); });
    assert(loading.map.dax_record_id == 8);
    loading.maps->source_identity = "synthetic-map-archive-v1";
    const auto resolver = *loading.maps;
    loading.maps->resolve = [](std::uint8_t) { GeoMap map; map.dax_record_id = 3; return map; };
    rejects([&] { loading.load_area({8,2,0}); });
    rejects([&] { restore_exploration(loading,bound,identity); });
    loading.maps = resolver;
    loading.load_area({0,0,0});
    assert(loading.map.dax_record_id == 0);
    restore_exploration(loading,bound,identity);
    loading.maps.reset();
    rejects([&] { loading.load_area({8,2,0}); });
    rejects([&] { restore_exploration(loading,bound,identity); });
    assert(loading.map.dax_record_id == 8 && loading.area == good_area);
    auto conflicting = loading.opcodes;
    for (auto& op : conflicting) if (op.opcode == 0x21) op.operand_count = 2;
    assert(!execute_ecl(script,EclEventKind::initialize,loading.dialect,conflicting,loading.state,host).exited);
    conflicting = loading.opcodes; conflicting.push_back({0x21,"OTHER_MAP",3});
    assert(!execute_ecl(script,EclEventKind::initialize,loading.dialect,conflicting,loading.state,host).exited);
    assert(!execute_ecl(script,EclEventKind::initialize,loading.dialect,loading.opcodes,loading.state,host).exited);
    assert(!execute_ecl(script,EclEventKind::initialize,loading.dialect,minimal_vm_opcodes(),loading.state,host).exited);
    auto bad_operand = script; bad_operand[21] = 2; bad_operand.insert(bad_operand.begin()+23,2); // 520, not a byte.
    assert(!execute_ecl(bad_operand,EclEventKind::initialize,loading.dialect,loading.opcodes,loading.state,host).exited);
    loading.maps = resolver;
    for (const auto& change : {std::pair{std::string("\"mapRecord\":8"),std::string("\"mapRecord\":256")},
        std::pair{std::string("\"area\":[255,2,44]"),std::string("\"area\":[255,16,44]")},
        std::pair{std::string("\"area\":[255,2,44]"),std::string("\"area\":[255,2]")}}) {
        auto invalid_binding = bound;
        const auto at = invalid_binding.find(change.first);
        assert(at != std::string::npos);
        invalid_binding.replace(at,change.first.size(),change.second);
        rejects([&] { restore_exploration(loading,invalid_binding,identity); });
        assert(loading.map.dax_record_id == 8 && loading.area == good_area);
    }
    return 0;
}
