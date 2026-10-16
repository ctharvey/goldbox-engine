// SPDX-License-Identifier: GPL-3.0-or-later
// Attribution-ID: sdl3
// Uses unmodified, separately installed SDL3; no upstream implementation copied.
#include "view.hpp"
#include <SDL3/SDL.h>
#define SDL_MAIN_HANDLED
#include <SDL3/SDL_main.h>
#include <algorithm>
#include <cmath>
#include <deque>
#include <fstream>
#include <memory>
#include <stdexcept>

namespace goldbox {
namespace {
void checked(bool ok) {
    if (!ok) throw std::runtime_error(SDL_GetError());
}
class WindowHost final : public EclHost {
public:
    ExplorationSession& session;
    SDL_Renderer* renderer;
    bool running{true};
    std::deque<std::string> lines;
    std::vector<std::string> choices;
    std::size_t selected{};

    WindowHost(ExplorationSession& value, SDL_Renderer* target) : session(value), renderer(target) {}
    void color(int r, int g, int b) {
        checked(SDL_SetRenderDrawColor(renderer, r, g, b, 255));
    }
    void rectangle(float x, float y, float w, float h) {
        const SDL_FRect rect{x,y,w,h};
        checked(SDL_RenderFillRect(renderer, &rect));
    }
    void label(float x, float y, const std::string& value) {
        checked(SDL_RenderDebugText(renderer, x, y, value.c_str()));
    }
    void text(std::string_view value, bool clear) override {
        if (clear) lines.clear();
        std::string line;
        for (const unsigned char ch : value) {
            if (ch == '\n' || line.size() == 110) {
                lines.push_back(std::move(line));
                line.clear();
            }
            if (ch != '\n') line += ch >= 32 && ch < 127 ? char(ch) : '?';
            if (lines.size() > 80) lines.pop_front();
        }
        if (!line.empty()) lines.push_back(std::move(line));
        while (lines.size() > 80) lines.pop_front();
    }
    void draw() {
        color(13,18,24);
        checked(SDL_RenderClear(renderer));
        color(207,176,112);
        label(24,22,"GOLDBOX  /  EXPLORATION");
        color(145,157,167);
        label(24,42,"Procedural preview | original artwork and campaign parity unverified");
        color(28,37,48); rectangle(24,72,600,180);
        color(49,43,37); rectangle(24,252,600,180);
        const double dx[]{0,1,0,-1}, dy[]{-1,0,1,0};
        const auto facing = static_cast<unsigned>(session.facing);
        for (int x = 0; x < 600; ++x) {
            const double camera = (2.0 * (x + 0.5) / 600 - 1) * 0.66;
            const auto hit = cast_wall(session.map, session.row, session.column,
                dx[facing] - dy[facing] * camera, dy[facing] + dx[facing] * camera);
            const float height = std::min(360.0, 360.0 / hit.distance);
            const double shade = (hit.east_west ? 0.76 : 1.0) / (1 + hit.distance * 0.13);
            const int tint = hit.wall % 4;
            color(int((154 + tint * 12) * shade), int((139 + tint * 5) * shade), int(112 * shade));
            // Overlap adjacent strips to avoid subpixel cracks when the software
            // backend scales the logical view to fractional physical pixels.
            const float width = x == 599 ? 1.0f : 2.0f;
            rectangle(24.0f + x, 252 - height / 2, width, height);
            // A thin cap gives untextured walls a readable silhouette.
            color(int(196 * shade), int(173 * shade), int(132 * shade));
            rectangle(24.0f + x, 252 - height / 2, width, 2);
        }
        color(207,176,112); label(664,72,"MAP OVERVIEW (REVEALED)");
        for (int row = 0; row < 16; ++row) for (int col = 0; col < 16; ++col) {
            const auto& cell = session.map.cell(row,col);
            const float x = 664 + col * 16, y = 94 + row * 16;
            color(28,37,45); rectangle(x,y,15,15);
            color(126,123,104);
            if (cell.north || row == 0) rectangle(x,y,16,1);
            if (cell.south || row == 15) rectangle(x,y+15,16,1);
            if (cell.west || col == 0) rectangle(x,y,1,16);
            if (cell.east || col == 15) rectangle(x+15,y,1,16);
        }
        const float px = 672 + session.column * 16, py = 102 + session.row * 16;
        color(233,194,104); rectangle(px-3,py-3,6,6);
        checked(SDL_RenderLine(renderer,px,py,float(px+dx[facing]*7),float(py+dy[facing]*7)));
        const char* directions[]{"NORTH","EAST","SOUTH","WEST"};
        label(664,370,"ROW " + std::to_string(session.row) + "  COL " + std::to_string(session.column));
        label(664,390,std::string("FACING ") + directions[facing]);
        color(145,157,167);
        label(24,448,"ARROWS/WASD move + turn | SPACE search | R rest | I initialize");
        label(24,464,"F5 save | F9 load | ESC quit | starts at (0,0), empty state");
        color(222,221,204);
        const auto first = lines.size() > 7 ? lines.size() - 7 : 0;
        for (std::size_t i = first; i < lines.size(); ++i) label(24,492 + float(i-first)*16,lines[i]);
        if (!choices.empty()) {
            color(22,29,37); rectangle(50,118,548,280);
            color(207,176,112); label(66,134,"CHOOSE / UP + DOWN, ENTER");
            const auto start = selected >= 8 ? selected - 7 : 0;
            for (std::size_t i = start; i < choices.size() && i < start + 8; ++i) {
                color(i == selected ? 233 : 160,i == selected ? 194 : 171,i == selected ? 104 : 180);
                label(66,162+float(i-start)*26,(i == selected ? "> " : "  ") + choices[i].substr(0,62));
            }
        }
        checked(SDL_RenderPresent(renderer));
    }
    std::size_t menu(std::span<const std::string> values, std::string_view prompt, bool) override {
        if (values.empty()) throw std::runtime_error("Empty event menu");
        text(prompt,false);
        choices.assign(values.begin(),values.end());
        selected = 0;
        while (running) {
            draw();
            SDL_Event event;
            checked(SDL_WaitEvent(&event));
            if (event.type == SDL_EVENT_QUIT ||
                (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)) {
                running = false;
                break;
            }
            if (event.type != SDL_EVENT_KEY_DOWN || event.key.repeat) continue;
            if (event.key.key == SDLK_UP) selected = (selected + choices.size()-1) % choices.size();
            if (event.key.key == SDLK_DOWN) selected = (selected+1) % choices.size();
            if (event.key.key == SDLK_RETURN) {
                choices.clear();
                return selected;
            }
        }
        choices.clear();
        throw std::runtime_error("Window closed during event menu");
    }
};
} // namespace

int run_exploration_window(ExplorationSession& session, std::string_view identity,
    const std::filesystem::path& save) {
    SDL_SetMainReady();
    checked(SDL_Init(SDL_INIT_VIDEO));
    struct Quit { ~Quit() { SDL_Quit(); } } quit;
    SDL_Window* raw_window{};
    SDL_Renderer* raw_renderer{};
    checked(SDL_CreateWindowAndRenderer("Goldbox Engine - exploration preview",960,640,
        SDL_WINDOW_RESIZABLE,&raw_window,&raw_renderer));
    std::unique_ptr<SDL_Window,decltype(&SDL_DestroyWindow)> window(raw_window,SDL_DestroyWindow);
    std::unique_ptr<SDL_Renderer,decltype(&SDL_DestroyRenderer)> renderer(raw_renderer,SDL_DestroyRenderer);
    checked(SDL_SetWindowPosition(raw_window,SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED));
    checked(SDL_SetRenderLogicalPresentation(raw_renderer,960,640,SDL_LOGICAL_PRESENTATION_LETTERBOX));
    WindowHost host(session,raw_renderer);
    host.text("Explore a map. Initialize runs ECL explicitly; campaign startup is not yet implemented.",false);
    const auto trigger = [&](formats::EclEventKind kind) {
        const auto result = session.trigger(kind,host);
        if (!result.exited) host.text("Event stopped: " + result.diagnostic,false);
    };
    while (host.running) {
        host.draw();
        SDL_Event event;
        checked(SDL_WaitEvent(&event));
        if (event.type == SDL_EVENT_QUIT) break;
        if (event.type != SDL_EVENT_KEY_DOWN || event.key.repeat) continue;
        const auto key = event.key.key;
        if (key == SDLK_ESCAPE) break;
        try {
            if (key == SDLK_LEFT || key == SDLK_A || key == SDLK_RIGHT || key == SDLK_D) {
                const int turn = key == SDLK_LEFT || key == SDLK_A ? 3 : 1;
                session.facing = static_cast<Direction>((unsigned(session.facing)+turn)%4);
            } else if (key == SDLK_UP || key == SDLK_W || key == SDLK_DOWN || key == SDLK_S) {
                const auto facing = session.facing;
                const bool backwards = key == SDLK_DOWN || key == SDLK_S;
                const bool moved = session.move(static_cast<Direction>((unsigned(facing)+(backwards ? 2 : 0))%4));
                session.facing = facing;
                if (moved) trigger(formats::EclEventKind::move);
                else host.text("Movement blocked.",false);
            } else if (key == SDLK_SPACE) trigger(formats::EclEventKind::search);
            else if (key == SDLK_R) trigger(formats::EclEventKind::rest);
            else if (key == SDLK_I) trigger(formats::EclEventKind::initialize);
            else if (key == SDLK_F5) {
                if (std::filesystem::exists(save) || std::filesystem::is_symlink(save))
                    throw std::runtime_error("Refusing to overwrite an existing save");
                std::ofstream output(save,std::ios::binary);
                output << serialize_exploration(session,identity) << '\n';
                if (!output) throw std::runtime_error("Failed writing save");
                host.text("State saved.",false);
            } else if (key == SDLK_F9) {
                if (std::filesystem::is_symlink(save) || !std::filesystem::is_regular_file(save) ||
                    std::filesystem::file_size(save) > 4*1024*1024) throw std::runtime_error("Invalid save file");
                std::ifstream input(save,std::ios::binary);
                if (!input) throw std::runtime_error("Cannot read save");
                const std::string json{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
                restore_exploration(session,json,identity);
                host.text("State restored.",false);
            }
        } catch (const std::exception& error) {
            host.text(error.what(),false);
        }
    }
    return 0;
}
} // namespace goldbox
