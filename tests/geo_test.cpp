// SPDX-License-Identifier: GPL-3.0-or-later
// Entirely synthetic, independently constructed GEO and DAX fixture bytes.
#include "goldbox/formats/geo.hpp"

#include <algorithm>
#include <array>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

using namespace goldbox::formats;
namespace {

template <typename F> void rejects(F&& callable) {
    bool did_throw = false;
    try { callable(); } catch (const std::exception&) { did_throw = true; }
    assert(did_throw);
}

std::vector<std::uint8_t> synthetic_geo() {
    std::vector<std::uint8_t> bytes(GeoMap::minimum_record_size, 0);
    bytes[0] = 0xBA; // opaque header bytes, not interpreted as dimensions
    bytes[1] = 0xBE;
    // 16x16 grid, so row 2/column 3 is index 35.
    constexpr std::size_t index = 2 * 16 + 3;
    bytes[2 + index] = 0xAB;        // north=10, east=11
    bytes[2 + 256 + index] = 0xCD;  // south=12, west=13
    bytes[2 + 512 + index] = 0x85;  // selector 5, unknown high flag set
    bytes[2 + 768 + index] = 0xA5;  // retain raw door flags
    bytes[2 + 512 + 0] = 0x7F;      // top-left trigger
    bytes[2 + 512 + 255] = 0x80;    // selector 0, flagged raw event
    return bytes;
}

// Encode a tiny independent DAX index for a synthetic record. The payload
// uses only literal runs and contains no SSI-supplied data.
std::vector<std::uint8_t> synthetic_dax(std::uint8_t record_id,
                                         const std::vector<std::uint8_t>& raw) {
    assert(raw.size() <= 65535);
    std::size_t packed_size = 0;
    for (std::size_t offset = 0; offset < raw.size();) {
        const auto count = std::min<std::size_t>(128, raw.size() - offset);
        packed_size += 1 + count;
        offset += count;
    }
    assert(packed_size <= 65535);
    const auto size = [](std::size_t n) -> std::uint8_t {
        return static_cast<std::uint8_t>(n & 0xFFu);
    };
    return {
        11, 0, record_id, 0,0,0,0, size(raw.size()), size(raw.size() >> 8),
        size(packed_size), size(packed_size >> 8)
    };
}

DaxArchive synthetic_archive(std::uint8_t id,
                             const std::vector<std::uint8_t>& content) {
    auto archive = synthetic_dax(id, content);
    std::vector<std::uint8_t> compressed;
    for (std::size_t offset = 0; offset < content.size();) {
        const auto count = std::min<std::size_t>(128, content.size() - offset);
        compressed.push_back(static_cast<std::uint8_t>(count - 1));
        compressed.insert(compressed.end(),
                          content.begin() + static_cast<std::ptrdiff_t>(offset),
                          content.begin() + static_cast<std::ptrdiff_t>(offset + count));
        offset += count;
    }
    archive.insert(archive.end(), compressed.begin(), compressed.end());
    return DaxArchive::parse(archive);
}

} // namespace

int main() {
    auto input = synthetic_geo();
    auto map = decode_geo16_record(input, 27);
    assert(map.dax_record_id == 27);
    assert(map.header_bytes[0] == 0xBA && map.header_bytes[1] == 0xBE);
    assert(map.trailing_bytes == 0);
    const auto& tile = map.cell(2, 3);
    assert(tile.north == 10 && tile.east == 11);
    assert(tile.south == 12 && tile.west == 13);
    assert(tile.raw_event == 0x85 && tile.event_selector() == 5);
    assert(tile.event_high_bit() && tile.door_flags == 0xA5);
    assert(map.cell(0, 0).event_selector() == 127);
    assert(map.cell(15, 15).event_selector() == 0);
    assert(map.cell(15, 15).event_high_bit());
    assert(map.cell(0, 1).raw_event == 0 && map.cell(0, 1).door_flags == 0);
    rejects([&] { (void)map.cell(16, 0); });
    rejects([&] { (void)map.cell(1, 16); });
    rejects([&] { (void)map.cell(999, 0); });

    const auto refs = geo_event_references(map);
    assert(refs.size() == 3);
    assert(refs[0].row == 0 && refs[0].column == 0 && refs[0].event_selector == 127);
    assert(refs[1].geo_record_id == 27 && refs[1].row == 2 && refs[1].column == 3);
    assert(refs[1].raw_event == 0x85 && refs[1].event_selector == 5);
    assert(!refs[1].ecl_record_id.has_value());
    assert(refs[2].row == 15 && refs[2].column == 15 && refs[2].raw_event == 0x80);
    assert(refs[2].event_selector == 0);

    // Missing/truncated planes are never silently filled with zeros.
    for (std::size_t length : std::array<std::size_t, 7>{0, 1, 2, 257, 513, 769, 1025}) {
        rejects([&] { (void)decode_geo16_record(
            std::span(input).first(length), 27); });
    }
    input.push_back(0xCE); // variant trailer can be retained without guessing semantics
    map = decode_geo16_record(input, 27);
    assert(map.trailing_bytes == 1);

    const auto geo_archive = synthetic_archive(27, input);
    const auto roundtrip = extract_geo16_map(geo_archive, 27);
    assert(roundtrip.cell(2,3).raw_event == 0x85);
    assert(roundtrip.trailing_bytes == 1);
    rejects([&] { (void)extract_geo16_map(geo_archive, 26); });

    // Explicit binding: preserve ECL record identity, not unverified byte offsets.
    const auto ecl_archive = synthetic_archive(9, {0x00, 0x13});
    const auto bound = bind_geo_events_to_ecl(roundtrip, ecl_archive, 9);
    assert(bound.size() == 3);
    assert(bound[1].ecl_record_id.has_value() && *bound[1].ecl_record_id == 9);
    assert(bound[1].event_selector == 5);
    rejects([&] { (void)bind_geo_events_to_ecl(roundtrip, ecl_archive, 8); });
    // Even an existing ECL record ID must decode successfully.
    auto invalid_ecl_data = synthetic_dax(9, {0x00});
    invalid_ecl_data.push_back(0x80); // invalid RLE repeat, exceeds raw size
    invalid_ecl_data.push_back(0x41);
    auto broken = DaxArchive::parse(invalid_ecl_data);
    rejects([&] { (void)bind_geo_events_to_ecl(roundtrip, broken, 9); });

    auto empty_map = std::vector<std::uint8_t>(GeoMap::minimum_record_size, 0);
    const auto no_events = geo_event_references(decode_geo16_record(empty_map, 0));
    assert(no_events.empty());
    return 0;
}
