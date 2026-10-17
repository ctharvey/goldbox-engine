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
    void wall_banks(std::array<std::uint8_t,3> banks) override {
        assert((banks == std::array<std::uint8_t,3>{7,255,127})); events += 'W';
    }
};
}
int main(int argc, char** argv) {
    if (argc == 10 && std::string_view(argv[1]) == "initialize") {
        // Read-only original-resource probe. Never print original text/pixels.
        const auto profile = read_game_profile(argv[2]);
        if (!profile.event_dispatch) throw std::runtime_error("Map probe requires a dispatch profile");
        const auto id = [](const char* value) {
            std::size_t used = 0; const auto n = std::stoul(value,&used);
            if (used != std::string_view(value).size() || n > 255) throw std::runtime_error("Invalid record ID");
            return static_cast<std::uint8_t>(n);
        };
        const auto geo = DaxArchive::from_file(argv[3]);
        ExplorationSession context;
        context.map = extract_geo16_map(geo,id(argv[4]));
        context.ecl_record = DaxArchive::from_file(argv[5]).extract(id(argv[6]));
        const auto& config = *profile.event_dispatch;
        context.dialect = {config.dialect,config.code_base,config.square_info_address,true,config.position_addresses};
        context.opcodes = minimal_vm_opcodes(profile.image_dialect == "ssi-images-3-v1" ? EclImageDialect::sprite3 : EclImageDialect::disabled,
            profile.area_dialect == "ssi-area-map3-v1" ? EclAreaDialect::map3 : EclAreaDialect::disabled);
        context.maps = MapResources{[geo](std::uint8_t record) { return extract_geo16_map(geo,record); },"read-only-local-probe"};
        struct Probe : EclHost {
            ExplorationSession& session;
            DaxArchive definitions, tiles, shared;
            unsigned texts{}, bindings{};
            Probe(ExplorationSession& s, const char* d, const char* t, const char* c) : session(s),
                definitions(DaxArchive::from_file(d)), tiles(DaxArchive::from_file(t)), shared(DaxArchive::from_file(c)) {}
            void text(std::string_view,bool) override { ++texts; }
            void wall_banks(std::array<std::uint8_t,3> values) override {
                const auto walls = load_wall_graphics(definitions,tiles,shared,values);
                for (const auto& cell : session.map.cells)
                    for (const auto wall : {cell.north,cell.east,cell.south,cell.west})
                        if (wall && (wall > 15 || !walls.present[wall-1])) throw std::runtime_error("Map has an unbound wall");
                session.wall_banks = values; ++bindings;
            }
        } probe(context,argv[7],argv[8],argv[9]);
        const auto run = context.trigger(EclEventKind::initialize,probe,{1000,64});
        std::cout << "exited=" << run.exited << " steps=" << run.steps << " offset=" << run.bytecode_offset
                  << " map=" << unsigned(context.map.dax_record_id) << " wallBindings=" << probe.bindings
                  << " textCalls=" << probe.texts << " diagnostic=" << run.diagnostic << '\n';
        if (context.wall_banks) std::cout << "banks=" << unsigned((*context.wall_banks)[0]) << ','
            << unsigned((*context.wall_banks)[1]) << ',' << unsigned((*context.wall_banks)[2]) << '\n';
        return run.exited ? 0 : 1;
    }
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
    assert(run(program({0x37,0,7,0,255,0,127,0}),ops).exited && host.events == "W");
    host.events.clear();
    assert(!run(program({0x37,2,0,1,0,255,0,127,0}),ops).exited && host.events.empty());
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
    assert(!execute_ecl(program({0x37,0,7,0,255,0,127,0}),EclEventKind::search,dialect,ops,state,headless).exited);
    ImageArchives assets{archive(7,sequence()),archive(7,sequence())};
    ImagePresentation display(assets);
    ExplorationSession session; session.row = session.column = 8;
    display.picture(7);
    assert(display.frame()->image.pixels[1] == 2 && display.palette() == EgaPaletteKind::standard);
    rejects([&] { display.picture(8); });
    assert(display.frame()->image.pixels[1] == 2);
    assert(!display.advance_time(10000) && display.animation_wait_ms() == 0);
    assets.picture_period_ms = 400;
    assert(!display.advance_time(399) && display.animation_wait_ms() == 1);
    assert(display.advance_time(1) && display.frame()->image.pixels[1] == (2^3));
    assert(display.advance_time(400) && display.frame()->image.pixels[1] == (2^4));
    assert(display.advance_time(400) && display.frame()->image.pixels[1] == 2);
    assert(!display.advance_time(1200));
    assert(display.advance_time(850) && display.animation_wait_ms() == 350);
    display.picture(7);
    (void)display.advance_time(UINT64_MAX);
    const std::uint8_t huge_expected[]{2,2^3,2^4};
    assert(display.frame()->image.pixels[1] == huge_expected[(UINT64_MAX/400)%3]);
    assert(display.animation_wait_ms() == 400-UINT64_MAX%400);
    display.picture(7); // Replacement restarts timing; failed loads preserve it.
    assert(display.animation_wait_ms() == 400);
    assert(!display.advance_time(100));
    rejects([&] { display.picture(8); });
    assert(display.animation_wait_ms() == 300);
    display.sprite_start({7,2,255},session);
    assert(!display.advance_time(UINT64_MAX) && display.animation_wait_ms() == 0);
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
    assert(!display.advance_time(400) && display.animation_wait_ms() == 0);
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
    const WallArchives walls{archive(7,std::vector<std::uint8_t>(780,2)),archive(7,part(3)),archive(203,part(13))};
    const auto initial_graphics = formats::load_wall_graphics(walls.definitions,walls.tiles,walls.shared,{7,255,255});
    const auto save = std::filesystem::temp_directory_path()/"goldbox-wall-host-save.json";
    assert(!std::filesystem::exists(save));
    const auto window = [&](std::initializer_list<SDL_Keycode> keys, const WallArchives& sources) {
        assert(SDL_Init(SDL_INIT_VIDEO));
        for (const auto code : keys) {
            SDL_Event event{}; event.type = SDL_EVENT_KEY_DOWN; event.key.key = code;
            assert(SDL_PushEvent(&event));
        }
        assert(run_exploration_window(session,"synthetic-walls",save,&initial_graphics,nullptr,nullptr,{},nullptr,&sources) == 0);
        SDL_Quit();
    };
    session = {}; session.dialect = dialect; session.opcodes = ops;
    session.ecl_record = program({0x37,0,7,0,255,0,255,9,0,42,1,1,0x4a,0});
    window({SDLK_SPACE,SDLK_F5,SDLK_ESCAPE},walls);
    assert((session.wall_banks == std::array<std::uint8_t,3>{7,255,255}));
    assert(session.state.memory[0x4a01] == 42);
    session.ecl_record = program({0x37,0,255,0,7,0,255,9,0,99,1,1,0x4a,0});
    window({SDLK_SPACE,SDLK_F9,SDLK_ESCAPE},walls);
    assert((session.wall_banks == std::array<std::uint8_t,3>{7,255,255}));
    assert(session.state.memory[0x4a01] == 42);
    session.ecl_record = program({0x37,0,8,0,255,0,255,9,0,73,1,2,0x4a,0});
    window({SDLK_SPACE,SDLK_ESCAPE},walls);
    assert((session.wall_banks == std::array<std::uint8_t,3>{7,255,255}));
    assert(session.state.memory[0x4a02] == 0); // Missing bank stops before this write.
    const WallArchives missing{archive(8,std::vector<std::uint8_t>(780,2)),archive(8,part(5)),walls.shared};
    session.wall_banks = std::array<std::uint8_t,3>{8,255,255};
    session.row = 2; session.state.memory[0x4a01] = 99;
    window({SDLK_F9,SDLK_ESCAPE},missing);
    assert(session.row == 2 && session.state.memory[0x4a01] == 99); // Failed visual restore is atomic.
    assert((session.wall_banks == std::array<std::uint8_t,3>{8,255,255}));
    std::filesystem::remove(save);
    session = {}; session.map.dax_record_id = 6; session.dialect = dialect;
    session.opcodes = minimal_vm_opcodes(EclImageDialect::sprite3,EclAreaDialect::map3);
    session.maps = MapResources{[](std::uint8_t id) {
        if (id != 6 && id != 9) throw std::runtime_error("Missing synthetic map");
        formats::GeoMap map; map.dax_record_id = id;
        if (id == 9) { map.cells[0].north = 1; map.cells[0].raw_event = 0x81; }
        return map;
    },"synthetic-window-maps-v1"};
    session.ecl_record = program({0x21,0,9,0,2,0,255,0x37,0,7,0,255,0,255,9,0,42,1,1,0x4a,0});
    window({SDLK_SPACE,SDLK_F5,SDLK_ESCAPE},walls);
    assert(session.map.dax_record_id == 9 && session.state.memory[0x4a01] == 42);
    assert(session.state.memory[dialect.square_info_address] == 0x81);
    session.load_area({6,2,255});
    assert(session.map.dax_record_id == 6);
    window({SDLK_F9,SDLK_ESCAPE},walls);
    assert(session.map.dax_record_id == 9 && session.map.cells[0].north == 1);
    assert(session.state.memory[0x4a01] == 42);
    std::filesystem::remove(save);
#endif
}
