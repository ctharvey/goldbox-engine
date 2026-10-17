// SPDX-License-Identifier: GPL-3.0-or-later
#include "goldbox/image_presentation.hpp"
#include "goldbox/game_profile.hpp"
#ifdef GOLDBOX_RENDERER
#include "view.hpp"
#include <SDL3/SDL.h>
#define SDL_MAIN_HANDLED
#include <SDL3/SDL_main.h>
#endif
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <algorithm>
#include <iostream>
#include <functional>

using namespace goldbox;
using namespace goldbox::formats;
namespace {
DaxArchive archive(std::uint8_t id, const std::vector<std::uint8_t>& raw) {
    std::vector<std::uint8_t> packed;
    for (std::size_t i = 0; i < raw.size(); i += 128) {
        const auto n = std::min<std::size_t>(128,raw.size()-i);
        packed.push_back(n-1);
        packed.insert(packed.end(),raw.begin()+i,raw.begin()+i+n);
    }
    std::vector<std::uint8_t> result{9,0,id,0,0,0,0,std::uint8_t(raw.size()),std::uint8_t(raw.size()>>8),
        std::uint8_t(packed.size()),std::uint8_t(packed.size()>>8)};
    result.insert(result.end(),packed.begin(),packed.end());
    return DaxArchive::parse(result);
}
std::vector<std::uint8_t> sequence() {
    std::vector<std::uint8_t> result{3};
    for (int i = 0; i < 3; ++i) {
        const auto at = result.size(); result.resize(at+25,0);
        result[at+4] = 1; result[at+6] = 1; result[at+21] = 0x12+i;
    }
    return result;
}
std::vector<std::uint8_t> part(std::uint8_t color) {
    std::vector<std::uint8_t> bytes(17+32,color*17);
    std::fill_n(bytes.begin(),17,0);
    bytes[0] = 8; bytes[2] = 1; bytes[8] = 1;
    return bytes;
}
std::vector<std::uint8_t> program(std::initializer_list<std::uint8_t> body) {
    std::vector<std::uint8_t> result;
    for (int i = 0; i < 5; ++i) result.insert(result.end(),{1,1,20,0x99});
    result.insert(result.end(),body);
    return result;
}
void rejects(const std::function<void()>& fn) {
    bool failed = false;
    try { fn(); } catch (const std::exception&) { failed = true; }
    assert(failed);
}
struct Host : EclHost {
    std::string events;
    void text(std::string_view, bool) override {}
    void picture(std::uint8_t id) override { assert(id == 7); events += 'P'; }
    void sprite_start(EclSpriteRequest r) override {
        assert(r.sprite == 7 && r.distance == 2 && r.replacement_picture == 255); events += 'S';
    }
    void sprite_advance() override { events += 'A'; }
    void clear_images() override { events += 'C'; }
};
}
int main(int argc, char** argv) {
    if (argc == 3) {
        ImageArchives assets{DaxArchive::from_file(argv[1]),DaxArchive::from_file(argv[2])};
        ImagePresentation display(assets);
        ExplorationSession context; context.row = context.column = 8;
        unsigned pictures = 0, sprites = 0;
        for (const auto& entry : assets.pictures.records()) { display.picture(entry.id); assert(display.frame()); ++pictures; }
        for (const auto& entry : assets.sprites.records()) {
            const auto frames = decode_ega_sequence(assets.sprites.extract(entry.id),EgaFrameEncoding::independent);
            for (unsigned i = 0; i < frames.size(); ++i) {
                display.sprite_start({entry.id,std::uint8_t(i),255},context);
                assert(display.frame()->image.pixels == frames[i].image.pixels);
                display.sprite_advance();
            }
            ++sprites;
        }
        std::cout << "pictures=" << pictures << " sprites=" << sprites << " host checks passed\n";
        return 0;
    }
    assert(argc == 1);
    Host host;
    EclState state;
    const EclDispatchDialect dialect{"ssi-five-entry-v1",0x9900,0xc04f};
    const auto ops = minimal_vm_opcodes(EclImageDialect::sprite3);
    const auto run = [&](auto bytes, const auto& specs) {
        return execute_ecl(bytes,EclEventKind::search,dialect,specs,state,host);
    };
    const auto script = program({0x0e,0,7,0x0c,0,7,0,2,0,255,0x0d,0x31,0x0e,0,255,0});
    assert(run(script,ops).exited && host.events == "PSACC");
    host.events.clear();
    assert(!run(script,minimal_vm_opcodes()).exited && host.events.empty());
    assert(!run(program({0x0e,2,0,1,0}),ops).exited && host.events.empty()); // ID 256
    assert(!run(program({0x0c,0,7,0,3,0,255,0}),ops).exited);
    assert(!run(program({0x0e,0x80,1,4,0}),ops).exited); // Text is not an ID.
    auto conflicting = ops;
    for (auto& op : conflicting) if (op.opcode == 0x0c) op.operand_count = 4;
    assert(!run(program({0x0c,0,7,0,2,0,255,0,0,0}),conflicting).exited && host.events.empty());
    conflicting = ops; conflicting.push_back({0x0c,"OTHER",3});
    assert(!run(script,conflicting).exited && host.events.empty());
    struct Headless : EclHost { void text(std::string_view,bool) override {} } headless;
    assert(!execute_ecl(script,EclEventKind::search,dialect,ops,state,headless).exited);
    ImageArchives assets{archive(7,sequence()),archive(7,sequence())};
    ImagePresentation display(assets);
    ExplorationSession session; session.row = session.column = 8;
    display.picture(7);
    assert(display.frame()->image.pixels[1] == 2 && display.palette() == EgaPaletteKind::standard);
    rejects([&] { display.picture(8); });
    assert(display.frame()->image.pixels[1] == 2);
    display.sprite_start({7,2,255},session);
    assert(display.frame()->image.pixels[1] == 4 && display.palette() == EgaPaletteKind::sprite);
    display.sprite_advance(); assert(display.frame()->image.pixels[1] == 3);
    display.sprite_advance(); display.sprite_advance(); assert(display.frame()->image.pixels[1] == 2);
    session.map.cells[7*16+8].north = 1;
    display.sprite_start({7,2,255},session); assert(display.frame()->image.pixels[1] == 3);
    session.map.cells[8*16+8].north = 1;
    display.sprite_start({7,2,255},session); assert(display.frame()->image.pixels[1] == 2);
    rejects([&] { display.sprite_start({7,1,7},session); });
    assert(display.frame()->image.pixels[1] == 2);
    session.map = {};
    session.map.cells[7*16+8].south = 1;
    display.sprite_start({7,2,255},session); assert(display.frame()->image.pixels[1] == 2);
    session.map = {};
    auto short_sequence = sequence(); short_sequence.resize(26); short_sequence[0] = 1;
    ImageArchives short_assets{assets.pictures,archive(7,short_sequence)};
    ImagePresentation missing_distance(short_assets);
    rejects([&] { missing_distance.sprite_start({7,2,255},session); });
    assert(!missing_distance.frame());
    std::vector<std::uint8_t> wide(1+21+48,0); wide[0] = 1; wide[5] = 1; wide[7] = 12;
    ImageArchives wide_assets{archive(7,wide),assets.sprites};
    ImagePresentation oversized(wide_assets);
    rejects([&] { oversized.picture(7); });
    assert(!oversized.frame());
    display.picture(255); assert(!display.frame());
    display.sprite_advance(); display.clear(); assert(!display.frame());
    session.row = 255;
    rejects([&] { display.sprite_start({7,0,255},session); });
    assets.portraits = PortraitArchives{archive(7,part(3)),archive(9,part(4))};
    const formats::EgaPortraitLayout layout{88,88,{0,0},{0,40}};
    display.portrait(7,9,layout);
    assert(display.frame()->image.pixels[0] == 3 && display.frame()->image.pixels[40*88] == 4);
    assert(display.palette() == EgaPaletteKind::standard);
    rejects([&] { display.portrait(8,9,layout); });
    assert(display.frame()->image.pixels[0] == 3);
    rejects([&] { display.portrait(7,8,layout); });
    assert(display.frame()->image.pixels[40*88] == 4);
    ImagePresentation no_portraits(short_assets);
    rejects([&] { no_portraits.portrait(7,9,layout); });
    display.clear(); assert(!display.frame());
#ifdef GOLDBOX_RENDERER
    session = {};
    session.dialect = dialect; session.opcodes = ops;
    session.ecl_record = program({0x0e,0,7,0x0c,0,7,0,0,0,255,0x0d,0x31,
        0x0e,0,7,9,0,9,1,0,0x4a,0});
    SDL_SetMainReady(); assert(SDL_Init(SDL_INIT_VIDEO));
    for (const auto code : {SDLK_SPACE,SDLK_ESCAPE}) {
        SDL_Event event{}; event.type = SDL_EVENT_KEY_DOWN; event.key.key = code;
        assert(SDL_PushEvent(&event));
    }
    assert(run_exploration_window(session,"synthetic-images",{},nullptr,&assets) == 0);
    assert(session.state.memory[0x4a00] == 9); // Reached past every graphics callback.
    SDL_Quit();
    session = {};
    session.dialect = dialect; session.opcodes = ops;
    session.ecl_record = program({9,0,7,1,0x10,0x4a,0x0e,0,9,
        9,0,255,1,0x10,0x4a,0x0e,0,7,9,0,42,1,0,0x4a,0});
    const GameProfile::Portrait config{0x4a10,layout};
    assert(SDL_Init(SDL_INIT_VIDEO));
    for (const auto code : {SDLK_SPACE,SDLK_ESCAPE}) {
        SDL_Event event{}; event.type = SDL_EVENT_KEY_DOWN; event.key.key = code;
        assert(SDL_PushEvent(&event));
    }
    assert(run_exploration_window(session,"synthetic-portraits",{},nullptr,&assets,nullptr,{},&config) == 0);
    assert(session.state.memory[0x4a00] == 42 && session.state.memory[0x4a10] == 255);
    SDL_Quit();
#endif
}
