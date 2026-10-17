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
    return 0;
}
