// SPDX-License-Identifier: GPL-3.0-or-later
// Optional, read-only original-resource check. No game bytes or text are output.
#include "goldbox/game_profile.hpp"
#include "goldbox/image_presentation.hpp"
#ifdef GOLDBOX_CAMPAIGN_VIEW_TESTS
#include "view.hpp"
#endif
#include <cstdlib>
#include <iostream>
#include <map>
#include <stdexcept>
#ifdef GOLDBOX_CAMPAIGN_WINDOW_TESTS
#include <SDL3/SDL.h>
static std::map<std::string,std::uint64_t> window_fingerprints;
extern "C" bool __real_SDL_RenderPresent(SDL_Renderer*);
extern "C" bool __wrap_SDL_RenderPresent(SDL_Renderer* renderer) {
    const std::pair<const char*,SDL_Rect> regions[]{
        {"topBorder",{0,0,320,8}}, {"scene",{24,24,88,88}},
        {"partyHeaders",{136,16,176,8}}, {"partyName",{136,32,120,8}},
        {"armorClass",{256,32,24,8}}, {"hitPoints",{288,32,24,8}},
        {"position",{136,120,176,8}}, {"dialogue",{8,136,304,48}}, {"commands",{0,192,320,8}}};
    auto* surface = SDL_RenderReadPixels(renderer,nullptr);
    if (!surface) throw std::runtime_error(SDL_GetError());
    for (const auto& [name,r] : regions) {
        std::uint64_t hash = 14695981039346656037ull;
        for (int y = r.y; y < r.y+r.h; ++y) for (int x = r.x; x < r.x+r.w; ++x) {
            float wx,wy; Uint8 red,green,blue,alpha;
            if (!SDL_RenderCoordinatesToWindow(renderer,x+0.5f,y+0.5f,&wx,&wy) ||
                !SDL_ReadSurfacePixel(surface,int(wx),int(wy),&red,&green,&blue,&alpha)) {
                SDL_DestroySurface(surface); throw std::runtime_error(SDL_GetError());
            }
            for (const auto channel : {red,green,blue}) { hash ^= channel; hash *= 1099511628211ull; }
        }
        window_fingerprints[name] = hash;
    }
    SDL_DestroySurface(surface);
    return __real_SDL_RenderPresent(renderer);
}
#endif

using namespace goldbox;
using namespace goldbox::formats;
namespace {
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
struct Host : EclHost {
    ExplorationSession& session;
    const GameProfile& profile;
    ImagePresentation images;
    DaxArchive definitions, tiles, shared;
    unsigned texts{}, menus{}, sprites{}, delays{}, sounds{};
    Host(ExplorationSession& s, const GameProfile& p, const ImageArchives& assets, const std::filesystem::path& game)
        : session(s), profile(p), images(assets),
          definitions(DaxArchive::from_file(game/p.campaign_startup->graphics_archives[4])),
          tiles(DaxArchive::from_file(game/p.campaign_startup->graphics_archives[5])),
          shared(DaxArchive::from_file(game/p.campaign_startup->graphics_archives[6])) {}
    void text(std::string_view, bool) override { ++texts; }
    std::size_t menu(std::span<const std::string>, std::string_view, bool) override { ++menus; return 0; }
    void picture(std::uint8_t id) override {
        const auto head = session.state.memory[profile.portrait->head_id_address];
        if (id != 255 && head != 255) images.portrait(head,id,profile.portrait->layout);
        else images.picture(id);
    }
    void sprite_start(EclSpriteRequest request) override {
        std::optional<EgaFrame> portrait;
        const auto head = session.state.memory[profile.portrait->head_id_address];
        if (head != 255 && request.replacement_picture != 255) {
            auto replacement = images;
            replacement.portrait(head,request.replacement_picture,profile.portrait->layout);
            portrait = *replacement.frame();
        }
        images.sprite_start(request,session,std::move(portrait)); ++sprites;
    }
    void sprite_advance() override { images.sprite_advance(); }
    void clear_images() override { images.clear(); }
    void delay() override { (void)images.advance_time(profile.script_delay_ms); ++delays; }
    void sound(std::uint16_t id) override {
        check(profile.speaker_audio && id < profile.speaker_audio->effect_count,"Invalid opening sound ID"); ++sounds;
    }
    void wall_banks(std::array<std::uint8_t,3> banks) override {
        const auto walls = load_wall_graphics(definitions,tiles,shared,banks);
        for (const auto& cell : session.map.cells) for (const auto id : {cell.north,cell.east,cell.south,cell.west})
            check(!id || (id <= 15 && walls.present[id-1]),"Opening wall bank missing");
        session.wall_banks = banks;
    }
};
struct MovementHost : EclHost {
    ExplorationSession& session;
    const GameProfile& profile;
    std::filesystem::path game;
    std::optional<ImageArchives> assets;
    std::optional<DaxArchive> definitions, tiles, shared;
    std::vector<std::uint8_t> bound_programs;
    MovementHost(ExplorationSession& value, const GameProfile& p, std::filesystem::path root)
        : session(value), profile(p), game(std::move(root)) {}
    void program_changed(std::uint8_t id) override {
        const auto& startup = *profile.campaign_startup;
        const auto& names = id == startup.program_record ? startup.graphics_archives : startup.program_graphics.at(id);
        const auto read = [&](unsigned i) { return DaxArchive::from_file(game/names.at(i)); };
        ImageArchives next{read(0),read(1)}; next.portraits = PortraitArchives{read(2),read(3)};
        auto defs = read(4), wall_tiles = read(5), common = read(6);
        assets = std::move(next); definitions = std::move(defs); tiles = std::move(wall_tiles); shared = std::move(common);
        bound_programs.push_back(id);
    }
    void text(std::string_view, bool) override {}
    std::size_t menu(std::span<const std::string>, std::string_view, bool) override { return 0; }
    void picture(std::uint8_t) override {}
    void sprite_start(EclSpriteRequest) override {}
    void sprite_advance() override {}
    void clear_images() override {}
    void delay() override {}
    void sound(std::uint16_t) override {}
    void native_call(std::uint16_t token) override {
        throw std::runtime_error("Unsupported boundary native call " + std::to_string(token));
    }
    void wall_banks(std::array<std::uint8_t,3> banks) override {
        check(definitions && tiles && shared,"Boundary graphics were not bound before wall loading");
        const auto walls = load_wall_graphics(*definitions,*tiles,*shared,banks);
        if (session.has_dungeon_map())
            for (const auto& cell : session.map.cells) for (const auto id : {cell.north,cell.east,cell.south,cell.west})
                check(!id || (id <= 15 && walls.present[id-1]),"Boundary wall bank missing");
        session.wall_banks = banks;
    }
};
}
int main(int argc, char** argv) {
    const char* installation = std::getenv("GOLDBOX_POOL_INSTALLATION");
    if (!installation || !*installation) { std::cout << "SKIP: set GOLDBOX_POOL_INSTALLATION to authorized game directory\n"; return 77; }
    try {
        check(argc == 2,"Expected profile path");
        const auto profile = read_game_profile(argv[1]);
        check(profile.campaign_startup && profile.character_rules && profile.random && profile.portrait,
            "Incomplete campaign profile");
        const auto game = std::filesystem::path(installation);
        const auto& config = *profile.campaign_startup;
        check(config.graphics_archives.size() == 7,"Missing startup graphics binding");
        const auto family = [&](const auto& files) {
            std::map<std::uint8_t,std::vector<std::uint8_t>> records;
            for (const auto& name : files) {
                const auto archive = DaxArchive::from_file(game/name);
                for (const auto& record : archive.records())
                    check(records.emplace(record.id,archive.extract(record.id)).second,"Duplicate resource ID");
            }
            return records;
        };
        const auto maps = family(config.map_archives), programs = family(config.program_archives);
        ExplorationSession session;
        session.maps = MapResources{[&](auto id) { return decode_geo16_record(maps.at(id),id); },"local-test-maps"};
        session.programs = ProgramResources{[&](auto id) { return ProgramResource{id,programs.at(id)}; },"local-test-programs"};
        const auto& dispatch = *profile.event_dispatch;
        session.dialect = {dispatch.dialect,dispatch.code_base,dispatch.square_info_address,true,dispatch.position_addresses};
        session.movement_addresses = dispatch.movement_addresses;
        session.transfer_clear_ranges = dispatch.transfer_clear_ranges;
        session.opcodes = minimal_vm_opcodes(EclImageDialect::sprite3,EclAreaDialect::map3,EclPartyDialect::weighted_strength);
        session.campaign_start = std::array<std::uint8_t,2>{config.map_record,config.program_record};
        session.position_refresh_call = config.position_refresh_call;
        session.position_advance_call = config.position_advance_call;
        session.sound_call = profile.sound_call; session.party_memory = profile.party_memory;
        session.party_strength = profile.party_strength;
        session.character_rules = profile.character_rules; session.initial_random = profile.random;
        session.random_dialect = profile.random_dialect;
        auto random = *profile.random;
        Character draft; draft.name = "TEST"; draft.race = "dwarf"; draft.gender = "male";
        draft.character_class = "fighter"; draft.alignment = "lawful-good";
        session.party.push_back(roll_character(*profile.character_rules,draft,random));
        session.state.random = random;
        const auto initial = session;
        ImageArchives assets{DaxArchive::from_file(game/config.graphics_archives[0]),
            DaxArchive::from_file(game/config.graphics_archives[1])};
        assets.portraits = PortraitArchives{DaxArchive::from_file(game/config.graphics_archives[2]),
            DaxArchive::from_file(game/config.graphics_archives[3])};
        assets.picture_period_ms = profile.picture_period_ms; assets.sprite_replacement_ms = profile.sprite_replacement_ms;
        Host host(session,profile,assets,game);
        const auto run = session.begin_campaign(host);
        check(run.exited,run.diagnostic.c_str());
        check(host.texts && host.menus && host.sprites && host.delays,"Opening callbacks absent");
        const auto saved = serialize_exploration(session,"private-opening-check");
        auto restored = initial;
        restore_exploration(restored,saved,"private-opening-check");
        check(serialize_exploration(restored,"private-opening-check") == saved,"Save round-trip differs");
        auto repeated = initial; Host second(repeated,profile,assets,game);
        check(repeated.begin_campaign(second).exited,"Repeated startup failed");
        check(serialize_exploration(repeated,"private-opening-check") == saved,"Deterministic startup differs");
        auto walked = session; MovementHost movement_host(walked,profile,game);
        const auto move = walked.move(Direction::west,movement_host);
        check(move.execution.has_value(),"Boundary event did not run");
        // Complete headless execution is separate from matched native party/UI parity.
        check(move.execution->exited,move.execution->diagnostic.c_str());
        check(movement_host.bound_programs == std::vector<std::uint8_t>{20},"Boundary program graphics selection differs");
        check(move.execution->bytecode_offset == 615 && walked.current_program_record == 20 &&
            walked.map.dax_record_id == 20,"Boundary execution checkpoint changed");
        check(move.moved && walked.column == 15 && walked.row == 4 && walked.facing == Direction::west,
            "Boundary probe did not reach the verified forced-step position");
        check(walked.state.memory[0x4a17] == 0 && walked.state.memory[0x4ac4] == 0 &&
            walked.state.memory[0x4ac5] == 1,"Boundary transfer temporary/global flags differ");
#ifdef GOLDBOX_CAMPAIGN_VIEW_TESTS
        const auto walls = load_wall_graphics(*movement_host.definitions,*movement_host.tiles,
            *movement_host.shared,*walked.wall_banks);
        const auto scene = compose_wall_view(walked,walls,profile.color_backdrop ? &*profile.color_backdrop : nullptr);
        const auto palette = ega_palette();
        std::uint64_t scene_hash = 14695981039346656037ull;
        for (const auto pixel : scene.pixels) for (unsigned channel = 0; channel < 3; ++channel) {
            scene_hash ^= palette.at(pixel)[channel]; scene_hash *= 1099511628211ull;
        }
        // Native 320x200 capture cropped to (24,24,88,88), RGB order, FNV-1a 64.
        // Only the fingerprint is public; original pixels remain private.
        check(scene_hash == 15040589715104877255ull,"Native boundary scene fingerprint differs");
#endif
#ifdef GOLDBOX_CAMPAIGN_WINDOW_TESTS
        auto windowed = session;
        const auto& layout = *profile.original_display;
        const auto font = decode_mono8_tiles(DaxArchive::from_file(game/layout.font_archive).extract(layout.font_record));
        const auto symbols = decode_ega_images(DaxArchive::from_file(game/layout.symbols_archive).extract(layout.symbols_record));
        const OriginalPresentation original{layout,compose_original_frame(layout,symbols.images)};
        const auto initial_walls = load_wall_graphics(host.definitions,host.tiles,host.shared,*session.wall_banks);
        auto destination = std::make_shared<const CampaignGraphics>(CampaignGraphics{*movement_host.assets,
            {*movement_host.definitions,*movement_host.tiles,*movement_host.shared}});
        const CampaignGraphicsResources graphics_sources{[&](std::uint8_t id) {
            check(id == 20,"Unexpected window graphics program"); return destination;
        }};
        check(SDL_Init(SDL_INIT_VIDEO),"SDL initialization failed");
        const auto key = [](SDL_Keycode code) {
            SDL_Event event{}; event.type = SDL_EVENT_KEY_DOWN; event.key.key = code;
            check(SDL_PushEvent(&event),"SDL input queue failed");
        };
        key(SDLK_UP);
        for (unsigned i = 0; i < 8; ++i) key(SDLK_RETURN);
        key(SDLK_ESCAPE);
        check(run_exploration_window(windowed,"private-window-check",{},&initial_walls,&assets,
            profile.color_backdrop ? &*profile.color_backdrop : nullptr,font,&*profile.portrait,nullptr,
            {},nullptr,{},0,nullptr,nullptr,&original,false,&graphics_sources) == 0,"Boundary window failed");
        check(windowed.current_program_record == 20 && windowed.column == 15 && windowed.row == 4 &&
            windowed.facing == Direction::west,"Window boundary state differs");
        check(window_fingerprints.at("scene") == scene_hash,"Window scene differs from compositor");
        check(window_fingerprints.at("topBorder") == 14742927133863922091ull,"Native top border differs");
        check(window_fingerprints.at("partyHeaders") == 10366432056459273136ull,"Native party headers differ");
        check(window_fingerprints.at("partyName") == 13181112814587718321ull,"Native party name differs");
        check(window_fingerprints.at("armorClass") == 10896576774587328698ull,"Native armor class display differs");
        check(window_fingerprints.at("dialogue") == 3056256239859949209ull,"Native boundary dialogue differs");
        check(window_fingerprints.at("position") == 13091622508662674760ull,"Native boundary clock differs");
        SDL_Quit();
#endif
        std::cout << "{\"status\":\"original-resource-replay\",\"steps\":" << run.steps
            << ",\"texts\":" << host.texts << ",\"menus\":" << host.menus
            << ",\"column\":" << unsigned(session.column) << ",\"row\":" << unsigned(session.row)
            << ",\"facing\":" << unsigned(session.facing) << ",\"bytecodeOffset\":" << run.bytecode_offset
            << ",\"codeBase\":" << dispatch.code_base << ",\"programRecord\":" << unsigned(*session.current_program_record)
            << ",\"flags\":{\"18967\":" << unsigned(session.state.memory[0x4a17])
            << ",\"19140\":" << unsigned(session.state.memory[0x4ac4])
            << ",\"19141\":" << unsigned(session.state.memory[0x4ac5])
            << "},\"saveRoundTrip\":true,\"deterministicReplay\":true,\"fullParityVerified\":false"
            << ",\"boundaryProbe\":{\"status\":\"executed\",\"diagnostic\":\"\",\"column\":"
            << unsigned(walked.column) << ",\"row\":" << unsigned(walked.row)
            << ",\"facing\":" << unsigned(walked.facing) << ",\"bytecodeOffset\":"
            << move.execution->bytecode_offset << ",\"programRecord\":"
            << unsigned(*walked.current_program_record) << ",\"mapRecord\":" << unsigned(walked.map.dax_record_id)
            << ",\"movementBlock\":" << unsigned(walked.state.memory[0x6dc9])
            << ",\"triedToLeaveMap\":" << unsigned(walked.state.memory[0x6dd5])
            << ",\"mapActive\":" << (walked.has_dungeon_map() ? "true" : "false")
#ifdef GOLDBOX_CAMPAIGN_VIEW_TESTS
            << ",\"sceneRgbFnv1a64\":\"" << scene_hash << "\""
#endif
            << ",\"flags\":{\"18967\":" << unsigned(walked.state.memory[0x4a17])
            << ",\"19140\":" << unsigned(walked.state.memory[0x4ac4])
            << ",\"19141\":" << unsigned(walked.state.memory[0x4ac5]) << "}}";
#ifdef GOLDBOX_CAMPAIGN_WINDOW_TESTS
        std::cout << ",\"windowRegionRgbFnv1a64\":{";
        bool first = true;
        for (const auto& [name,hash] : window_fingerprints) {
            if (!first) std::cout << ',';
            first = false; std::cout << '"' << name << "\":\"" << hash << '"';
        }
        std::cout << '}';
#endif
        std::cout << "}\n";
        return 0;
    } catch (const std::exception& failure) { std::cerr << failure.what() << '\n'; return 1; }
}
