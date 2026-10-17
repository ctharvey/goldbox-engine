// SPDX-License-Identifier: GPL-3.0-or-later
#include "goldbox/formats/graphics.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <algorithm>
#include <functional>
#include <iostream>

using namespace goldbox::formats;
namespace {
void rejects(const std::function<void()>& fn) {
    bool failed = false;
    try { fn(); } catch (const std::exception&) { failed = true; }
    assert(failed);
}
std::vector<std::uint8_t> ega(unsigned count) {
    std::vector<std::uint8_t> b(17+count*32,0);
    b[0] = 8; b[2] = 1; b[8] = count;
    for (unsigned i = 0; i < count*32; ++i) b[17+i] = ((i%16)<<4)|((i+1)%16);
    return b;
}
DaxArchive records(std::initializer_list<std::pair<std::uint8_t,std::vector<std::uint8_t>>> entries) {
    const auto index_size = entries.size()*9;
    std::vector<std::uint8_t> bytes(2+index_size,0), payload;
    bytes[0] = index_size;
    std::size_t index = 2;
    for (const auto& [id,raw] : entries) {
        std::vector<std::uint8_t> packed;
        for (std::size_t at = 0; at < raw.size(); at += 128) {
            const auto size = std::min<std::size_t>(128,raw.size()-at);
            packed.push_back(size-1);
            packed.insert(packed.end(),raw.begin()+at,raw.begin()+at+size);
        }
        bytes[index] = id;
        for (unsigned n = 0; n < 4; ++n) bytes[index+1+n] = payload.size()>>(8*n);
        bytes[index+5] = raw.size(); bytes[index+6] = raw.size()>>8;
        bytes[index+7] = packed.size(); bytes[index+8] = packed.size()>>8;
        index += 9;
        payload.insert(payload.end(),packed.begin(),packed.end());
    }
    bytes.insert(bytes.end(),payload.begin(),payload.end());
    return DaxArchive::parse(bytes);
}
DaxArchive archive(std::uint8_t id, const std::vector<std::uint8_t>& raw) { return records({{id,raw}}); }
} // namespace
int main(int argc, char** argv) {
    if (argc == 4) {
        // Opt-in local resource check, never registered with public CTest.
        const auto tiles = DaxArchive::from_file(argv[1]);
        const auto definitions = DaxArchive::from_file(argv[2]);
        const auto shared = DaxArchive::from_file(argv[3]);
        unsigned images = 0, banks = 0, views = 0, unsupported = 0, failed = 0;
        for (const auto& record : tiles.records()) {
            try { images += decode_ega_images(tiles.extract(record.id)).images.size(); }
            catch (const std::exception& error) {
                ++unsupported;
                std::cout << "Unsupported EGA record=" << unsigned(record.id) << ": " << error.what() << '\n';
            }
        }
        for (const auto& record : definitions.records()) {
            try {
                const auto graphics = load_wall_graphics(definitions,tiles,shared,{record.id,255,255});
                assert(graphics.present[0]);
                ++banks;
                views += definitions.extract(record.id).size()/156*10;
            } catch (const std::exception& error) {
                ++failed;
                std::cout << "Failed wall bank=" << unsigned(record.id) << ": " << error.what() << '\n';
            }
        }
        std::cout << "EGA images=" << images << " unsupported records=" << unsupported
                  << " wall records=" << banks << " views=" << views << " failed records=" << failed << '\n';
        return failed ? 1 : 0;
    }
    assert(argc == 1);
    const auto data = ega(2);
    auto decoded = decode_ega_images(data);
    assert(decoded.images.size() == 2 && decoded.images[0].width == 8 && decoded.images[0].height == 8);
    assert(decoded.images[0].pixels[0] == 0 && decoded.images[0].pixels[1] == 1);
    assert(decoded.images[0].pixels[30] == 15 && decoded.images[0].pixels[31] == 0);
    const auto palette = ega_palette();
    assert((palette[6] == Rgba{170,85,0,255}));
    assert((palette[13] == Rgba{255,85,255,255}));
    for (std::size_t n = 0; n < 17; ++n) rejects([&] { decode_ega_images(std::span(data).first(n)); });
    for (const auto index : {0,2,8}) {
        auto invalid = data; invalid[index] = 0;
        rejects([&] { decode_ega_images(invalid); });
    }
    auto truncated = data; truncated.pop_back();
    rejects([&] { decode_ega_images(truncated); });
    auto extra = data; extra.push_back(0);
    rejects([&] { decode_ega_images(extra); });
    auto oversized = data; oversized[3] = 255;
    rejects([&] { decode_ega_images(oversized); });
    std::vector<IndexedImage> symbols{{8,8,std::vector<std::uint8_t>(64,13)},decoded.images[0]};
    std::vector<std::uint8_t> definitions(156,1);
    definitions[54] = 0;
    const auto walls = decode_wall_definitions(definitions,symbols);
    assert(walls.size() == 1 && walls[0][6].width == 56 && walls[0][6].height == 64);
    assert(walls[0][6].pixels[0] == 13 && walls[0][6].pixels[8] == 0);
    assert(walls[0][8].width == 16 && walls[0][8].height == 88);
    auto bad_ref = definitions; bad_ref[155] = 2;
    rejects([&] { decode_wall_definitions(bad_ref,symbols); });
    rejects([&] { decode_wall_definitions(std::span(definitions).first(155),symbols); });
    symbols[1].pixels[0] = 16;
    rejects([&] { decode_wall_definitions(definitions,symbols); });
    const auto tile_archive = archive(7,ega(1)), shared = archive(203,ega(1));
    const auto wall_archive = archive(7,std::vector<std::uint8_t>(780,2));
    const auto loaded = load_wall_graphics(wall_archive,tile_archive,shared,{7,255,7});
    assert(loaded.present[0] && !loaded.present[5] && loaded.present[10]);
    assert(loaded.walls[0][6].pixels[0] == decoded.images[0].pixels[0]);
    rejects([&] { load_wall_graphics(wall_archive,tile_archive,shared,{8,255,255}); });
    rejects([&] { load_wall_graphics(wall_archive,tile_archive,tile_archive,{7,255,255}); });
    const auto bad_walls = archive(7,std::vector<std::uint8_t>(780,3));
    rejects([&] { load_wall_graphics(bad_walls,tile_archive,shared,{7,255,255}); });
    const auto short_bank = archive(7,std::vector<std::uint8_t>(156,1));
    rejects([&] { load_wall_graphics(short_bank,tile_archive,shared,{7,255,255}); });
    const auto combined = archive(7,std::vector<std::uint8_t>(2340,2));
    const auto continued = load_wall_graphics(combined,tile_archive,shared,{7,127,127});
    assert(continued.present[0] && continued.present[5] && continued.present[10]);
    rejects([&] { load_wall_graphics(wall_archive,tile_archive,shared,{7,127,255}); });
    rejects([&] { load_wall_graphics(wall_archive,tile_archive,shared,{255,127,255}); });
    auto last = ega(1);
    std::fill(last.begin()+17,last.end(),0x77);
    const auto chunks = records({{101,ega(1)},{102,ega(1)},{103,last}});
    const auto zero = archive(0,std::vector<std::uint8_t>(2340,4));
    const auto expanded = load_wall_graphics(zero,chunks,shared,{127,127,127});
    assert(expanded.walls[14][6].pixels[0] == 7);
    rejects([&] { load_wall_graphics(zero,records({{101,ega(1)},{102,ega(1)}}),shared,{0,255,255}); });
    return 0;
}
