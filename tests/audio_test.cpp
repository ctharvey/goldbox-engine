// SPDX-License-Identifier: GPL-3.0-or-later
#include "goldbox/formats/audio.hpp"
#ifdef GOLDBOX_RENDERER
#include "audio.hpp"
#define SDL_MAIN_HANDLED
#include <SDL3/SDL_main.h>
#endif
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

using namespace goldbox::formats;
template<class F> void rejects(F action) {
    try { action(); } catch (const std::runtime_error&) { return; }
    throw std::runtime_error("Invalid audio accepted");
}
int main(int argc, char** argv) {
    std::vector<std::uint8_t> data(160);
    const auto put = [&](std::size_t p, std::uint16_t v) { data[p] = v; data[p+1] = v >> 8; };
    put(0,32); put(2,64);
    const std::vector<std::uint8_t> first{255,4,0,8,255,10,3,0,255,0,3,0,255,0,0,0};
    const std::vector<std::uint8_t> second{255,4,0,4,255,10,3,0,255,0,5,0,255,0,0,0};
    std::copy(first.begin(),first.end(),data.begin()+32);
    std::copy(second.begin(),second.end(),data.begin()+64);
    const auto clip = decode_ssi_speaker(data,0,0,5000);
    assert(clip.frames.size() == 6);
    assert((clip.frames[0] == SpeakerFrame{0,true})); // Pitch is latched before interpreting commands.
    assert((clip.frames[1] == SpeakerFrame{2048,true}));
    assert((clip.frames[2] == SpeakerFrame{2048,true}));
    assert((clip.frames[3] == SpeakerFrame{1024,true})); // First active channel wins.
    assert(!clip.frames.back().enabled);
    const auto pcm = render_speaker(clip);
    assert(pcm.size() == std::uint64_t(6)*5000*48000/1193181);
    assert(pcm == render_speaker(clip));
    assert(pcm.back() == 0.0f);
    assert(std::all_of(pcm.begin(),pcm.end(),[](float v) { return std::isfinite(v) && std::abs(v) <= 0.12f; }));
    auto bad = data;
    bad[32] = 253; rejects([&] { decode_ssi_speaker(bad,0,0,5000); });
    bad = data; bad[33] = 14; rejects([&] { decode_ssi_speaker(bad,0,0,5000); });
    bad = data; bad[42] = 255; bad[43] = 255;
    rejects([&] { decode_ssi_speaker(bad,0,0,5000); });
    rejects([&] { decode_ssi_speaker(data,155,0,5000); });
    bad = data; bad[0] = 159;
    rejects([&] { decode_ssi_speaker(bad,0,0,5000); });
    rejects([&] { decode_ssi_speaker(data,0,0,0); });
    rejects([&] { render_speaker(clip,0); });
    std::vector<std::uint8_t> sweep(256);
    sweep[0] = 32;
    const std::vector<std::uint8_t> loop{
        255,4,0,16, 255,6,240,255, 255,10,3,0, 255,38,2,0,
        255,0,2,0, 254,38,48,0, 255,0,0,0};
    std::copy(loop.begin(),loop.end(),sweep.begin()+32);
    const auto repeated = decode_ssi_speaker(sweep,0,0,5000);
    assert((repeated.frames == std::vector<SpeakerFrame>{{0,true},{4080,true},{4064,true},{4048,true},{0,false}}));
    sweep[32] = 254; sweep[33] = 38; sweep[34] = 32; sweep[35] = 0;
    rejects([&] { decode_ssi_speaker(sweep,0,0,5000); }); // Zero counter jumps indefinitely, bounded.
    sweep[33] = 2;
    rejects([&] { decode_ssi_speaker(sweep,0,0,5000); }); // Script-pointer fields are not loop counters.
    const std::vector<std::uint8_t> vibrato{
        255,4,232,3, 255,10,3,0, 255,28,200,0, 255,32,16,0,
        255,34,254,255, 255,36,32,0, 255,0,3,0, 255,0,0,0};
    std::fill(sweep.begin()+32,sweep.end(),0);
    std::copy(vibrato.begin(),vibrato.end(),sweep.begin()+32);
    sweep[201] = 128;
    const auto modulated = decode_ssi_speaker(sweep,0,0,5000);
    assert((modulated.frames == std::vector<SpeakerFrame>{{0,true},{1001,true},{1000,true},{0,false}}));
    // Synthetic EXEPACK final literal run, followed by one fill run.
    std::vector<std::uint8_t> exe(98);
    const auto ew = [&](std::size_t p, std::uint16_t v) { exe[p] = v; exe[p+1] = v >> 8; };
    ew(0,0x5a4d); ew(8,2); ew(20,18); ew(22,3);
    ew(80+12,2); ew(80+14,1); ew(80+16,0x4252);
    // 24 prefix bytes, four literals, four repeated bytes -> 32-byte output.
    for (std::size_t i = 32; i < 56; ++i) exe[i] = 7;
    for (std::size_t i = 56; i < 60; ++i) exe[i] = 7;
    exe[60] = 4; exe[61] = 0; exe[62] = 179;
    exe[63] = 9; exe[64] = 4; exe[65] = 0; exe[66] = 176;
    for (std::size_t i = 67; i < 80; ++i) exe[i] = 255;
    const auto unpacked = unpack_exepack(exe);
    assert(unpacked.size() == 32 && unpacked[27] == 7 && unpacked[28] == 9 && unpacked[31] == 9);
    rejects([&] { unpack_exepack(std::span(exe).first(70)); });
    auto invalid = exe; invalid[66] = 175;
    rejects([&] { unpack_exepack(invalid); });
    invalid = exe; invalid[64] = 0;
    rejects([&] { unpack_exepack(invalid); });
#ifdef GOLDBOX_RENDERER
    SDL_SetMainReady();
    assert(SDL_SetEnvironmentVariable(SDL_GetEnvironment(),"SDL_AUDIODRIVER","dummy",true));
    { goldbox::AudioPlayer player(true); player.play(clip); player.play(clip); player.stop(); }
    SDL_Quit();
#endif
    if (argc == 3 || argc == 4) {
        std::ifstream input(argv[1],std::ios::binary);
        assert(input);
        const std::vector<std::uint8_t> executable{std::istreambuf_iterator<char>(input),{}};
        const auto resident = unpack_exepack(executable);
        const auto entry = argc == 4 ? std::stoul(argv[3]) : 8;
        assert(entry < 20);
        const auto original = decode_ssi_speaker(std::span(resident).subspan(0x8ae0),0x55f,entry,5041);
        std::ifstream expected(argv[2]);
        assert(expected);
        for (const auto frame : original.frames) {
            unsigned divisor{}, enabled{};
            assert(expected >> divisor >> enabled);
            assert(frame.divisor == divisor && frame.enabled == bool(enabled));
        }
        unsigned extra{};
        assert(!(expected >> extra));
        std::cout << "Original speaker frames matched: " << original.frames.size()
                  << "; PCM samples: " << render_speaker(original).size() << '\n';
#ifdef GOLDBOX_RENDERER
        // Private, explicit original-resource run also checks a real output device.
        if (argc == 3) {
            assert(SDL_UnsetEnvironmentVariable(SDL_GetEnvironment(),"SDL_AUDIODRIVER"));
            { goldbox::AudioPlayer player(true); player.play(original); SDL_Delay(150); }
            SDL_Quit();
            std::cout << "Original effect submitted to default audio device\n";
        }
#endif
    }
    std::cout << "Audio tests passed\n";
}
