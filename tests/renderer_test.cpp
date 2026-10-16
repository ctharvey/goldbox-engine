// SPDX-License-Identifier: GPL-3.0-or-later
#include "view.hpp"
#include <SDL3/SDL.h>
#define SDL_MAIN_HANDLED
#include <SDL3/SDL_main.h>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <limits>

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
    assert(argc == 2);
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
    SDL_Quit();
}
