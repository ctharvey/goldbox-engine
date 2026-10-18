// SPDX-License-Identifier: GPL-3.0-or-later
// Optional, read-only original-resource check. No game bytes or text are output.
#include "goldbox/game_profile.hpp"
#include "goldbox/image_presentation.hpp"
#include <cstdlib>
#include <iostream>
#include <map>
#include <stdexcept>

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
        session.opcodes = minimal_vm_opcodes(EclImageDialect::sprite3,EclAreaDialect::map3,EclPartyDialect::load_character);
        session.campaign_start = std::array<std::uint8_t,2>{config.map_record,config.program_record};
        session.position_refresh_call = config.position_refresh_call;
        session.sound_call = profile.sound_call; session.party_memory = profile.party_memory;
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
        std::cout << "{\"status\":\"original-resource-replay\",\"steps\":" << run.steps
            << ",\"texts\":" << host.texts << ",\"menus\":" << host.menus
            << ",\"column\":" << unsigned(session.column) << ",\"row\":" << unsigned(session.row)
            << ",\"facing\":" << unsigned(session.facing) << ",\"bytecodeOffset\":" << run.bytecode_offset
            << ",\"codeBase\":" << dispatch.code_base << ",\"programRecord\":" << unsigned(*session.current_program_record)
            << ",\"flags\":{\"18967\":" << unsigned(session.state.memory[0x4a17])
            << ",\"19140\":" << unsigned(session.state.memory[0x4ac4])
            << ",\"19141\":" << unsigned(session.state.memory[0x4ac5])
            << "},\"saveRoundTrip\":true,\"deterministicReplay\":true,\"fullParityVerified\":false}\n";
        return 0;
    } catch (const std::exception& failure) { std::cerr << failure.what() << '\n'; return 1; }
}
