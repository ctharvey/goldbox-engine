// SPDX-License-Identifier: GPL-3.0-or-later
// Independently authored synthetic bytecode, not original SSI game data.
#include "goldbox/formats/ecl_dispatch.hpp"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <functional>

using namespace goldbox::formats;
namespace {
void rejects(const std::function<void()>& fn) {
    bool failed = false;
    try { fn(); } catch (const std::exception&) { failed = true; }
    assert(failed);
}

std::vector<std::uint8_t> record() {
    // Five lifecycle GOTOs to offset 20; mask raw square-info into temp byte,
    // then an indexed table. Two targets deliberately skip embedded text data.
    std::vector<std::uint8_t> code;
    for (int i = 0; i < 5; ++i) code.insert(code.end(), {1, 1, 0x14, 0x99});
    code.insert(code.end(), {0x2f, 1, 0x4f, 0xc0, 0, 127, 1, 0, 0x4a}); // AND -> 0x4a00, offset 20
    code.insert(code.end(), {0x25, 1, 0, 0x4a, 0, 2, 1, 0x30, 0x99, 1, 0x31, 0x99}); // table offset 29
    code.resize(48, 0x80); // data, never linearly disassembled
    code.insert(code.end(), {0, 0}); // EXIT offsets 48,49
    return code;
}

DaxArchive archive(const std::vector<std::uint8_t>& raw) {
    std::vector<std::uint8_t> bytes{9,0,7,0,0,0,0,
        static_cast<std::uint8_t>(raw.size()), static_cast<std::uint8_t>(raw.size() >> 8),
        static_cast<std::uint8_t>(raw.size() + 1), 0, static_cast<std::uint8_t>(raw.size() - 1)};
    bytes.insert(bytes.end(), raw.begin(), raw.end());
    return DaxArchive::parse(bytes);
}

std::vector<std::uint8_t> conditional_route(std::uint8_t condition, std::uint8_t right,
    std::optional<std::uint8_t> mask = {}) {
    std::vector<std::uint8_t> code;
    for (int i = 0; i < 5; ++i) code.insert(code.end(), {1,1,20,0x99});
    code.insert(code.end(), {3,1,0,0x71,0,right}); // COMPARE profile X with constant.
    if (mask) code.insert(code.end(), {0x2f,0,*mask,0,1,1,0,0x4a});
    code.push_back(condition);
    const auto jump = code.size();
    code.insert(code.end(), {1,1,0,0x99});
    const auto no = code.size();
    code.insert(code.end(), {0x25,0,0,0,1,1,0,0x99});
    const auto yes = code.size();
    code.insert(code.end(), {0x25,0,0,0,1,1,0,0x99});
    code[jump+2] = static_cast<std::uint8_t>(yes);
    code[no+6] = static_cast<std::uint8_t>(code.size()); code.push_back(0);
    code[yes+6] = static_cast<std::uint8_t>(code.size()); code.push_back(0);
    return code;
}
} // namespace

int main() {
    const EclDispatchDialect dialect{"ssi-five-entry-v1", 0x9900, 0xc04f};
    auto opcodes = routing_ecl_opcodes();
    auto bytes = record();
    const auto entries = decode_ecl_program_entries(bytes, dialect, opcodes);
    assert(entries.header_size == 20 && entries.record_prefix_size == 0);
    auto prefixed = bytes;
    prefixed.insert(prefixed.begin(), {0x88, 0x13});
    assert(decode_ecl_program_entries(prefixed, dialect, opcodes).record_prefix_size == 2);
    const auto table = decode_ecl_dispatch_table(bytes, 29, dialect, opcodes, 20);
    assert(table.target_offsets == std::vector<std::size_t>({48,49}));
    auto map = decode_geo16_record(std::vector<std::uint8_t>(1026), 27);
    map.cells[35].raw_event = 0x81; // mask -> selector one
    const auto refs = resolve_geo_events(map, archive(bytes), 7, EclEventKind::move, dialect, opcodes);
    assert(refs.size() == 1 && refs[0].bytecode_offset == 49 && refs[0].dispatch_offset == 29);
    assert(refs[0].source.row == 2 && refs[0].source.column == 3 && refs[0].source.ecl_record_id == 7);
    assert(geo_event_id("synthetic.game", "GEO1.DAX", refs[0].source) == "synthetic.game:geo:GEO1.DAX:27:2:3");
    assert(resolve_geo_events(map, archive(prefixed), 7, EclEventKind::search, dialect, opcodes)[0].bytecode_offset == 49);
    // Identical GEO selectors can route differently by location. Coordinates
    // come from the configured adapter, never fixed Pool memory addresses.
    auto positioned = dialect;
    positioned.position_addresses = std::array<std::uint16_t,3>{0x7100,0x7101,0x7102};
    auto locations = map;
    locations.cells[35].raw_event = 0;
    locations.cells[1].raw_event = locations.cells[16].raw_event = 0x81;
    auto coordinate_route = bytes;
    coordinate_route[22] = 0; coordinate_route[23] = 0x71;
    auto routes = resolve_geo_events(locations,archive(coordinate_route),7,EclEventKind::move,positioned,opcodes);
    assert(routes.size() == 2 && routes[0].bytecode_offset == 49 && routes[1].bytecode_offset == 48);
    coordinate_route[22] = 1;
    routes = resolve_geo_events(locations,archive(coordinate_route),7,EclEventKind::move,positioned,opcodes);
    assert(routes[0].bytecode_offset == 48 && routes[1].bytecode_offset == 49);
    coordinate_route[22] = 2; // Arrival facing is not a property of the cell.
    routes = resolve_geo_events(locations,archive(coordinate_route),7,EclEventKind::move,positioned,opcodes);
    assert(!routes[0].bytecode_offset && routes[0].diagnostic ==
        "Dispatch depends on unknown game state at address 28930");
    // Each IF condition follows the VM's skip-one-instruction semantics.
    // Equal GEO selectors can choose different tables using only known X/Y.
    for (std::uint8_t condition = 0x16; condition <= 0x1b; ++condition) {
        for (std::uint8_t right = 0; right <= 2; ++right) {
            const auto code = conditional_route(condition,right);
            const auto resolved = resolve_geo_events(locations,archive(code),7,EclEventKind::move,positioned,opcodes);
            for (const auto& route : resolved) {
                const int x = route.source.column;
                const bool expected[]{x == right,x != right,x < right,x > right,x <= right,x >= right};
                assert(route.bytecode_offset == code.size() - (expected[condition-0x16] ? 1 : 2));
            }
        }
    }
    for (const std::uint8_t mask : {0,1}) {
        const auto code = conditional_route(0x16,0,mask);
        const auto resolved = resolve_geo_events(locations,archive(code),7,EclEventKind::move,positioned,opcodes);
        for (const auto& route : resolved) // AND replaces the earlier COMPARE flag.
            assert(route.bytecode_offset == code.size() - (mask == 0 ? 1 : 2));
    }
    auto state_route = conditional_route(0x16,0);
    state_route[22] = 0xd5; state_route[23] = 0x6d;
    routes = resolve_geo_events(locations,archive(state_route),7,EclEventKind::move,positioned,opcodes);
    assert(!routes[0].bytecode_offset && routes[0].diagnostic ==
        "Dispatch depends on unknown game state at address 28117");
    auto truncated_skip = conditional_route(0x17,1); truncated_skip.resize(28);
    routes = resolve_geo_events(locations,archive(truncated_skip),7,EclEventKind::move,positioned,opcodes);
    assert(!routes[0].bytecode_offset && !routes[0].diagnostic.empty());
    for (auto& spec : opcodes) if (spec.opcode == 3) spec.operand_count = 1;
    rejects([&] { (void)decode_ecl_program_entries(state_route,positioned,opcodes); });
    opcodes = routing_ecl_opcodes();
    for (auto& spec : opcodes) if (spec.opcode == 0x16) spec.operand_count = 1;
    rejects([&] { (void)decode_ecl_program_entries(state_route,positioned,opcodes); });
    opcodes = routing_ecl_opcodes();
    for (const auto addresses : {std::array<std::uint16_t,3>{0x7100,0x7100,0x7102},
        std::array<std::uint16_t,3>{dialect.square_info_address,0x7101,0x7102}}) {
        auto conflicting = positioned; conflicting.position_addresses = addresses;
        rejects([&] { (void)resolve_geo_events(locations,archive(coordinate_route),7,EclEventKind::move,conflicting,opcodes); });
    }
    rejects([&] { (void)resolve_geo_events(map, archive(bytes), 8, EclEventKind::move, dialect, opcodes); });
    map.cells[35].raw_event = 3;
    assert(!resolve_geo_events(map, archive(bytes), 7, EclEventKind::move, dialect, opcodes)[0].bytecode_offset);
    map.cells[35].raw_event = 0x80;
    assert(resolve_geo_events(map, archive(bytes), 7, EclEventKind::move, dialect, opcodes)[0].bytecode_offset == 48);
    auto bad = bytes;
    bad[31] = 1; // routing now reads unknown memory 0x4a01
    assert(!resolve_geo_events(map, archive(bad), 7, EclEventKind::move, dialect, opcodes)[0].bytecode_offset);
    bad = bytes; bad[37] = 0x98; // first target below base
    rejects([&] { (void)decode_ecl_dispatch_table(bad, 29, dialect, opcodes, 20); });
    bad = bytes; bad[36] = 35; // target inside table
    rejects([&] { (void)decode_ecl_dispatch_table(bad, 29, dialect, opcodes, 20); });
    bad = bytes; bad.resize(36);
    rejects([&] { (void)decode_ecl_dispatch_table(bad, 29, dialect, opcodes, 20); });
    bad = bytes; bad[2] = 1; // lifecycle target inside header
    rejects([&] { (void)decode_ecl_program_entries(bad, dialect, opcodes); });
    auto unknown = dialect; unknown.id = "unsupported";
    rejects([&] { (void)decode_ecl_program_entries(bytes, unknown, opcodes); });
    opcodes.push_back(opcodes[0]);
    rejects([&] { (void)decode_ecl_program_entries(bytes, dialect, opcodes); });
    opcodes = routing_ecl_opcodes(); opcodes[1].operand_count = 0;
    rejects([&] { (void)decode_ecl_program_entries(bytes, dialect, opcodes); });
    // A routing cycle terminates with a diagnostic, no pattern-search fallback.
    bytes[20] = 1; bytes[21] = 1; bytes[22] = 20; bytes[23] = 0x99;
    assert(resolve_geo_events(map, archive(bytes), 7, EclEventKind::move, dialect, routing_ecl_opcodes())[0].diagnostic ==
        "Dispatch routing step limit exceeded");
    // Original-style index lengths and legacy synthetic offsets both remain valid.
    assert(archive(record()).data_offset() == 11);
    assert(DaxArchive::parse(std::vector<std::uint8_t>{0,0}).records().empty());
    return 0;
}
