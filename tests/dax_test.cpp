#include "goldbox/formats/dax.hpp"

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

using goldbox::formats::DaxArchive;
namespace fs = std::filesystem;

namespace {
void fails(const std::function<void()>& fn) {
    bool failed = false;
    try { fn(); } catch (const std::exception&) { failed = true; }
    assert(failed);
}

// Independently authored DAX index fixture, not an SSI resource file.
std::vector<std::uint8_t> sample() {
    return {
        20, 0,                    // 2 + 2 * 9 byte headers
        1, 0,0,0,0, 5,0, 6,0,      // rec #1, offset 0, raw size 5, stored size 6
        2, 6,0,0,0, 6,0, 4,0,      // rec #2, offset 6, raw size 6, stored size 4
        4, 'H','e','l','l','o',       // literal 5 bytes
        0xfe, 'X', 0, '!',         // repeat X twice; literal ! once
    };
}
} // namespace

int main() {
    const auto bytes = sample();
    // Fix the synthetic fixture: second record has three decoded bytes.
    auto fixture = bytes;
    fixture[2 + 9 + 5] = 3;
    auto archive = DaxArchive::parse(fixture);
    assert(archive.data_offset() == 20);
    assert(archive.records().size() == 2);
    assert(archive.extract(1) == std::vector<std::uint8_t>({'H', 'e', 'l', 'l', 'o'}));
    assert(archive.extract(2) == std::vector<std::uint8_t>({'X', 'X', '!'}));
    fails([&] { (void)archive.extract(3); });

    // Wrong index lengths, truncated files, duplicate IDs, out-of-bounds and
    // overlaps must fail before decoding any payload.
    fails([&] { (void)DaxArchive::parse(std::vector<std::uint8_t>{0}); });
    auto invalid = fixture;
    invalid[0] = 19; fails([&] { (void)DaxArchive::parse(invalid); });
    invalid = fixture;
    invalid[11] = 1; fails([&] { (void)DaxArchive::parse(invalid); });
    invalid = fixture;
    invalid[12] = 0xff; fails([&] { (void)DaxArchive::parse(invalid); });
    invalid = fixture;
    invalid[12] = 4; fails([&] { (void)DaxArchive::parse(invalid); });
    invalid = fixture;
    invalid[20] = 20; fails([&] { (void)DaxArchive::parse(invalid).extract(1); });
    invalid = fixture;
    invalid[26] = 0x80; fails([&] { (void)DaxArchive::parse(invalid).extract(2); });
    invalid = fixture;
    invalid[2 + 9 + 5] = 0; fails([&] { (void)DaxArchive::parse(invalid).extract(2); });

    // Empty index and no payload is a valid, parseable (though not useful) archive.
    const auto empty = DaxArchive::parse(std::vector<std::uint8_t>{2,0});
    assert(empty.records().empty());
    return 0;
}
