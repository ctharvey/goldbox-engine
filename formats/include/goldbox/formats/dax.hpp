#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

namespace goldbox::formats {

// Original SSI archives use a 2-byte little-endian data offset followed by
// fixed 9-byte record index entries. All offsets/sizes are validated on parse.
struct DaxRecord {
    std::uint8_t id;
    std::uint32_t relative_offset;
    std::uint16_t unpacked_size;
    std::uint16_t stored_size;
};

class DaxArchive {
public:
    static constexpr std::size_t max_file_size = 64u * 1024u * 1024u;

    [[nodiscard]] static DaxArchive parse(std::span<const std::uint8_t> bytes);
    [[nodiscard]] static DaxArchive from_file(const std::filesystem::path& file);
    [[nodiscard]] const std::vector<DaxRecord>& records() const noexcept { return records_; }
    [[nodiscard]] std::uint16_t data_offset() const noexcept { return data_offset_; }
    [[nodiscard]] std::vector<std::uint8_t> extract(std::uint8_t record_id) const;

private:
    std::uint16_t data_offset_ = 0;
    std::vector<DaxRecord> records_;
    std::vector<std::uint8_t> bytes_;
};

} // namespace goldbox::formats
