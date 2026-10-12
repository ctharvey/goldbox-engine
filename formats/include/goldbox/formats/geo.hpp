// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Goldbox Engine contributors
// GEO 16x16 cell layout adapted from Gold Box Explorer (MIT), by Bil Simser.
// Upstream: src/Common/Plugins/GeoDax/GeoDaxFile.cs and GeoWallRecord.cs.
// See THIRD_PARTY.md and licenses/gold-box-explorer-MIT.txt.
#pragma once

#include "goldbox/formats/dax.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

namespace goldbox::formats {

// Only the 16x16 GEO/DAX layout is supported. Other games may have distinct
// layouts and should register a different format decoder.
class GeoDecodeError final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

struct GeoCell {
    std::uint8_t north{};
    std::uint8_t east{};
    std::uint8_t south{};
    std::uint8_t west{};
    std::uint8_t raw_event{};
    std::uint8_t door_flags{};

    [[nodiscard]] std::uint8_t event_selector() const noexcept {
        return static_cast<std::uint8_t>(raw_event & 0x7fu);
    }
    [[nodiscard]] bool event_high_bit() const noexcept {
        return (raw_event & 0x80u) != 0;
    }
};

struct GeoMap {
    static constexpr std::size_t width = 16;
    static constexpr std::size_t height = 16;
    static constexpr std::size_t cell_count = width * height;
    static constexpr std::size_t minimum_record_size = 2 + 4 * cell_count;

    // The first two bytes' meaning and any trailing data are not yet verified.
    // Retain them for versioned decoding instead of assigning invented fields.
    std::uint8_t dax_record_id{};
    std::array<std::uint8_t, 2> header_bytes{};
    std::size_t trailing_bytes{};
    std::array<GeoCell, cell_count> cells{};

    [[nodiscard]] const GeoCell& cell(std::size_t row, std::size_t column) const;
};

// The ECL event selector comes from GEO's event byte. This is NOT an ECL
// bytecode offset. Linking an event to a concrete address requires a separate
// title-specific event dispatch-table decoder.
struct GeoEventReference {
    std::uint8_t geo_record_id{};
    std::uint8_t row{};
    std::uint8_t column{};
    std::uint8_t raw_event{};
    std::uint8_t event_selector{};
    // Present only when a caller explicitly pairs this map with an ECL record.
    std::optional<std::uint8_t> ecl_record_id;
};

// Decode a single, already decompressed DAX GEO record. Requires at least
// 1026 bytes. All four map planes and event flags are preserved.
[[nodiscard]] GeoMap decode_geo16_record(
    std::span<const std::uint8_t> record_bytes, std::uint8_t record_id);

// Convenience adapter for the existing bounded, read-only DAX implementation.
[[nodiscard]] GeoMap extract_geo16_map(const DaxArchive& archive,
                                        std::uint8_t record_id);

// Include every nonzero RAW event byte (including 0x80, for which selector=0).
[[nodiscard]] std::vector<GeoEventReference> geo_event_references(const GeoMap& map);

// Explicitly associate a GEO map's event selectors with an ECL record that
// exists in the supplied archive. This does not infer event offsets or execute
// bytecode. Throws if the requested ECL record ID is absent.
[[nodiscard]] std::vector<GeoEventReference> bind_geo_events_to_ecl(
    const GeoMap& map, const DaxArchive& ecl_archive,
    std::uint8_t ecl_record_id);

} // namespace goldbox::formats
