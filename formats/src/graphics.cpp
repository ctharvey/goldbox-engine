// SPDX-License-Identifier: MIT
// Attribution-ID: gold-box-explorer-ega
// Copyright (c) 2018 Bil Simser
// EGA/mono decoding adapted from Gold Box Explorer's EgaBlock.cs,
// EgaSpriteBlock.cs and MonoBlock.cs (see attribution registry).
// Bounds, indexed output, wall assembly and explicit bank loading added here.
// MIT notice: licenses/gold-box-explorer-MIT.txt
#include "goldbox/formats/graphics.hpp"
#include <algorithm>
#include <stdexcept>

namespace goldbox::formats {
namespace {
std::uint16_t word(std::span<const std::uint8_t> b, std::size_t at) {
    return static_cast<std::uint16_t>(b[at] | (std::uint16_t(b[at+1]) << 8));
}
void require_tiles(std::span<const IndexedImage> tiles) {
    for (const auto& tile : tiles) {
        if (tile.width != 8 || tile.height != 8 || tile.pixels.size() != 64 ||
            std::any_of(tile.pixels.begin(),tile.pixels.end(),[](auto p) { return p > 15; }))
            throw std::runtime_error("Wall tiles must be valid 8x8 EGA images");
    }
}
} // namespace
EgaImages decode_ega_images(std::span<const std::uint8_t> record) {
    if (record.size() < 17) throw std::runtime_error("Truncated SSI EGA header");
    const auto height = word(record,0), columns = word(record,2);
    const std::size_t count = record[8];
    if (!height || height > 200 || !columns || columns > 40 || !count)
        throw std::runtime_error("Unsupported SSI EGA dimensions/count");
    const std::size_t width = columns*8, packed_size = width*height/2;
    if (record.size() != 17 + packed_size*count)
        throw std::runtime_error("SSI EGA payload size/count mismatch");
    EgaImages result;
    if (word(record,4) > 8191 || word(record,6) > 8191)
        throw std::runtime_error("SSI EGA offsets overflow pixel coordinates");
    result.x_offset = word(record,4)*8;
    result.y_offset = word(record,6)*8;
    std::copy_n(record.begin()+9,8,result.cga_mapping.begin());
    for (std::size_t n = 0; n < count; ++n) {
        IndexedImage image{static_cast<std::uint16_t>(width),height,{}};
        image.pixels.reserve(width*height);
        for (const auto b : record.subspan(17+n*packed_size,packed_size)) {
            image.pixels.push_back(b >> 4);
            image.pixels.push_back(b & 15);
        }
        result.images.push_back(std::move(image));
    }
    return result;
}

std::vector<IndexedImage> decode_mono8_tiles(std::span<const std::uint8_t> record) {
    if (record.empty() || record.size()%8 || record.size() > 256*8)
        throw std::runtime_error("Invalid monochrome 8x8 tile strip");
    std::vector<IndexedImage> result;
    for (std::size_t at = 0; at < record.size(); at += 8) {
        IndexedImage tile{8,8,{}};
        tile.pixels.reserve(64);
        for (const auto row : record.subspan(at,8)) for (unsigned bit = 0; bit < 8; ++bit)
            tile.pixels.push_back(row & (0x80 >> bit) ? 15 : 0);
        result.push_back(std::move(tile));
    }
    return result;
}

IndexedImage compose_ega_portrait(const EgaImages& head, const EgaImages& body,
    const EgaPortraitLayout& layout) {
    if (!layout.width || layout.width > 320 || !layout.height || layout.height > 200)
        throw std::runtime_error("Unsupported portrait canvas dimensions");
    if (head.images.size() != 1 || body.images.size() != 1)
        throw std::runtime_error("Portrait parts require exactly one frame");
    const bool header_offsets = head.x_offset || head.y_offset || body.x_offset || body.y_offset;
    IndexedImage result{layout.width,layout.height,std::vector<std::uint8_t>(std::size_t(layout.width)*layout.height,0)};
    const auto paint = [&](const EgaImages& part, const std::array<std::uint16_t,2>& fallback) {
        const auto& image = part.images.front();
        const std::size_t x = header_offsets ? part.x_offset : fallback[0];
        const std::size_t y = header_offsets ? part.y_offset : fallback[1];
        if (!image.width || !image.height || image.pixels.size() != std::size_t(image.width)*image.height ||
            x+image.width > layout.width || y+image.height > layout.height ||
            std::any_of(image.pixels.begin(),image.pixels.end(),[](auto p) { return p > 15; }))
            throw std::runtime_error("Portrait part exceeds canvas or has invalid pixels");
        for (std::size_t row = 0; row < image.height; ++row)
            std::copy_n(image.pixels.begin()+row*image.width,image.width,
                result.pixels.begin()+(y+row)*layout.width+x);
    };
    // Independent bounded composition; SSI Engine was consulted for placement
    // semantics only. Neither input pixels nor metadata are modified.
    paint(head,layout.head_offset);
    paint(body,layout.body_offset);
    return result;
}

std::vector<EgaFrame> decode_ega_sequence(std::span<const std::uint8_t> record, EgaFrameEncoding encoding) {
    if (encoding != EgaFrameEncoding::independent && encoding != EgaFrameEncoding::xor_first)
        throw std::runtime_error("Invalid EGA frame encoding");
    if (record.empty() || !record[0] || record[0] > 8)
        throw std::runtime_error("Unsupported EGA sequence frame count");
    std::vector<EgaFrame> result;
    std::vector<std::uint8_t> base;
    std::size_t cursor = 1;
    for (unsigned n = 0; n < record[0]; ++n) {
        if (record.size()-cursor < 21) throw std::runtime_error("Truncated EGA sequence header");
        const auto header = record.subspan(cursor,21);
        const auto height = word(header,4), columns = word(header,6);
        if (!height || height > 200 || !columns || columns > 40 || word(header,8) > 8191 || word(header,10) > 8191)
            throw std::runtime_error("Unsupported EGA sequence dimensions/offsets");
        const std::size_t width = columns*8, packed_size = width*height/2;
        cursor += 21;
        if (record.size()-cursor < packed_size) throw std::runtime_error("Truncated EGA sequence pixels");
        if (n && encoding == EgaFrameEncoding::xor_first &&
            (result[0].image.width != width || result[0].image.height != height))
            throw std::runtime_error("Dependent EGA frames must match the first frame dimensions");
        EgaFrame frame;
        frame.delay = std::uint32_t(header[0]) | (std::uint32_t(header[1]) << 8) |
            (std::uint32_t(header[2]) << 16) | (std::uint32_t(header[3]) << 24);
        frame.x_offset = word(header,8)*8;
        frame.y_offset = word(header,10)*8;
        frame.header_byte = header[12];
        std::copy_n(header.begin()+13,8,frame.cga_mapping.begin());
        frame.image = {static_cast<std::uint16_t>(width),height,{}};
        frame.image.pixels.reserve(width*height);
        const auto packed = record.subspan(cursor,packed_size);
        if (!n && encoding == EgaFrameEncoding::xor_first) base.assign(packed.begin(),packed.end());
        for (std::size_t i = 0; i < packed.size(); ++i) {
            const auto b = n && encoding == EgaFrameEncoding::xor_first ? packed[i]^base[i] : packed[i];
            frame.image.pixels.push_back(b >> 4);
            frame.image.pixels.push_back(b & 15);
        }
        result.push_back(std::move(frame));
        cursor += packed_size;
    }
    if (cursor != record.size()) throw std::runtime_error("Trailing EGA sequence data");
    return result;
}

std::array<Rgba,16> ega_palette(EgaPaletteKind kind) {
    if (kind != EgaPaletteKind::standard && kind != EgaPaletteKind::sprite && kind != EgaPaletteKind::combat)
        throw std::runtime_error("Invalid EGA palette kind");
    std::array<Rgba,16> result{};
    for (unsigned i = 0; i < 16; ++i) {
        const unsigned bright = i & 8 ? 85 : 0;
        result[i] = {static_cast<std::uint8_t>((i & 4 ? 170 : 0)+bright),
            static_cast<std::uint8_t>((i & 2 ? 170 : 0)+bright),
            static_cast<std::uint8_t>((i & 1 ? 170 : 0)+bright),255};
    }
    result[6][1] = 85; // EGA brown, rather than dark yellow.
    if (kind != EgaPaletteKind::standard) {
        result[kind == EgaPaletteKind::sprite ? 13 : 8] = result[0];
        result[0][3] = 0;
    }
    return result;
}

std::vector<WallViews> decode_wall_definitions(std::span<const std::uint8_t> record,
    std::span<const IndexedImage> tiles) {
    if (record.empty() || record.size()%156 || record.size()/156 > 15)
        throw std::runtime_error("Malformed or oversized WALLDEF record");
    if (tiles.empty() || tiles.size() > 256) throw std::runtime_error("Invalid wall tile count");
    require_tiles(tiles);
    constexpr std::array<unsigned,10> columns{1,1,1,3,2,2,7,2,2,1};
    constexpr std::array<unsigned,10> rows{2,4,4,4,8,8,8,11,11,2};
    std::vector<WallViews> result(record.size()/156);
    std::size_t cursor = 0;
    for (auto& wall : result) for (std::size_t view = 0; view < 10; ++view) {
        auto& image = wall[view];
        image.width = columns[view]*8;
        image.height = rows[view]*8;
        image.pixels.resize(image.width*image.height);
        for (unsigned row = 0; row < rows[view]; ++row) for (unsigned col = 0; col < columns[view]; ++col) {
            const auto index = record[cursor++];
            if (index >= tiles.size()) throw std::runtime_error("WALLDEF references missing tile");
            for (unsigned y = 0; y < 8; ++y) for (unsigned x = 0; x < 8; ++x)
                image.pixels[(row*8+y)*image.width+col*8+x] = tiles[index].pixels[y*8+x];
        }
    }
    return result;
}

WallGraphics load_wall_graphics(const DaxArchive& definitions, const DaxArchive& tiles,
    const DaxArchive& shared, std::array<std::uint8_t,3> banks) {
    const auto common = decode_ega_images(shared.extract(203));
    if (common.x_offset || common.y_offset) throw std::runtime_error("Unsupported shared tile offsets");
    require_tiles(common.images);
    WallGraphics result;
    std::uint8_t previous_record{};
    unsigned previous_start{};
    bool previous_present = false;
    for (unsigned bank = 0; bank < 3; ++bank) {
        if (banks[bank] == 255) continue;
        if (bank && banks[bank] == 127 && !previous_present)
            throw std::runtime_error("Continuation bank has no preceding wall record");
        const auto id = banks[bank] == 127 ? previous_record : banks[bank];
        const unsigned start = banks[bank] == 127 && bank ? previous_start+5 : 0;
        const auto raw = definitions.extract(id);
        std::vector<IndexedImage> symbols{{8,8,std::vector<std::uint8_t>(64,13)}};
        symbols.insert(symbols.end(),common.images.begin(),common.images.end());
        const auto exists = [&](unsigned record) {
            return std::any_of(tiles.records().begin(),tiles.records().end(),
                [&](const auto& entry) { return entry.id == record; });
        };
        const auto append = [&](unsigned record) {
            const auto own = decode_ega_images(tiles.extract(static_cast<std::uint8_t>(record)));
            if (own.x_offset || own.y_offset) throw std::runtime_error("Unsupported wall tile offsets");
            if (own.images.size()+symbols.size() > 256)
                throw std::runtime_error("Unsupported wall tile bank layout");
            symbols.insert(symbols.end(),own.images.begin(),own.images.end());
        };
        if (exists(id)) append(id);
        else if (raw.size() <= 780) throw std::runtime_error("Wall bank tile record is missing");
        if (raw.size() > 780) {
            const unsigned base = id == 0 ? 100 : 10*id;
            for (unsigned extra = 1; extra <= 3; ++extra) if (exists(base+extra)) append(base+extra);
        }
        auto views = decode_wall_definitions(raw,symbols);
        if (views.size()%5 || start+5 > views.size())
            throw std::runtime_error("Wall bank selection exceeds definition record");
        for (unsigned i = 0; i < 5; ++i) {
            result.walls[bank*5+i] = std::move(views[start+i]);
            result.present[bank*5+i] = true;
        }
        previous_record = id;
        previous_start = start;
        previous_present = true;
    }
    return result;
}
} // namespace goldbox::formats
