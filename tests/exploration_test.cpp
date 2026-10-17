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
    void text(std::string_view value, bool) override { output += value; }
};
void rejects(const std::function<void()>& fn) {
    bool failed = false;
    try { fn(); } catch (const std::exception&) { failed = true; }
    assert(failed);
}
} // namespace

int main() {
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
