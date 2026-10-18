// SPDX-License-Identifier: GPL-3.0-or-later
#include "view.hpp"
#include <SDL3/SDL.h>
#define SDL_MAIN_HANDLED
#include <SDL3/SDL_main.h>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <algorithm>
#include <cmath>
#include <limits>
#include <iostream>

using namespace goldbox;
namespace {
void near(double actual, double expected) { assert(std::abs(actual-expected) < 0.00001); }
void key(SDL_Keycode code) {
    SDL_Event event{};
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.key = code;
    assert(SDL_PushEvent(&event));
}
}
int main(int argc, char** argv) {
    assert(argc == 2 || argc == 4);
    std::deque<std::string> dialogue;
    std::vector<std::deque<std::string>> pages;
    const auto page = [&] { pages.push_back(dialogue); };
    append_dialogue(dialogue,"HELLO",true,12,2,page);
    append_dialogue(dialogue," WORLD",false,12,2,page);
    assert(dialogue == std::deque<std::string>{"HELLO WORLD"} && pages.empty());
    append_dialogue(dialogue,"ALPHA BETA",true,8,2,page);
    assert((dialogue == std::deque<std::string>{"ALPHA","BETA"}));
    append_dialogue(dialogue," GAMMA",false,8,2,page);
    assert(pages.size() == 1 && (pages.front() == std::deque<std::string>{"ALPHA","BETA"}));
    assert(dialogue == std::deque<std::string>{"GAMMA"});
    append_dialogue(dialogue," DELTA\nEPSILON",false,8,2,page);
    assert(pages.size() == 2 && (pages.back() == std::deque<std::string>{"GAMMA","DELTA"}));
    assert(dialogue == std::deque<std::string>{"EPSILON"});
    append_dialogue(dialogue,"A\n\nB",true,8,4,page);
    assert((dialogue == std::deque<std::string>{"A","","B"}));
    append_dialogue(dialogue,std::string(100,' ')+"Z",true,8,4,page);
    assert(dialogue == std::deque<std::string>{"Z"});
    append_dialogue(dialogue,"ABCDEFGHIJ",true,4,2,page);
    assert((pages.back() == std::deque<std::string>{"ABCD","EFGH"}));
    assert(dialogue == std::deque<std::string>{"IJ"});
    for (const auto dimensions : {std::pair{0,2},std::pair{81,2},std::pair{8,0},std::pair{8,61}}) {
        bool rejected = false;
        try { append_dialogue(dialogue,"X",true,dimensions.first,dimensions.second,page); }
        catch (const std::runtime_error&) { rejected = true; }
        assert(rejected);
    }
    DialoguePage styled;
    std::vector<DialoguePage> colored_pages;
    const auto colored_page = [&] { colored_pages.push_back(styled); };
    append_dialogue(styled,"%FHELLO %CWORLD",true,8,2,colored_page);
    assert((styled.lines == std::deque<std::string>{"HELLO","WORLD"}));
    assert(styled.ink[0] == std::vector<std::uint8_t>(5,15));
    assert(styled.ink[1] == std::vector<std::uint8_t>(5,12));
    append_dialogue(styled," %BAGAIN",false,8,2,colored_page);
    assert(colored_pages.size() == 1 && colored_pages[0].ink[1] == std::vector<std::uint8_t>(5,12));
    assert(styled.lines == std::deque<std::string>{"AGAIN"});
    assert(styled.ink[0] == std::vector<std::uint8_t>(5,11));
    append_dialogue(styled,"STILL",true,8,2,colored_page); // CLEAR retains the last text color.
    assert(styled.ink[0] == std::vector<std::uint8_t>(5,11));
    append_dialogue(styled,"%0G%% %Z %",true,12,2,colored_page);
    assert(styled.lines == std::deque<std::string>{"G% %Z %"});
    assert(styled.ink[0] == std::vector<std::uint8_t>(7,10));
    append_dialogue(styled,"%1AB%2CD%3EF%4GH%5IJ",true,4,2,colored_page);
    assert((colored_pages.back().lines == std::deque<std::string>{"ABCD","EFGH"}));
    assert((colored_pages.back().ink[0] == std::vector<std::uint8_t>{1,1,2,2}));
    assert((colored_pages.back().ink[1] == std::vector<std::uint8_t>{3,3,4,4}));
    assert(styled.lines == std::deque<std::string>{"IJ"});
    assert(styled.ink[0] == std::vector<std::uint8_t>(2,5));
    append_dialogue(styled,"%A\n\n%FX",true,8,3,colored_page);
    assert((styled.lines == std::deque<std::string>{"","","X"}));
    assert(styled.ink[0].empty() && styled.ink[1].empty() && styled.ink[2][0] == 15);
    append_dialogue(styled,"%6",true,8,2,colored_page);
    assert(styled.lines.empty() && styled.ink.empty() && styled.color == 6);
    append_dialogue(styled,"NEXT",false,8,2,colored_page);
    assert(styled.ink[0] == std::vector<std::uint8_t>(4,6));
    for (std::size_t i = 0; i < 16; ++i) {
        const auto control = std::string("%")+"0123456789ABCDEF"[i]+"X";
        append_dialogue(styled,control,true,8,2,colored_page);
        assert(styled.lines == std::deque<std::string>{"X"});
        assert(styled.ink[0][0] == (i ? i : 10));
    }
    append_dialogue(dialogue,"%FPLAIN",true,12,2,page); // Plain helper does not interpret SSI codes.
    assert(dialogue == std::deque<std::string>{"%FPLAIN"});
    DialoguePage malformed;
    malformed.lines.push_back("X");
    bool bad_colors = false;
    try { append_dialogue(malformed,"Y",false,8,2,colored_page); }
    catch (const std::runtime_error&) { bad_colors = true; }
    assert(bad_colors);
    ExplorationSession selection;
    selection.party.resize(2);
    selection.loaded_character = 1;
    assert(party_name_color(selection,0) == 15 && party_name_color(selection,1) == 11);
    selection.party_memory.emplace(); selection.party_memory->selected_address = 0x1234;
    selection.state.memory[0x1234] = 1;
    assert(party_name_color(selection,0) == 11 && party_name_color(selection,1) == 15);
    selection.loaded_character = 0;
    assert(party_name_color(selection,1) == 15); // LOAD_CHARACTER must not move selection.
    selection.state.memory[0x1234] = 255;
    assert(party_name_color(selection,0) == 11 && party_name_color(selection,1) == 11);
    const std::string phrase = "Hello world!";
    constexpr std::uint8_t glyphs[]{8,5,12,12,15,32,23,15,18,12,4,33};
    for (std::size_t i = 0; i < phrase.size(); ++i) assert(font_glyph(phrase[i]) == glyphs[i]);
    for (const unsigned char byte : {0,31,96,123,127,128,255}) assert(font_glyph(byte) == 63);
    assert(font_glyph('@') == 0 && font_glyph('_') == 31 && font_glyph('[') == 27);
    assert(font_glyph('A') == 1 && font_glyph('z') == 26);
    assert(font_glyph(' ') == 32 && font_glyph('0') == 48 && font_glyph('?') == 63);
    std::vector<formats::IndexedImage> font(64,{8,8,std::vector<std::uint8_t>(64,0)});
    for (std::size_t i = 0; i < font.size(); ++i) font[i].pixels[i] = 15;
    const auto atlas = compose_font_atlas(font);
    assert(atlas.width == 128 && atlas.height == 32);
    for (std::size_t i = 0; i < font.size(); ++i)
        for (std::size_t y = 0; y < 8; ++y) for (std::size_t x = 0; x < 8; ++x)
            assert(atlas.pixels[((i/16)*8+y)*128+(i%16)*8+x] == font[i].pixels[y*8+x]);
    const auto rejects_font = [&](const auto& tiles) {
        bool failed = false;
        try { (void)compose_font_atlas(tiles); } catch (const std::exception&) { failed = true; }
        assert(failed);
    };
    rejects_font(std::vector<formats::IndexedImage>{});
    rejects_font(std::vector<formats::IndexedImage>(63,font[0]));
    rejects_font(std::vector<formats::IndexedImage>(257,font[0]));
    auto broken_font = font;
    broken_font[3].pixels[0] = 13; rejects_font(broken_font);
    broken_font = font; broken_font[0].width = 7; rejects_font(broken_font);
    broken_font = font; broken_font[0].pixels.pop_back(); rejects_font(broken_font);
    // A different layout and synthetic symbols exercise the shared frame adapter.
    GameProfile::OriginalDisplay layout;
    layout.canvas = {320,200}; layout.scene = {32,24,88,88};
    layout.party = {144,16,168,80}; layout.dialogue = {8,136,304,48};
    layout.commands = {0,192,320,8}; layout.position = {144,120};
    layout.frame_runs = {{8,8,0,3,0},{8,16,1,3,1}};
    const std::vector<formats::IndexedImage> symbols{{8,8,std::vector<std::uint8_t>(64,4)},
        {8,8,std::vector<std::uint8_t>(64,14)}};
    OriginalPresentation original{layout,compose_original_frame(layout,symbols)};
    assert(original.frame.pixels[8*320+8] == 4 && original.frame.pixels[15*320+31] == 4);
    assert(original.frame.pixels[39*320+8] == 14 && original.frame.pixels[40*320+8] == 0);
    assert(original.frame.pixels[24*320+32] == 0); // Scene is untouched.
    for (const auto bad : {std::array<std::uint16_t,5>{8,8,2,1,0},{312,8,0,2,0},
        {8,8,0,0,0},{8,8,0,1,2}}) {
        auto invalid = layout; invalid.frame_runs = {bad};
        bool rejected = false;
        try { (void)compose_original_frame(invalid,symbols); }
        catch (const std::runtime_error&) { rejected = true; }
        assert(rejected);
    }
    if (argc == 4) {
        const auto id = std::stoi(argv[3]);
        assert(id >= 0 && id <= 255);
        font = formats::decode_mono8_tiles(formats::DaxArchive::from_file(argv[2]).extract(id));
        (void)compose_font_atlas(font);
        std::cout << "Local font tiles=" << font.size() << "; atlas validated\n";
    }
    ExplorationSession session;
    session.row = 3;
    session.column = 4;
    near(cast_wall(session.map,3,4,0,-1).distance,3.5);
    near(cast_wall(session.map,3,4,1,0).distance,11.5);
    near(cast_wall(session.map,3,4,0,1).distance,12.5);
    near(cast_wall(session.map,3,4,-1,0).distance,4.5);
    session.map.cells[3*16+4].north = 2;
    auto hit = cast_wall(session.map,3,4,0,-1);
    near(hit.distance,0.5);
    assert(hit.wall == 2 && !hit.east_west);
    session.map.cells[3*16+5].west = 7;
    hit = cast_wall(session.map,3,4,1,0.25);
    near(hit.distance,0.5);
    assert(hit.wall == 7 && hit.east_west);
    assert(!session.move(Direction::east));
    for (const auto ray : {std::pair{0.0,0.0},std::pair{std::numeric_limits<double>::infinity(),1.0}}) {
        bool failed = false;
        try { (void)cast_wall(session.map,3,4,ray.first,ray.second); }
        catch (const std::exception&) { failed = true; }
        assert(failed);
    }
    // Every heading, corner and oblique ray is bounded; compare a near corner hit.
    near(cast_wall(session.map,0,0,-1,-1).distance,0.5);
    for (int y = 0; y < 16; ++y) for (int x = 0; x < 16; ++x)
        for (int angle = 0; angle < 360; angle += 7) {
            const double a = angle * 3.141592653589793 / 180;
            hit = cast_wall(session.map,y,x,std::cos(a),std::sin(a));
            assert(hit.distance > 0 && std::isfinite(hit.distance));
        }

    formats::WallGraphics graphics;
    const formats::IndexedImage solid{8,8,std::vector<std::uint8_t>(64,4)};
    graphics.walls[0] = formats::decode_wall_definitions(std::vector<std::uint8_t>(156,0),
        std::span(&solid,1))[0];
    graphics.present[0] = true;
    session.map = {};
    auto& origin = session.map.cells[3*16+4];
    std::uint8_t* edges[]{&origin.north,&origin.east,&origin.south,&origin.west};
    for (unsigned d = 0; d < 4; ++d) {
        origin = {};
        *edges[d] = 1;
        session.facing = static_cast<Direction>(d);
        const auto frame = compose_wall_view(session,graphics);
        assert(frame.width == 88 && frame.height == 88 && frame.pixels[44*88+44] == 4);
        assert(session.row == 3 && session.column == 4);
    }
    session.facing = Direction::north;
    origin = {2,0,0,0,0,0};
    session.map.cells[16+4].north = 1;
    graphics.present[1] = true;
    graphics.walls[1] = graphics.walls[0];
    for (auto& view : graphics.walls[1]) std::fill(view.pixels.begin(),view.pixels.end(),2);
    assert(compose_wall_view(session,graphics).pixels[44*88+44] == 2);
    graphics.walls[1][6].pixels[36*56+28] = 13;
    assert(compose_wall_view(session,graphics).pixels[44*88+44] == 4);
    origin.north = 1;
    // Transparency reveals the background; unresolved wall banks fail closed.
    for (auto& view : graphics.walls[0]) std::fill(view.pixels.begin(),view.pixels.end(),13);
    auto frame = compose_wall_view(session,graphics);
    assert(frame.pixels[20*88+44] == 0 && frame.pixels[70*88+44] == 6);
    graphics.present[0] = false;
    bool unresolved = false;
    try { (void)compose_wall_view(session,graphics); } catch (const std::exception&) { unresolved = true; }
    assert(unresolved);
    graphics.present[0] = true;

    // Color backdrops are opt-in, use the current GEO cell immediately, and
    // reflect ECL memory without changing either state or map context.
    const GameProfile::ColorBackdrop backdrop{{0x49fd,0x49fe}};
    session.state.memory[0x49fd] = 3;
    session.state.memory[0x49fe] = 2;
    constexpr std::uint8_t expected_sky[]{0,15,4,11,13,2,9,14,0,15,4,11,13,2,9,14};
    origin.raw_event = 7;
    for (unsigned i = 0; i < 16; ++i) {
        session.state.memory[0x49fd] = i;
        frame = compose_wall_view(session,graphics,&backdrop);
        assert(frame.pixels[42*88] == expected_sky[i]);
        assert(frame.pixels[43*88] == 15 && frame.pixels[44*88] == 7);
        assert(frame.pixels[45*88] == 7 && frame.pixels[46*88] == 6);
        assert(frame.pixels.back() == 6 && origin.raw_event == 7);
        assert(session.state.memory[0x49fd] == i);
    }
    origin.raw_event = 0x87;
    frame = compose_wall_view(session,graphics,&backdrop);
    assert(frame.pixels[0] == 4);
    session.state.memory[0x49fe] = 16;
    bool invalid_sky = false;
    try { (void)compose_wall_view(session,graphics,&backdrop); }
    catch (const std::exception&) { invalid_sky = true; }
    assert(invalid_sky);
    // Opaque walls cover the background, transparent pieces reveal it.
    session.state.memory[0x49fe] = 3;
    graphics.walls[0][6].pixels[36*56+28] = 5;
    frame = compose_wall_view(session,graphics,&backdrop);
    assert(frame.pixels[44*88+44] == 5 && frame.pixels[0] == 11);
    graphics.walls[0][6].pixels[36*56+28] = 13;
    session.dialect = {"ssi-five-entry-v1",0x9900,0xc04f,true,std::nullopt};
    session.opcodes = minimal_vm_opcodes();
    for (int i = 0; i < 5; ++i) session.ecl_record.insert(session.ecl_record.end(),{1,1,20,0x99});
    session.ecl_record.insert(session.ecl_record.end(),{9,0,7,1,0xfe,0x49,0});
    struct SilentHost : EclHost { void text(std::string_view,bool) override {} } host;
    assert(session.trigger(formats::EclEventKind::search,host).exited);
    assert(compose_wall_view(session,graphics,&backdrop).pixels[0] == 14);
    const auto colors_save = serialize_exploration(session,"backdrop-synthetic");
    session.state.memory[0x49fe] = 0;
    restore_exploration(session,colors_save,"backdrop-synthetic");
    assert(compose_wall_view(session,graphics,&backdrop).pixels[0] == 14);

    // Real SDL loop with synthetic input: movement, event menu, save/restore,
    // resize-safe software rendering, and clean shutdown without a display.
    session = {};
    session.dialect = {"ssi-five-entry-v1",0x9900,0xc04f,true,std::nullopt};
    session.opcodes = minimal_vm_opcodes();
    for (int i = 0; i < 5; ++i) session.ecl_record.insert(session.ecl_record.end(),{1,1,20,0x99});
    session.ecl_record.insert(session.ecl_record.end(),{0x2b,1,3,0x4a,0,2,0x80,1,4,0x80,1,8,0});
    const std::filesystem::path save(argv[1]);
    assert(!std::filesystem::exists(save));
    SDL_SetMainReady();
    assert(SDL_Init(SDL_INIT_VIDEO));
    key(SDLK_RIGHT);
    key(SDLK_UP); // Move east; two-choice menu writes selection to memory.
    key(SDLK_DOWN);
    key(SDLK_RETURN);
    key(SDLK_F5);
    key(SDLK_LEFT);
    key(SDLK_F9);
    key(SDLK_ESCAPE);
    assert(run_exploration_window(session,"renderer-synthetic",save) == 0);
    assert(session.row == 0 && session.column == 1 && session.facing == Direction::east);
    assert(session.state.memory[0x4a03] == 1);
    assert(std::filesystem::is_regular_file(save));
    std::filesystem::remove(save);
    // Exercise indexed-to-RGBA upload and the imported graphics path as well.
    session.map.cells[1].east = 1;
    assert(SDL_Init(SDL_INIT_VIDEO));
    key(SDLK_SPACE);
    key(SDLK_RETURN);
    key(SDLK_ESCAPE);
    assert(run_exploration_window(session,"renderer-synthetic",save,&graphics,nullptr,&backdrop,font) == 0);
    assert(session.state.memory[0x4a03] == 0);
    SDL_Quit();

    // The same ECL menu and state mutation work with original presentation.
    auto original_session = session;
    original_session.ecl_record.insert(original_session.ecl_record.begin()+20,{0x11,0x81,0,0x4b});
    original_session.state.strings[0x4b00] = "%FWHITE %CRED %0GREEN %% %Z";
    assert(SDL_Init(SDL_INIT_VIDEO));
    key(SDLK_SPACE); key(SDLK_DOWN); key(SDLK_RETURN); key(SDLK_ESCAPE);
    assert(run_exploration_window(original_session,"renderer-original",save,&graphics,nullptr,&backdrop,font,
        nullptr,nullptr,{},nullptr,{},0,nullptr,nullptr,&original) == 0);
    assert(original_session.state.memory[0x4a03] == 1);
    assert(original_session.state.strings[0x4b00] == "%FWHITE %CRED %0GREEN %% %Z");
    SDL_Quit();

    // Begin must use the campaign adapter, rather than the launch preview's ECL.
    // Restart then exercises the same created party and startup route again.
    session = {};
    session.dialect = {"ssi-five-entry-v1",0x9900,0xc04f,true,std::nullopt};
    session.opcodes = minimal_vm_opcodes(EclImageDialect::sprite3);
    session.map.dax_record_id = 18;
    session.sound_call = EclSoundCall{0xba03,0x03de,{{8,10}}};
    std::vector<std::uint8_t> opening;
    for (int i = 0; i < 5; ++i) opening.insert(opening.end(),{1,1,20,0x99});
    opening.insert(opening.end(),{9,0,8,3,0xde,3,0x2d,1,3,0xba,0x0e,0,7,9,0,7,1,0x20,0x4a,0});
    session.maps = MapResources{[](std::uint8_t id) {
        formats::GeoMap map; map.dax_record_id = id; return map;
    },"synthetic-startup-maps"};
    session.programs = ProgramResources{[opening](std::uint8_t id) { return ProgramResource{id,opening}; },
        "synthetic-startup-programs"};
    session.campaign_start = std::array<std::uint8_t,2>{0,0};
    session.party.emplace_back(); session.party[0].name = "TEST";
    const std::vector<formats::IndexedImage> titles{{320,200,std::vector<std::uint8_t>(64000,0)}};
    std::vector<std::uint8_t> picture(26,0);
    picture[0] = 1; picture[5] = picture[7] = 1; picture[22] = 0x12;
    std::vector<std::uint8_t> dax{9,0,7,0,0,0,0,26,0,27,0,25};
    dax.insert(dax.end(),picture.begin(),picture.end());
    ImageArchives launch_assets{}; // Deliberately lacks the opening picture.
    CampaignGraphics campaign_assets{{formats::DaxArchive::parse(dax),{}},{}};
    assert(SDL_Init(SDL_INIT_VIDEO));
    key(SDLK_B); key(SDLK_F10); key(SDLK_B); key(SDLK_ESCAPE);
    assert(run_exploration_window(session,"startup-synthetic",save,nullptr,&launch_assets,nullptr,{},nullptr,nullptr,
        titles,nullptr,{},0,nullptr,&campaign_assets) == 0);
    assert(session.map.dax_record_id == 0 && session.current_program_record == 0);
    assert(session.state.memory[0x4a20] == 7 && session.party[0].name == "TEST");
    SDL_Quit();
}
