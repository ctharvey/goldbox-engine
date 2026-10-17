// SPDX-License-Identifier: GPL-3.0-or-later
#include "goldbox/formats/graphics.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <algorithm>
#include <cassert>
#include <functional>
#include <iostream>

using namespace goldbox::formats;
namespace {
void rejects(const std::function<void()>& fn) {
    bool failed = false;
    try { fn(); } catch (const std::exception&) { failed = true; }
    assert(failed);
}
void frame(std::vector<std::uint8_t>& bytes, unsigned height, std::uint8_t pixels) {
    const auto at = bytes.size();
    bytes.resize(at+21+height*4,0);
    bytes[at] = 0x78; bytes[at+1] = 0x56; bytes[at+2] = 0x34; bytes[at+3] = 0x12;
    bytes[at+4] = height; bytes[at+6] = 1;
    bytes[at+8] = 2; bytes[at+10] = 3; bytes[at+12] = 0x99; bytes[at+13] = 7;
    std::fill(bytes.begin()+at+21,bytes.end(),pixels);
}
}
int main(int argc, char** argv) {
    if (argc >= 3) {
        // Explicit, opt-in local validation. Counts/diagnostics only; no exports.
        const std::string mode(argv[1]);
        const auto archive = DaxArchive::from_file(argv[2]);
        unsigned records = 0, images = 0, failed = 0;
        for (const auto& entry : archive.records()) {
            if (argc == 4 && std::to_string(entry.id) != argv[3]) continue;
            ++records;
            try {
                const auto raw = archive.extract(entry.id);
                if (mode == "mono8") images += decode_mono8_tiles(raw).size();
                else if (mode == "ega") images += decode_ega_images(raw).images.size();
                else if (mode == "independent" || mode == "xor-first")
                    images += decode_ega_sequence(raw,mode == "independent" ? EgaFrameEncoding::independent :
                        EgaFrameEncoding::xor_first).size();
                else throw std::runtime_error("Unknown explicit image format");
            } catch (const std::exception& error) {
                ++failed;
                std::cout << "record=" << unsigned(entry.id) << " error=" << error.what() << '\n';
            }
        }
        std::cout << "records=" << records << " images=" << images << " failed=" << failed << '\n';
        return failed || !records ? 1 : 0;
    }
    assert(argc == 1);
    std::vector<std::uint8_t> mono(16,0);
    mono[0] = 0x81; mono[8+7] = 0x40;
    const auto tiles = decode_mono8_tiles(mono);
    assert(tiles.size() == 2 && tiles[0].pixels[0] == 15 && tiles[0].pixels[7] == 15);
    assert(tiles[0].pixels[1] == 0 && tiles[1].pixels[57] == 15);
    rejects([&] { decode_mono8_tiles({}); });
    rejects([&] { decode_mono8_tiles(std::span(mono).first(15)); });
    rejects([&] { decode_mono8_tiles(std::vector<std::uint8_t>(2056)); });
    std::vector<std::uint8_t> sequence{3};
    frame(sequence,2,0x12); frame(sequence,2,0x34); frame(sequence,2,0x56);
    const auto copy = sequence;
    auto decoded = decode_ega_sequence(sequence,EgaFrameEncoding::independent);
    assert(decoded.size() == 3 && decoded[2].image.pixels[0] == 5 && decoded[2].image.pixels[1] == 6);
    assert(decoded[0].delay == 0x12345678 && decoded[0].x_offset == 16 && decoded[0].y_offset == 24);
    assert(decoded[0].header_byte == 0x99 && decoded[0].cga_mapping[0] == 7);
    decoded = decode_ega_sequence(sequence,EgaFrameEncoding::xor_first);
    assert(decoded[1].image.pixels[0] == 2 && decoded[1].image.pixels[1] == 6);
    assert(decoded[2].image.pixels[0] == 4 && decoded[2].image.pixels[1] == 4); // First, not previous frame.
    assert(sequence == copy);
    for (std::size_t end = 0; end < sequence.size(); ++end)
        rejects([&] { decode_ega_sequence(std::span(sequence).first(end),EgaFrameEncoding::independent); });
    auto trailing = sequence; trailing.push_back(0);
    rejects([&] { decode_ega_sequence(trailing,EgaFrameEncoding::independent); });
    auto invalid = sequence; invalid[0] = 9;
    rejects([&] { decode_ega_sequence(invalid,EgaFrameEncoding::independent); });
    invalid = sequence; invalid[0] = 0;
    rejects([&] { decode_ega_sequence(invalid,EgaFrameEncoding::independent); });
    invalid = sequence; invalid[1+6] = 0;
    rejects([&] { decode_ega_sequence(invalid,EgaFrameEncoding::independent); });
    invalid = sequence; invalid[1+6] = 41;
    rejects([&] { decode_ega_sequence(invalid,EgaFrameEncoding::independent); });
    invalid = sequence; invalid[1+4] = 201;
    rejects([&] { decode_ega_sequence(invalid,EgaFrameEncoding::independent); });
    invalid = sequence; invalid[1+9] = 255;
    rejects([&] { decode_ega_sequence(invalid,EgaFrameEncoding::independent); });
    std::vector<std::uint8_t> varied{2}; frame(varied,2,0x12); frame(varied,3,0x34);
    assert(decode_ega_sequence(varied,EgaFrameEncoding::independent)[1].image.height == 3);
    rejects([&] { decode_ega_sequence(varied,EgaFrameEncoding::xor_first); });
    const auto normal = ega_palette(), sprite = ega_palette(EgaPaletteKind::sprite), combat = ega_palette(EgaPaletteKind::combat);
    assert(normal[0][3] == 255 && sprite[0][3] == 0 && combat[0][3] == 0);
    assert((sprite[13] == Rgba{0,0,0,255}) && (combat[8] == Rgba{0,0,0,255}));
    assert(sprite[8] == normal[8] && combat[13] == normal[13]);
    rejects([&] { ega_palette(static_cast<EgaPaletteKind>(99)); });
    rejects([&] { decode_ega_sequence(sequence,static_cast<EgaFrameEncoding>(99)); });
}
