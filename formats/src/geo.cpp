// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Goldbox Engine contributors
// GEO cell-plane parsing adapted from Gold Box Explorer by Bil Simser (MIT).
// Upstream revision eac30abaa6ee66aea6f5d65ebe6d676b10015a8f.
// See THIRD_PARTY.md and licenses/gold-box-explorer-MIT.txt.
#include "goldbox/formats/geo.hpp"

#include <algorithm>
#include <string>

namespace goldbox::formats {

const GeoCell& GeoMap::cell(const std::size_t row,
                             const std::size_t column) const {
    if (row >= height || column >= width) {
        throw std::out_of_range("GEO cell coordinates are outside 16x16 map");
    }
    return cells[row * width + column];
}

GeoMap decode_geo16_record(std::span<const std::uint8_t> record_bytes,
                           const std::uint8_t record_id) {
    if (record_bytes.size() < GeoMap::minimum_record_size) {
        throw GeoDecodeError("GEO record truncated: need 2 header bytes + 4x256 cell bytes");
    }

    GeoMap map;
    map.dax_record_id = record_id;
    map.header_bytes = {record_bytes[0], record_bytes[1]};
    map.trailing_bytes = record_bytes.size() - GeoMap::minimum_record_size;

    constexpr std::size_t north_east_plane = 2;
    constexpr std::size_t south_west_plane = north_east_plane + GeoMap::cell_count;
    constexpr std::size_t event_plane = south_west_plane + GeoMap::cell_count;
    constexpr std::size_t door_plane = event_plane + GeoMap::cell_count;

    for (std::size_t index = 0; index < GeoMap::cell_count; ++index) {
        auto& cell = map.cells[index];
        const std::uint8_t ne = record_bytes[north_east_plane + index];
        const std::uint8_t sw = record_bytes[south_west_plane + index];
        cell.north = static_cast<std::uint8_t>(ne >> 4);
        cell.east = static_cast<std::uint8_t>(ne & 0x0fu);
        cell.south = static_cast<std::uint8_t>(sw >> 4);
        cell.west = static_cast<std::uint8_t>(sw & 0x0fu);
        cell.raw_event = record_bytes[event_plane + index];
        cell.door_flags = record_bytes[door_plane + index];
    }
    return map;
}

GeoMap extract_geo16_map(const DaxArchive& archive,
                          const std::uint8_t record_id) {
    return decode_geo16_record(archive.extract(record_id), record_id);
}

std::vector<GeoEventReference> geo_event_references(const GeoMap& map) {
    std::vector<GeoEventReference> refs;
    for (std::size_t index = 0; index < GeoMap::cell_count; ++index) {
        const auto& cell = map.cells[index];
        if (cell.raw_event == 0) continue;
        refs.push_back({
            map.dax_record_id,
            static_cast<std::uint8_t>(index / GeoMap::width),
            static_cast<std::uint8_t>(index % GeoMap::width),
            cell.raw_event,
            cell.event_selector(),
            std::nullopt
        });
    }
    return refs;
}

std::vector<GeoEventReference> bind_geo_events_to_ecl(
    const GeoMap& map, const DaxArchive& ecl_archive,
    const std::uint8_t ecl_record_id) {
    const auto& records = ecl_archive.records();
    if (std::none_of(records.begin(), records.end(),
                     [ecl_record_id](const DaxRecord& r) {
                         return r.id == ecl_record_id;
                     })) {
        throw GeoDecodeError("Requested ECL record not present in ECL DAX archive: " +
                             std::to_string(ecl_record_id));
    }
    // Validate actual ECL record decompression before advertising the pairing.
    // This deliberately does not interpret the record contents or event offsets.
    (void)ecl_archive.extract(ecl_record_id);
    auto refs = geo_event_references(map);
    for (auto& ref : refs) ref.ecl_record_id = ecl_record_id;
    return refs;
}

} // namespace goldbox::formats
