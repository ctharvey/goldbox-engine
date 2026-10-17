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
#include <fstream>

using namespace goldbox;
using namespace goldbox::formats;
namespace {
DaxArchive archive(std::uint8_t id, const std::vector<std::uint8_t>& raw, std::optional<std::uint8_t> second = {}) {
    std::vector<std::uint8_t> packed;
    for (std::size_t i = 0; i < raw.size(); i += 128) {
        const auto n = std::min<std::size_t>(128,raw.size()-i);
        packed.push_back(n-1);
        packed.insert(packed.end(),raw.begin()+i,raw.begin()+i+n);
    }
    std::vector<std::uint8_t> result{9,0,id,0,0,0,0,std::uint8_t(raw.size()),std::uint8_t(raw.size()>>8),
        std::uint8_t(packed.size()),std::uint8_t(packed.size()>>8)};
    result.insert(result.end(),packed.begin(),packed.end());
    if (second) {
        result[0] = 18;
        const std::vector<std::uint8_t> entry{*second,std::uint8_t(packed.size()),std::uint8_t(packed.size()>>8),0,0,
            std::uint8_t(raw.size()),std::uint8_t(raw.size()>>8),std::uint8_t(packed.size()),std::uint8_t(packed.size()>>8)};
        result.insert(result.begin()+11,entry.begin(),entry.end());
        result.insert(result.end(),packed.begin(),packed.end());
    }
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
#ifdef GOLDBOX_RENDERER
    if (argc == 3 && std::string_view(argv[1]) == "titles") {
        const auto screens = load_startup_screens(DaxArchive::from_file(argv[2]),std::array<std::uint8_t,2>{1,2});
        std::cout << "startup screens=" << screens.size() << " canvas=320x200 validated\n";
        return 0;
    }
#endif
    if (argc == 5 && std::string_view(argv[1]) == "portraits") {
        // Read-only local-resource validation; report counts, never original pixels or text.
        const auto profile = read_game_profile(argv[2]);
        assert(profile.portrait);
        ImageArchives portrait_assets{DaxArchive::from_file(argv[3]),DaxArchive::from_file(argv[4])};
        portrait_assets.portraits = PortraitArchives{portrait_assets.pictures,portrait_assets.sprites};
        assert(portrait_assets.pictures.records().size() <= 32 && portrait_assets.sprites.records().size() <= 32);
        ImagePresentation portrait_display(portrait_assets);
        unsigned combinations = 0;
        for (const auto& head : portrait_assets.pictures.records()) for (const auto& body : portrait_assets.sprites.records()) {
            portrait_display.portrait(head.id,body.id,profile.portrait->layout);
            assert(portrait_display.frame()); ++combinations;
        }
        std::cout << "portrait heads=" << portrait_assets.pictures.records().size()
            << " bodies=" << portrait_assets.sprites.records().size() << " combinations=" << combinations << " validated\n";
        return 0;
    }
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
        context.character_rules = profile.character_rules;
        context.party_memory = profile.party_memory;
        context.map = extract_geo16_map(geo,id(argv[4]));
        context.ecl_record = DaxArchive::from_file(argv[5]).extract(id(argv[6]));
        const auto& config = *profile.event_dispatch;
        context.dialect = {config.dialect,config.code_base,config.square_info_address,true,config.position_addresses};
        context.opcodes = minimal_vm_opcodes(profile.image_dialect == "ssi-images-3-v1" ? EclImageDialect::sprite3 : EclImageDialect::disabled,
            profile.area_dialect == "ssi-area-map3-v1" ? EclAreaDialect::map3 : EclAreaDialect::disabled,
            profile.party_memory ? EclPartyDialect::load_character : EclPartyDialect::disabled);
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
    auto title = part(3); title[4] = title[6] = 1;
    const auto titles = archive(7,title);
    const auto screens = load_startup_screens(titles,std::array<std::uint8_t,2>{7,7});
    assert(screens.size() == 2 && screens[0].pixels[0] == 0 && screens[0].pixels[96*320+156] == 3);
    rejects([&] { (void)load_startup_screens(titles,std::array<std::uint8_t,1>{8}); });
    rejects([&] { (void)load_startup_screens(titles,{}); });
    auto outside = title; outside[4] = 40;
    assert(load_startup_screens(archive(7,outside),std::array<std::uint8_t,1>{7})[0].pixels == screens[0].pixels);
    auto full_title = part(5); full_title[0] = 200; full_title[2] = 40;
    full_title[4] = 1; full_title.resize(17+320*200/2,0x55);
    const auto full = load_startup_screens(archive(7,full_title),std::array<std::uint8_t,1>{7});
    assert(full[0].pixels.front() == 5 && full[0].pixels.back() == 5);
    auto multi = part(3); multi[8] = 2; multi.resize(17+64,0x33);
    rejects([&] { (void)load_startup_screens(archive(7,multi),std::array<std::uint8_t,1>{7}); });
    const auto startup_save = std::filesystem::temp_directory_path()/"goldbox-startup-test-save.json";
    assert(!std::filesystem::exists(startup_save));
    session = {}; session.dialect = dialect; session.opcodes = ops;
    session.ecl_record = program({9,0,42,1,0,0x4a,0});
    const auto startup_window = [&](std::initializer_list<SDL_Keycode> keys, bool startup = true,
        const ImageArchives* portrait_images = nullptr, const GameProfile::Portrait* portrait_config = nullptr) {
        SDL_SetMainReady(); assert(SDL_Init(SDL_INIT_VIDEO));
        for (const auto code : keys) {
            SDL_Event event{};
            if (code == SDLK_UNKNOWN) {
                event.type = SDL_EVENT_TEXT_INPUT; event.text.text = "ALICE";
            } else { event.type = SDL_EVENT_KEY_DOWN; event.key.key = code; }
            assert(SDL_PushEvent(&event));
        }
        assert(run_exploration_window(session,"startup-synthetic",startup_save,nullptr,portrait_images,nullptr,{},
            portrait_config,nullptr,startup ? std::span<const IndexedImage>(screens) : std::span<const IndexedImage>{}) == 0);
        SDL_Quit();
    };
    const auto initial_startup = serialize_exploration(session,"startup-synthetic");
    startup_window({SDLK_F10,SDLK_F10,SDLK_ESCAPE});
    startup_window({SDLK_RETURN,SDLK_F10,SDLK_ESCAPE});
    startup_window({SDLK_RETURN,SDLK_RETURN,SDLK_RIGHT,SDLK_F10,SDLK_ESCAPE});
    assert(serialize_exploration(session,"startup-synthetic") == initial_startup);
    startup_window({SDLK_SPACE,SDLK_RIGHT,SDLK_F10,SDLK_ESCAPE},false);
    assert(serialize_exploration(session,"startup-synthetic") == initial_startup);
    const auto initial_program = session.ecl_record;
    session.ecl_record = program({9,0,77,1,0,0x4a,0x2b,1,3,0x4a,0,2,0x80,1,4,0x80,1,8,0});
    startup_window({SDLK_RETURN,SDLK_RETURN,SDLK_F10,SDLK_ESCAPE}); // Restart inside initialization menu.
    assert(session.state.memory[0x4a00] == 0 && session.state.memory[0x4a03] == 0);
    startup_window({SDLK_SPACE,SDLK_F10,SDLK_ESCAPE},false); // Restart inside exploration event menu.
    assert(session.state.memory[0x4a00] == 0 && session.state.memory[0x4a03] == 0);
    session.ecl_record = initial_program;
    startup_window({SDLK_ESCAPE});
    assert(session.state.memory[0x4a00] == 0); // Title escape does not initialize.
    startup_window({SDLK_RETURN,SDLK_DOWN,SDLK_DOWN,SDLK_RETURN});
    assert(session.state.memory[0x4a00] == 0); // Quit from the menu does not initialize.
    startup_window({SDLK_RETURN,SDLK_DOWN,SDLK_RETURN,SDLK_ESCAPE});
    assert(session.state.memory[0x4a00] == 0); // Missing save stays in menu.
    startup_window({SDLK_RETURN,SDLK_RETURN,SDLK_F5,SDLK_ESCAPE});
    assert(session.state.memory[0x4a00] == 42 && std::filesystem::exists(startup_save));
    session.state.memory[0x4a00] = 99;
    startup_window({SDLK_RETURN,SDLK_DOWN,SDLK_RETURN,SDLK_ESCAPE});
    assert(session.state.memory[0x4a00] == 42); // Load uses saved state.
    const auto read_startup_save = [&] {
        std::ifstream input(startup_save);
        return std::string{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
    };
    const auto saved_startup = read_startup_save();
    session.state.memory[0x4a00] = 99;
    startup_window({SDLK_RETURN,SDLK_DOWN,SDLK_RETURN,SDLK_F10,SDLK_ESCAPE});
    assert(session.state.memory[0x4a00] == 99 && read_startup_save() == saved_startup);
    session.ecl_record = program({9,0,73,1,0,0x4a,0xff});
    session.state.memory[0x4a00] = 11;
    startup_window({SDLK_RETURN,SDLK_RETURN,SDLK_ESCAPE});
    assert(session.state.memory[0x4a00] == 11); // Failed initialization rolls back before returning to menu.
    { std::ofstream invalid(startup_save); invalid << "{}"; }
    startup_window({SDLK_RETURN,SDLK_DOWN,SDLK_RETURN,SDLK_ESCAPE});
    assert(session.state.memory[0x4a00] == 11); // Failed restore does not enter exploration.
    std::filesystem::remove(startup_save);
    session = {}; session.dialect = dialect; session.opcodes = ops;
    session.ecl_record = program({9,0,42,1,0,0x4a,0});
    CreationRules creation;
    creation.id = "synthetic-party"; creation.party_limit = 2; creation.name_limit = 15;
    creation.dice_count = 1; creation.dice_sides = 2; creation.bounds = CreationBounds::clamp;
    creation.abilities = {{"might","MIGHT"}}; creation.genders = {{"any","Any"}};
    creation.alignments = {{"neutral","Neutral"}};
    creation.classes = {{"adventurer","Adventurer",{12},{"neutral"},false,{12}}};
    creation.races = {{"visitor","Visitor",{3},{{"any",{18}}},{{"any",0}},{"adventurer"}}};
    creation.races[0].starting_ages = {{"adventurer",{20,1,2}}};
    creation.initial_random_bounds = {256};
    creation.percentile_ability = 0; creation.percentile_at = 12; creation.percentile_roll_maximum = 100;
    creation.classes[0].percentile = true; creation.races[0].percentile_maximum["any"] = 100;
    creation.classes[0].starting_money = CreationMoney{"tokens",1,2,10};
    creation.classes[0].hit_points = CreationHitPoints{1,2,1,0,12,{2}};
    creation.classes[0].starting_spells = CreationSpells{{"synthetic.light"},2,0,12,{1}};
    session.initial_random.emplace(0x12345678,134775813,1);
    session.state.random = session.initial_random;
    session.character_rules = creation;
    startup_window({SDLK_RETURN,SDLK_DOWN,SDLK_RETURN,SDLK_ESCAPE});
    assert(session.party.empty() && session.state.memory[0x4a00] == 0); // Cannot begin with an empty party.
    startup_window({SDLK_RETURN,SDLK_RETURN,SDLK_RETURN,SDLK_ESCAPE,SDLK_ESCAPE,SDLK_ESCAPE});
    assert(session.party.empty()); // Cancel race selection and return to startup without closing unexpectedly.
    startup_window({SDLK_RETURN,SDLK_RETURN,SDLK_RETURN,SDLK_RETURN,SDLK_RETURN,SDLK_RETURN,
        SDLK_RETURN,SDLK_RETURN,SDLK_UNKNOWN,SDLK_RETURN,SDLK_RETURN,
        SDLK_DOWN,SDLK_DOWN,SDLK_DOWN,SDLK_RETURN,SDLK_DOWN,SDLK_RETURN,SDLK_F5,SDLK_ESCAPE});
    assert(session.party.size() == 1 && session.party[0].name == "ALICE");
    assert(session.party[0].abilities.size() == 1 && session.state.memory[0x4a00] == 42);
    assert(session.party[0].abilities[0].value == 12); // Creation-only floor permits this class from a raw 3.
    assert(session.party[0].age && *session.party[0].age >= 21 && *session.party[0].age <= 22);
    assert(session.party[0].age == 22 && session.party[0].abilities[0].percentile == 45);
    assert(session.party[0].money.at("tokens") == 20 && session.party[0].hit_points->current == 4);
    assert(session.party[0].spellbook->known == std::vector<std::string>{"synthetic.light"} &&
        session.party[0].spellbook->first_level_slots == 3);
    assert(session.state.random->state() == 0xe28f6d7a); // Money then HP; no confirmation-time redraw.
    const auto created_age = session.party[0].age;
    session.party.clear();
    startup_window({SDLK_RETURN,SDLK_DOWN,SDLK_DOWN,SDLK_RETURN,SDLK_ESCAPE});
    assert(session.party.size() == 1 && session.party[0].name == "ALICE");
    assert(session.party[0].abilities[0].value == 12);
    assert(session.party[0].age == created_age);
    assert(session.party[0].money.at("tokens") == 20 && session.party[0].hit_points->maximum == 4);
    assert(session.party[0].spellbook->known == std::vector<std::string>{"synthetic.light"} &&
        session.party[0].spellbook->first_level_slots == 3);
    assert(session.state.random->state() == 0xe28f6d7a);
    auto single_session = session;
    auto compound_creation = creation;
    auto mystic = creation.classes[0]; mystic.id = "mystic"; mystic.label = "Mystic";
    compound_creation.classes.push_back(mystic);
    auto dual = creation.classes[0]; dual.id = "dual"; dual.label = "Dual";
    dual.components = {"adventurer","mystic"}; dual.starting_money.reset(); dual.starting_spells.reset();
    dual.hit_points = CreationHitPoints{0,0,0,0,12,{2}};
    dual.starting_spellbooks = {{"divine",{{"synthetic.heal"},2,{},{},{}}},
        {"arcane",{{"synthetic.light"},1,{},{},{}}}};
    compound_creation.classes.push_back(dual);
    compound_creation.races[0].classes = {"adventurer","mystic","dual"};
    compound_creation.races[0].starting_ages["mystic"] = {20,1,2};
    compound_creation.races[0].starting_ages["dual"] = {22,0,2};
    session.party.clear(); session.state.random = session.initial_random; session.character_rules = compound_creation;
    startup_window({SDLK_RETURN,SDLK_RETURN,SDLK_RETURN,SDLK_RETURN,SDLK_RETURN,SDLK_DOWN,SDLK_DOWN,
        SDLK_RETURN,SDLK_RETURN,SDLK_RETURN,SDLK_UNKNOWN,SDLK_RETURN,SDLK_RETURN,SDLK_ESCAPE,SDLK_ESCAPE});
    if (session.party.empty()) std::cerr << "Compound UI created no party\n";
    else if (session.party[0].character_class != "dual" || session.party[0].age != 22)
        std::cerr << "Compound UI class " << session.party[0].character_class << " age " << session.party[0].age.value_or(0) << '\n';
    assert(session.party.size() == 1 && session.party[0].character_class == "dual" && session.party[0].age == 22);
    assert(session.party[0].spellbooks.at("divine").first_level_slots == 2 &&
        session.party[0].spellbooks.at("arcane").first_level_slots == 1);
    auto compound_rng = *session.initial_random;
    for (int draw = 0; draw < 7; ++draw) (void)compound_rng.draw(1);
    assert(session.state.random->state() == compound_rng.state()); // No age draw or confirmation-time rerolls.
    session = std::move(single_session);
    single_session = session;
    auto portrait_creation = creation; portrait_creation.portrait = CreationPortrait{{7,8},{9,10}};
    ImageArchives creation_assets{assets.pictures,assets.sprites};
    creation_assets.portraits = PortraitArchives{archive(7,part(3),8),archive(9,part(4),10)};
    const GameProfile::Portrait creation_layout{0,layout};
    session.party.clear(); session.state.random = session.initial_random; session.character_rules = portrait_creation;
    startup_window({SDLK_RETURN,SDLK_RETURN,SDLK_RETURN,SDLK_RETURN,SDLK_RETURN,SDLK_RETURN,SDLK_RETURN,SDLK_RETURN,
        SDLK_UNKNOWN,SDLK_RETURN,SDLK_H,SDLK_H,SDLK_B,SDLK_B,SDLK_H,SDLK_B,SDLK_K,SDLK_RETURN,
        SDLK_DOWN,SDLK_RETURN,SDLK_RETURN,SDLK_RETURN,SDLK_ESCAPE,SDLK_ESCAPE},
        true,&creation_assets,&creation_layout);
    assert(session.party.size() == 1 && session.party[0].portrait->head == 8 && session.party[0].portrait->body == 10);
    assert(session.state.random->state() == 0xe28f6d7a); // Choosing/cycling artwork consumes no RNG.
    const auto painted_save = serialize_exploration(session,"startup-synthetic"); session.party.clear();
    restore_exploration(session,painted_save,"startup-synthetic");
    assert(session.party[0].portrait->head == 8 && session.party[0].portrait->body == 10);
    session.party.clear(); session.state.random = session.initial_random;
    startup_window({SDLK_RETURN,SDLK_RETURN,SDLK_RETURN,SDLK_RETURN,SDLK_RETURN,SDLK_RETURN,SDLK_RETURN,SDLK_RETURN,
        SDLK_UNKNOWN,SDLK_RETURN,SDLK_H,SDLK_ESCAPE,SDLK_ESCAPE,SDLK_ESCAPE},true,&creation_assets,&creation_layout);
    assert(session.party.empty() && session.state.random->state() == 0xe28f6d7a);
    session.state.random = session.initial_random;
    startup_window({SDLK_RETURN,SDLK_RETURN,SDLK_RETURN,SDLK_RETURN,SDLK_RETURN,SDLK_RETURN,SDLK_RETURN,SDLK_RETURN,
        SDLK_UNKNOWN,SDLK_RETURN,SDLK_H,SDLK_F10,SDLK_ESCAPE},true,&creation_assets,&creation_layout);
    assert(session.party.empty() && session.state.random->state() == session.initial_random->state());
    // Missing artwork fails before publishing a character, with input-driven cancellation still usable.
    startup_window({SDLK_RETURN,SDLK_RETURN,SDLK_RETURN,SDLK_RETURN,SDLK_RETURN,SDLK_RETURN,SDLK_RETURN,SDLK_RETURN,
        SDLK_UNKNOWN,SDLK_RETURN,SDLK_ESCAPE,SDLK_ESCAPE});
    assert(session.party.empty());
    auto incomplete_assets = creation_assets; incomplete_assets.portraits->heads = archive(7,part(3));
    session.state.random = session.initial_random;
    startup_window({SDLK_RETURN,SDLK_RETURN,SDLK_RETURN,SDLK_RETURN,SDLK_RETURN,SDLK_RETURN,SDLK_RETURN,SDLK_RETURN,
        SDLK_UNKNOWN,SDLK_RETURN,SDLK_ESCAPE,SDLK_ESCAPE},true,&incomplete_assets,&creation_layout);
    assert(session.party.empty() && session.state.random->state() == 0xe28f6d7a); // Missing offered head fails preflight.
    session = std::move(single_session);
    auto second_member = session.party.front(); second_member.name = "BOB";
    session.party.push_back(second_member); session.loaded_character = 1;
    session.party_memory = PartyMemoryLayout{"ssi-selected-character-v1",0x5000,256,0x4fff,
        {{0x44,"might",false}},0x4ffe};
    session.opcodes = minimal_vm_opcodes(EclImageDialect::sprite3,EclAreaDialect::disabled,EclPartyDialect::load_character);
    session.ecl_record = program({0x39,0x80,1,4,0x11,0x81,0,0x50,0});
    startup_window({SDLK_RETURN,SDLK_DOWN,SDLK_RETURN,SDLK_DOWN,SDLK_RETURN,SDLK_ESCAPE});
    assert(session.loaded_character == 1 && session.state.memory[0x4ffe] == 1);
    startup_window({SDLK_RETURN,SDLK_RETURN,SDLK_DOWN,SDLK_DOWN,SDLK_RETURN,
        SDLK_RETURN,SDLK_RETURN,SDLK_ESCAPE,SDLK_ESCAPE});
    assert(session.party.size() == 1 && session.party.front().name == "BOB" && session.loaded_character == 0);
    assert(session.state.memory[0x4ffe] == 0);
    startup_window({SDLK_RETURN,SDLK_RETURN,SDLK_DOWN,SDLK_DOWN,SDLK_RETURN,
        SDLK_RETURN,SDLK_RETURN,SDLK_ESCAPE,SDLK_ESCAPE});
    assert(session.party.empty()); // Remove member, return to startup, quit.
    std::filesystem::remove(startup_save);
    auto invalid_screens = screens; invalid_screens[0].pixels[0] = 16;
    rejects([&] { (void)run_exploration_window(session,"startup-synthetic",{},nullptr,nullptr,nullptr,{},
        nullptr,nullptr,invalid_screens); });
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
    window({SDLK_SPACE,SDLK_F10,SDLK_ESCAPE},walls);
    assert(!session.wall_banks && session.state.memory[0x4a01] == 0);
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
