#include "goldbox/formats/dax.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>

namespace goldbox::formats {
namespace {

std::uint16_t u16(std::span<const std::uint8_t> b, std::size_t at) {
    return static_cast<std::uint16_t>(b[at] | (static_cast<std::uint16_t>(b[at + 1]) << 8));
}
std::uint32_t u32(std::span<const std::uint8_t> b, std::size_t at) {
    return static_cast<std::uint32_t>(b[at]) |
        (static_cast<std::uint32_t>(b[at + 1]) << 8) |
        (static_cast<std::uint32_t>(b[at + 2]) << 16) |
        (static_cast<std::uint32_t>(b[at + 3]) << 24);
}

std::vector<std::uint8_t> decode_rle(std::span<const std::uint8_t> packed, std::uint16_t unpacked_size) {
    std::vector<std::uint8_t> result;
    result.reserve(unpacked_size);
    std::size_t pos = 0;
    while (pos < packed.size()) {
        const int raw_control = packed[pos++];
        const int control = raw_control < 128 ? raw_control : raw_control - 256;
        if (control >= 0) {
            const auto count = static_cast<std::size_t>(control) + 1;
            if (count > packed.size() - pos || count > unpacked_size - result.size()) {
                throw std::runtime_error("invalid DAX literal run");
            }
            result.insert(result.end(), packed.begin() + static_cast<std::ptrdiff_t>(pos),
                          packed.begin() + static_cast<std::ptrdiff_t>(pos + count));
            pos += count;
        } else {
            const auto count = static_cast<std::size_t>(-static_cast<int>(control));
            if (pos == packed.size() || count > unpacked_size - result.size()) {
                throw std::runtime_error("invalid DAX repeated run");
            }
            result.insert(result.end(), count, packed[pos++]);
        }
    }
    if (result.size() != unpacked_size) {
        throw std::runtime_error("DAX record decompressed length mismatch");
    }
    return result;
}

} // namespace

DaxArchive DaxArchive::parse(std::span<const std::uint8_t> bytes) {
    if (bytes.size() > max_file_size) throw std::runtime_error("DAX archive exceeds 64 MiB limit");
    if (bytes.size() < 2) throw std::runtime_error("DAX archive is truncated");
    DaxArchive archive;
    archive.data_offset_ = u16(bytes, 0);
    if (archive.data_offset_ < 2 || archive.data_offset_ > bytes.size() ||
        (archive.data_offset_ - 2) % 9 != 0) {
        throw std::runtime_error("invalid DAX index size");
    }
    const auto count = static_cast<std::size_t>(archive.data_offset_ - 2) / 9;
    archive.records_.reserve(count);
    std::unordered_set<std::uint8_t> ids;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> intervals;
    intervals.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const auto at = 2 + i * 9;
        DaxRecord record{bytes[at], u32(bytes, at + 1), u16(bytes, at + 5), u16(bytes, at + 7)};
        if (!ids.insert(record.id).second) throw std::runtime_error("duplicate DAX record id");
        const auto start = static_cast<std::uint64_t>(archive.data_offset_) + record.relative_offset;
        const auto end = start + record.stored_size;
        if (end > bytes.size()) throw std::runtime_error("DAX record extends beyond archive");
        if (record.stored_size == 0 && record.unpacked_size != 0) {
            throw std::runtime_error("DAX nonempty record has empty storage");
        }
        if (record.stored_size > 0) intervals.emplace_back(start, end);
        archive.records_.push_back(record);
    }
    std::sort(intervals.begin(), intervals.end());
    for (std::size_t i = 1; i < intervals.size(); ++i) {
        if (intervals[i].first < intervals[i - 1].second) {
            throw std::runtime_error("overlapping DAX record regions");
        }
    }
    archive.bytes_.assign(bytes.begin(), bytes.end());
    return archive;
}

DaxArchive DaxArchive::from_file(const std::filesystem::path& file) {
    if (std::filesystem::is_symlink(file) || !std::filesystem::is_regular_file(file)) {
        throw std::runtime_error("DAX input must be a regular non-symlinked file");
    }
    if (std::filesystem::file_size(file) > max_file_size) {
        throw std::runtime_error("DAX input exceeds 64 MiB limit");
    }
    std::ifstream input(file, std::ios::binary);
    if (!input) throw std::runtime_error("unable to open DAX input file");
    const auto size = static_cast<std::size_t>(std::filesystem::file_size(file));
    if (size > max_file_size) throw std::runtime_error("DAX input exceeds 64 MiB limit");
    std::vector<std::uint8_t> bytes(size);
    if (size > 0) input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size));
    if (!input || input.peek() != std::char_traits<char>::eof()) {
        throw std::runtime_error("DAX file changed size or failed while reading");
    }
    return parse(bytes);
}

std::vector<std::uint8_t> DaxArchive::extract(std::uint8_t record_id) const {
    const auto found = std::find_if(records_.begin(), records_.end(),
        [record_id](const DaxRecord& r) { return r.id == record_id; });
    if (found == records_.end()) throw std::out_of_range("DAX record not found");
    const auto start = static_cast<std::size_t>(data_offset_) + found->relative_offset;
    return decode_rle(std::span(bytes_).subspan(start, found->stored_size), found->unpacked_size);
}

} // namespace goldbox::formats
