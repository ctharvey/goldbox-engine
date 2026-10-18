// SPDX-License-Identifier: GPL-3.0-or-later
// Attribution-ID: sdl3
// Uses unmodified, separately installed SDL3; no upstream implementation copied.
#include "view.hpp"
#include "audio.hpp"
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
void require_wall_bindings(const ExplorationSession& session, const formats::WallGraphics& graphics) {
    for (const auto& cell : session.map.cells)
        for (const auto id : {cell.north,cell.east,cell.south,cell.west})
            if (id && (id > 15 || !graphics.present[id-1]))
                throw std::runtime_error("GEO wall has no selected graphics bank");
}
class WindowHost final : public EclHost {
public:
    ExplorationSession& session;
    SDL_Renderer* renderer;
    const formats::WallGraphics* graphics;
    const GameProfile::ColorBackdrop* backdrop;
    const GameProfile::Portrait* portrait_config;
    const WallArchives* wall_archives;
    std::optional<formats::WallGraphics> selected_graphics;
    std::unique_ptr<SDL_Texture,decltype(&SDL_DestroyTexture)> texture{nullptr,SDL_DestroyTexture};
    std::optional<ImagePresentation> images;
    const ImageArchives* character_assets{};
    AudioPlayer* audio{};
    const formats::SpeakerClip* preview_audio{};
    const formats::TandyClip* startup_music{};
    std::function<formats::SpeakerClip(std::uint16_t)> sounds;
    std::unique_ptr<SDL_Texture,decltype(&SDL_DestroyTexture)> image_texture{nullptr,SDL_DestroyTexture};
    std::unique_ptr<SDL_Texture,decltype(&SDL_DestroyTexture)> font_texture{nullptr,SDL_DestroyTexture};
    bool running{true};
    bool restart_requested{};
    std::deque<std::string> lines;
    DialoguePage dialogue;
    std::vector<std::string> choices;
    std::size_t selected{};
    const OriginalPresentation* original{};
    std::unique_ptr<SDL_Texture,decltype(&SDL_DestroyTexture)> frame_texture{nullptr,SDL_DestroyTexture};
    Uint64 image_clock{};
    std::mt19937 creation_random{static_cast<unsigned>(SDL_GetPerformanceCounter())};

    WindowHost(ExplorationSession& value, SDL_Renderer* target, const formats::WallGraphics* walls,
        const ImageArchives* archives, const GameProfile::ColorBackdrop* colors,
        std::span<const formats::IndexedImage> font, const GameProfile::Portrait* portrait, const WallArchives* sources,
        const OriginalPresentation* presentation)
        : session(value), renderer(target), graphics(walls), backdrop(colors), portrait_config(portrait), wall_archives(sources),
          original(presentation) {
        if (archives) images.emplace(*archives);
        if (original) {
            const auto& frame = original->frame;
            frame_texture.reset(SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_STATIC,
                frame.width,frame.height));
            if (!frame_texture) throw std::runtime_error(SDL_GetError());
            checked(SDL_SetTextureScaleMode(frame_texture.get(),SDL_SCALEMODE_NEAREST));
            const auto palette = formats::ega_palette();
            std::vector<formats::Rgba> rgba;
            for (const auto p : frame.pixels) rgba.push_back(palette.at(p));
            checked(SDL_UpdateTexture(frame_texture.get(),nullptr,rgba.data(),frame.width*4));
        }
        if (!font.empty()) {
            const auto atlas = compose_font_atlas(font);
            std::vector<formats::Rgba> rgba;
            rgba.reserve(atlas.pixels.size());
            for (const auto pixel : atlas.pixels) rgba.push_back({255,255,255,std::uint8_t(pixel ? 255 : 0)});
            font_texture.reset(SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_STATIC,128,32));
            if (!font_texture) throw std::runtime_error(SDL_GetError());
            checked(SDL_SetTextureScaleMode(font_texture.get(),SDL_SCALEMODE_NEAREST));
            checked(SDL_SetTextureBlendMode(font_texture.get(),SDL_BLENDMODE_BLEND));
            checked(SDL_UpdateTexture(font_texture.get(),nullptr,rgba.data(),128*4));
        }
        if (graphics) {
            texture.reset(SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_STATIC,88,88));
            if (!texture) throw std::runtime_error(SDL_GetError());
            checked(SDL_SetTextureScaleMode(texture.get(),SDL_SCALEMODE_NEAREST));
        }
        if (wall_archives && session.wall_banks) install_walls(load_banks(session,*session.wall_banks));
    }
    void update_image() {
        image_clock = SDL_GetTicks();
        image_texture.reset();
        const auto* frame = images ? images->frame() : nullptr;
        if (frame) {
            image_texture.reset(SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_STATIC,
                frame->image.width,frame->image.height));
            if (!image_texture) throw std::runtime_error(SDL_GetError());
            checked(SDL_SetTextureScaleMode(image_texture.get(),SDL_SCALEMODE_NEAREST));
            checked(SDL_SetTextureBlendMode(image_texture.get(),SDL_BLENDMODE_BLEND));
            const auto palette = formats::ega_palette(images->palette());
            std::vector<formats::Rgba> rgba;
            for (const auto pixel : frame->image.pixels) rgba.push_back(palette[pixel]);
            checked(SDL_UpdateTexture(image_texture.get(),nullptr,rgba.data(),frame->image.width*4));
        }
    }
    void animate() {
        const auto now = SDL_GetTicks();
        const auto elapsed = now - image_clock;
        image_clock = now;
        if (images && images->advance_time(elapsed)) update_image();
    }
    bool wait(SDL_Event& event) {
        const auto delay = images ? images->animation_wait_ms() : 0;
        if (!delay) { checked(SDL_WaitEvent(&event)); return true; }
        SDL_ClearError();
        if (SDL_WaitEventTimeout(&event,static_cast<Sint32>(std::min(delay,60000u)))) return true;
        if (*SDL_GetError()) throw std::runtime_error(SDL_GetError());
        return false;
    }
    formats::WallGraphics load_banks(const ExplorationSession& context, std::array<std::uint8_t,3> banks) {
        if (!wall_archives) throw std::runtime_error("No wall archives selected");
        auto decoded = formats::load_wall_graphics(wall_archives->definitions,wall_archives->tiles,
            wall_archives->shared,banks);
        require_wall_bindings(context,decoded);
        return decoded;
    }
    void install_walls(formats::WallGraphics decoded) {
        if (!texture) {
            texture.reset(SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_STATIC,88,88));
            if (!texture) throw std::runtime_error(SDL_GetError());
            checked(SDL_SetTextureScaleMode(texture.get(),SDL_SCALEMODE_NEAREST));
        }
        selected_graphics = std::move(decoded);
        graphics = &*selected_graphics;
    }
    void wall_banks(std::array<std::uint8_t,3> banks) override {
        install_walls(load_banks(session,banks));
        session.wall_banks = banks;
    }
    void restore(std::string_view json, std::string_view identity) {
        auto candidate = session;
        restore_exploration(candidate,json,identity);
        if (candidate.wall_banks) install_walls(load_banks(candidate,*candidate.wall_banks));
        else if (wall_archives && session.wall_banks)
            throw std::runtime_error("Save lacks wall banks; cannot restore its graphics selection");
        session = std::move(candidate);
        clear_images();
    }
    void load_save(const std::filesystem::path& save, std::string_view identity) {
        if (std::filesystem::is_symlink(save) || !std::filesystem::is_regular_file(save) ||
            std::filesystem::file_size(save) > 4*1024*1024) throw std::runtime_error("Invalid save file");
        std::ifstream input(save,std::ios::binary);
        if (!input) throw std::runtime_error("Cannot read save");
        const std::string json{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
        restore(json,identity);
    }
    // Returns 0=quit, 1=initialize exploration, 2=load engine save, 3=manage party.
    int startup(std::span<const formats::IndexedImage> screens, const std::string& error, bool show_titles,
        std::uint32_t period_ms) {
        const auto menu_screen = period_ms ? screens.size() : screens.size()-1;
        std::size_t screen = show_titles && error.empty() ? 0 : menu_screen;
        bool music_playing = startup_music && screen != menu_screen;
        if (music_playing) audio->play(*startup_music);
        std::size_t selection = 0;
        std::size_t uploaded = screens.size();
        Uint64 screen_clock = SDL_GetTicks();
        std::unique_ptr<SDL_Texture,decltype(&SDL_DestroyTexture)> title(nullptr,SDL_DestroyTexture);
        std::vector<std::string> options{"BEGIN EXPLORATION [B]", "LOAD ENGINE SAVE [L]", "EXIT [E]"};
        if (session.campaign_start) options[0] = "BEGIN GAME [B]";
        if (session.character_rules) options.insert(options.begin(),original ? "CREATE / MANAGE [C]" : "CREATE / MANAGE PARTY [C]");
        while (running) {
            const bool menu = screen == menu_screen;
            if (menu && music_playing) { audio->stop(); music_playing = false; }
            if (uploaded != screen && (!menu || !period_ms)) {
                title.reset(SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_STATIC,320,200));
                if (!title) throw std::runtime_error(SDL_GetError());
                checked(SDL_SetTextureScaleMode(title.get(),SDL_SCALEMODE_NEAREST));
                const auto palette = formats::ega_palette();
                std::vector<formats::Rgba> rgba;
                for (const auto pixel : screens[screen].pixels) rgba.push_back(palette[pixel]);
                checked(SDL_UpdateTexture(title.get(),nullptr,rgba.data(),320*4));
                uploaded = screen;
                screen_clock = SDL_GetTicks();
            }
            color(0,0,0); checked(SDL_RenderClear(renderer));
            const SDL_FRect target{0,0,original ? float(original->layout.canvas[0]) : 960,
                original ? float(original->layout.canvas[1]) : 600};
            if (!menu || !period_ms) checked(SDL_RenderTexture(renderer,title.get(),nullptr,&target));
            if (menu) {
                if (original) {
                    checked(SDL_RenderTexture(renderer,frame_texture.get(),nullptr,nullptr));
                    const auto& r = original->layout.party;
                    for (std::size_t i = 0; i < options.size(); ++i) {
                        color(i == selection ? 85 : 255,255,i == selection ? 85 : 255);
                        label(r[0],r[1]+float(i)*16,(i == selection ? "> " : "  ")+
                            options[i].substr(0,r[2]/8-2));
                    }
                } else {
                    color(0,0,0); rectangle(240,448,480,152);
                    color(207,176,112); label(264,464,"GOLDBOX ENGINE - EXPLORATION PREVIEW");
                    for (std::size_t i = 0; i < options.size(); ++i) {
                        color(i == selection ? 255 : 160,i == selection ? 220 : 160,160);
                        label(264,492+float(i)*28,std::string(i == selection ? "> " : "  ")+options[i]);
                    }
                }
            }
            color(220,220,220);
            if (original) {
                if (menu) label(0,original->layout.commands[1],"UP/DOWN ENTER  F10 RESTART");
                if (!error.empty()) label(original->layout.dialogue[0],original->layout.dialogue[1],error.substr(0,38));
            } else {
                label(24,608,menu ? "UP / DOWN, ENTER OR LETTER TO SELECT - ESC TO QUIT - F10 RESTART" :
                    period_ms ? "ANY KEY TO CONTINUE - ESC TO QUIT - F10 RESTART" :
                    "ENTER / SPACE TO CONTINUE - ESC TO QUIT - F10 RESTART");
                if (!error.empty()) { color(255,160,140); label(24,624,error.substr(0,114)); }
            }
            checked(SDL_RenderPresent(renderer));
            SDL_Event event;
            if (!menu && period_ms) {
                const auto elapsed = SDL_GetTicks()-screen_clock;
                if (elapsed >= period_ms) { ++screen; continue; }
                SDL_ClearError();
                if (!SDL_WaitEventTimeout(&event,static_cast<Sint32>(period_ms-elapsed))) {
                    if (*SDL_GetError()) throw std::runtime_error(SDL_GetError());
                    continue;
                }
            } else checked(SDL_WaitEvent(&event));
            if (event.type == SDL_EVENT_QUIT ||
                (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)) return 0;
            if (event.type != SDL_EVENT_KEY_DOWN || event.key.repeat) continue;
            const auto key = event.key.key;
            if (key == SDLK_F8 && audio && preview_audio) { audio->play(*preview_audio); continue; }
            if (key == SDLK_F10) { restart_requested = true; return 0; }
            if (!menu) {
                if (period_ms || key == SDLK_RETURN || key == SDLK_SPACE) ++screen;
            } else {
                if (key == SDLK_C && session.character_rules) return 3;
                if (key == SDLK_B) return 1;
                if (key == SDLK_L) return 2;
                if (key == SDLK_E) return 0;
                if (key == SDLK_UP) selection = (selection+options.size()-1)%options.size();
                if (key == SDLK_DOWN) selection = (selection+1)%options.size();
                if (key == SDLK_RETURN) {
                    if (session.character_rules && selection == 0) return 3;
                    const auto action = selection-(session.character_rules ? 1 : 0);
                    return action == 2 ? 0 : static_cast<int>(action+1);
                }
            }
        }
        return 0;
    }
    void picture(std::uint8_t id) override {
        if (!images) throw std::runtime_error("No image archives selected");
        if (portrait_config && session.state.memory[portrait_config->head_id_address] != 255)
            images->portrait(session.state.memory[portrait_config->head_id_address],id,portrait_config->layout);
        else images->picture(id);
        update_image();
    }
    void sound(std::uint16_t id) override {
        // Audio is optional presentation; a muted launch still executes the event.
        if (!sounds) return;
        if (!audio) throw std::runtime_error("Sound host unavailable");
        audio->play(sounds(id));
    }
    void sprite_start(EclSpriteRequest request) override {
        if (!images) throw std::runtime_error("No image archives selected");
        std::optional<formats::EgaFrame> portrait;
        if (request.replacement_picture != 255 && portrait_config &&
            session.state.memory[portrait_config->head_id_address] != 255) {
            auto replacement = *images;
            replacement.portrait(session.state.memory[portrait_config->head_id_address],
                request.replacement_picture,portrait_config->layout);
            portrait = *replacement.frame();
        }
        images->sprite_start(request,session,std::move(portrait)); update_image();
    }
    void delay() override {
        const auto end = SDL_GetTicks() + session.script_delay_ms;
        do {
            animate(); draw();
            const auto now = SDL_GetTicks();
            if (now >= end) break;
            SDL_Event event;
            if (!SDL_WaitEventTimeout(&event,static_cast<Sint32>(std::min<Uint64>(end-now,20)))) continue;
            if (event.type == SDL_EVENT_QUIT) { running = false; throw std::runtime_error("Window closed"); }
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_F10) {
                restart_requested = true; throw std::runtime_error("Restart requested");
            }
        } while (running);
    }
    void sprite_advance() override {
        if (!images) throw std::runtime_error("No image archives selected");
        images->sprite_advance(); update_image();
    }
    void clear_images() override {
        if (images) images->clear();
        update_image();
    }
    void color(int r, int g, int b) {
        checked(SDL_SetRenderDrawColor(renderer, r, g, b, 255));
    }
    void rectangle(float x, float y, float w, float h) {
        const SDL_FRect rect{x,y,w,h};
        checked(SDL_RenderFillRect(renderer, &rect));
    }
    void label(float x, float y, const std::string& value) {
        if (!font_texture) { checked(SDL_RenderDebugText(renderer,x,y,value.c_str())); return; }
        std::uint8_t r,g,b,a;
        checked(SDL_GetRenderDrawColor(renderer,&r,&g,&b,&a));
        checked(SDL_SetTextureColorMod(font_texture.get(),r,g,b));
        const float start_x = x;
        for (const unsigned char ch : value) {
            if (ch == '\n') { x = start_x; y += 8; continue; }
            const auto glyph = font_glyph(ch);
            const SDL_FRect source{float((glyph%16)*8),float((glyph/16)*8),8,8};
            const SDL_FRect target{x,y,8,8};
            checked(SDL_RenderTexture(renderer,font_texture.get(),&source,&target));
            x += 8;
        }
    }
    void text(std::string_view value, bool clear) override {
        if (original) {
            append_dialogue(dialogue,value,clear,original->layout.dialogue[2]/8,original->layout.dialogue[3]/8,[&] {
                const std::array<std::string,1> next{"PRESS RETURN TO CONTINUE"};
                (void)choose(next,"",false);
            });
            return;
        }
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
    std::optional<std::size_t> pick(const std::vector<std::string>& values, const std::string& prompt) {
        text(prompt,true);
        return choose(values,"",true);
    }
    template<class T> std::optional<std::size_t> pick_option(const std::vector<T>& values, const char* prompt) {
        std::vector<std::string> labels;
        for (const auto& v : values) labels.push_back(v.label);
        return pick(labels,prompt);
    }
    std::string character_summary(const Character& c) {
        const auto& rules = *session.character_rules;
        std::string result = c.name + " / " + c.race + " / " + c.character_class + " / " + c.alignment + "\n";
        if (c.age) result += "AGE " + std::to_string(*c.age) + "\n";
        if (c.hit_points) result += "HP " + std::to_string(c.hit_points->current) + "/" +
            std::to_string(c.hit_points->maximum) + "\n";
        for (const auto& [currency,balance] : c.money)
            result += currency + " " + std::to_string(balance) + "\n";
        if (c.spellbook && !c.spellbook->known.empty()) result += "SPELLS " +
            std::to_string(c.spellbook->known.size()) + "   LEVEL 1 SLOTS " +
            std::to_string(c.spellbook->first_level_slots) + "\n";
        for (const auto& [name,book] : c.spellbooks) if (!book.known.empty())
            result += name + " SPELLS " + std::to_string(book.known.size()) + "   LEVEL 1 SLOTS " +
                std::to_string(book.first_level_slots) + "\n";
        for (std::size_t i = 0; i < c.abilities.size(); ++i) {
            result += rules.abilities[i].label + " " + std::to_string(c.abilities[i].value);
            if (c.abilities[i].percentile) result += "/" + (c.abilities[i].percentile == 100 ?
                std::string("00") : std::to_string(c.abilities[i].percentile));
            result += i%2 ? "\n" : "   ";
        }
        return result;
    }
    std::optional<std::string> character_name(std::size_t limit) {
        auto* window = SDL_GetRenderWindow(renderer);
        checked(SDL_StartTextInput(window));
        struct StopInput { SDL_Window* window; ~StopInput() { SDL_StopTextInput(window); } } stop{window};
        text("TYPE A NAME. ENTER ACCEPTS, ESC CANCELS, F10 RESTARTS.",true);
        std::string name;
        while (running) {
            choices = {"NAME: " + name}; selected = 0; draw();
            SDL_Event event; checked(SDL_WaitEvent(&event));
            if (event.type == SDL_EVENT_QUIT) { running = false; break; }
            if (event.type == SDL_EVENT_TEXT_INPUT) {
                for (const unsigned char c : std::string_view(event.text.text))
                    if (c >= 32 && c < 127 && name.size() < limit) name += static_cast<char>(c);
            }
            if (event.type != SDL_EVENT_KEY_DOWN || event.key.repeat) continue;
            if (event.key.key == SDLK_F10) {
                restart_requested = true; throw std::runtime_error("Restart requested during character creation");
            }
            if (event.key.key == SDLK_ESCAPE) break;
            if (event.key.key == SDLK_BACKSPACE && !name.empty()) name.pop_back();
            if (event.key.key == SDLK_RETURN && name.find_first_not_of(' ') != std::string::npos) {
                choices.clear();
                return name.substr(name.find_first_not_of(' '),name.find_last_not_of(' ')-name.find_first_not_of(' ')+1);
            }
        }
        choices.clear(); return std::nullopt;
    }
    // Preview artwork without replacing the event image or its animation state.
    struct PortraitPreview {
        WindowHost& host;
        ImagePresentation saved;
        decltype(image_texture) texture;
        explicit PortraitPreview(WindowHost& value) : host(value),saved(std::move(*value.images)),
            texture(std::move(value.image_texture)) {
            if (value.character_assets) {
                value.images.reset(); value.images.emplace(*value.character_assets);
            }
        }
        ~PortraitPreview() {
            host.images.reset(); host.images.emplace(std::move(saved));
            host.image_texture = std::move(texture); host.image_clock = SDL_GetTicks();
        }
    };
    void show_character(const Character& c) {
        std::optional<PortraitPreview> preview;
        if (c.portrait && images && portrait_config) {
            preview.emplace(*this);
            images->portrait(c.portrait->head,c.portrait->body,portrait_config->layout); update_image();
        }
        (void)pick({"BACK"},character_summary(c));
    }
    void create_character() {
        const auto& rules = *session.character_rules;
        if (session.party.size() >= rules.party_limit) throw std::runtime_error("Party is full");
        Character c;
        const auto race = pick_option(rules.races,"CREATE CHARACTER / RACE"); if (!race) return;
        c.race = rules.races[*race].id;
        const auto gender = pick_option(rules.genders,"CREATE CHARACTER / GENDER"); if (!gender) return;
        c.gender = rules.genders[*gender].id;
        std::optional<std::size_t> class_index;
        std::vector<std::string> labels;
        const auto pick_class = [&] {
            const auto available = creation_classes(rules,c);
            labels.clear();
            for (const auto i : available) labels.push_back(rules.classes[i].label);
            const auto selected = pick(labels,"CREATE CHARACTER / CLASS");
            if (!selected) return false;
            class_index = available[*selected]; c.character_class = rules.classes[*class_index].id;
            return true;
        };
        const auto pick_alignment = [&] {
            const auto& class_rule = rules.classes[*class_index];
            labels.clear();
            for (const auto& a : class_rule.alignments) {
                const auto it = std::find_if(rules.alignments.begin(),rules.alignments.end(),
                    [&](const auto& v) { return v.id == a; });
                labels.push_back(it->label);
            }
            const auto selected = pick(labels,"CREATE CHARACTER / ALIGNMENT");
            if (!selected) return false;
            c.alignment = class_rule.alignments[*selected]; return true;
        };
        if (!rules.races[*race].starting_ages.empty()) {
            for (const auto value : rules.races[*race].maximum.at(c.gender)) c.abilities.push_back({value,0});
            if (!pick_class()) return;
            if (!pick_alignment()) return;
            if (session.initial_random && !session.state.random)
                throw std::runtime_error("This legacy save has no random state; start a new seeded game");
        }
        while (true) {
            if (session.initial_random && !session.state.random)
                throw std::runtime_error("This legacy save has no random state; start a new seeded game to roll characters");
            if (class_index && !rules.races[*race].starting_ages.empty())
                c = session.state.random ? roll_character(rules,c,*session.state.random) :
                    roll_character(rules,c,creation_random);
            else c.abilities = session.state.random ? roll_abilities(rules,c.race,c.gender,*session.state.random) :
                roll_abilities(rules,c.race,c.gender,creation_random);
            const auto available = creation_classes(rules,c);
            const bool qualifies = !class_index ||
                std::find(available.begin(),available.end(),*class_index) != available.end();
            const auto choice = qualifies ? pick({"KEEP SCORES","REROLL","CANCEL"},character_summary(c)) :
                pick({"REROLL","CANCEL"},character_summary(c)+"\nCLASS REQUIREMENTS NOT MET");
            if (!choice || (qualifies ? *choice == 2 : *choice == 1)) return;
            if (qualifies && *choice == 0) break;
        }
        if (!class_index && !pick_class()) return;
        if (c.alignment.empty() && !pick_alignment()) return;
        const auto name = character_name(rules.name_limit); if (!name) return;
        c.name = *name;
        if (c.age) validate_character(rules,c); // Prepared rolls already include the percentile at its native point.
        else c = session.state.random ? finish_character(rules,std::move(c),*session.state.random) :
            finish_character(rules,std::move(c),creation_random);
        std::optional<PortraitPreview> preview;
        if (rules.portrait) {
            if (!images || !portrait_config) throw std::runtime_error("Character portrait artwork is not loaded");
            preview.emplace(*this);
            const auto& parts = *rules.portrait;
            // Validate every offered resource before presenting a partially usable picker.
            for (const auto head_id : parts.heads) images->portrait(head_id,parts.bodies.front(),portrait_config->layout);
            for (const auto body_id : parts.bodies) images->portrait(parts.heads.front(),body_id,portrait_config->layout);
            auto head = std::find(parts.heads.begin(),parts.heads.end(),c.portrait ? c.portrait->head : parts.heads.front());
            auto body = std::find(parts.bodies.begin(),parts.bodies.end(),c.portrait ? c.portrait->body : parts.bodies.front());
            if (head == parts.heads.end() || body == parts.bodies.end()) throw std::runtime_error("Unknown character portrait");
            std::size_t h = head-parts.heads.begin(), b = body-parts.bodies.begin();
            choices = {"H: NEXT HEAD","B: NEXT BODY","K: KEEP PORTRAIT"}; selected = 2;
            bool keep = false;
            while (running && !keep) {
                images->portrait(parts.heads[h],parts.bodies[b],portrait_config->layout); update_image();
                text("CHOOSE PORTRAIT\nHEAD " + std::to_string(h+1) + " / BODY " + std::to_string(b+1),true);
                draw(); SDL_Event event{};
                if (!wait(event)) continue;
                if (event.type == SDL_EVENT_QUIT) { running = false; break; }
                if (event.type != SDL_EVENT_KEY_DOWN) continue;
                const auto key = event.key.key;
                if (key == SDLK_F10) {
                    restart_requested = true; throw std::runtime_error("Restart requested during portrait selection");
                }
                if (key == SDLK_ESCAPE) { choices.clear(); return; }
                if (key == SDLK_UP) selected = (selected+choices.size()-1)%choices.size();
                if (key == SDLK_DOWN) selected = (selected+1)%choices.size();
                if (key == SDLK_H || (key == SDLK_RETURN && selected == 0)) h = (h+1)%parts.heads.size();
                if (key == SDLK_B || (key == SDLK_RETURN && selected == 1)) b = (b+1)%parts.bodies.size();
                keep = key == SDLK_K || (key == SDLK_RETURN && selected == 2);
            }
            choices.clear(); if (!running) return;
            c.portrait = CharacterPortrait{parts.heads[h],parts.bodies[b]};
        }
        const auto confirm = pick({"ADD TO PARTY","CANCEL"},character_summary(c));
        if (confirm && *confirm == 0) add_character(session.party,rules,std::move(c));
    }
    void manage_party() {
        std::string diagnostic;
        while (running) {
            const auto action = pick({"CREATE CHARACTER","VIEW CHARACTER","REMOVE CHARACTER","BACK"},
                "PARTY " + std::to_string(session.party.size()) + "/" +
                std::to_string(session.character_rules->party_limit) + "\n" + diagnostic);
            diagnostic.clear();
            if (!action || *action == 3) return;
            try {
                if (*action == 0) { create_character(); continue; }
                if (session.party.empty()) throw std::runtime_error("Party is empty");
                std::vector<std::string> names;
                for (const auto& c : session.party) names.push_back(c.name + " / " + c.character_class);
                const auto member = pick(names,"SELECT CHARACTER"); if (!member) continue;
                const auto summary = character_summary(session.party[*member]);
                if (*action == 1) show_character(session.party[*member]);
                else {
                    const auto remove = pick({"REMOVE FROM PARTY","CANCEL"},summary);
                    if (remove && *remove == 0) {
                        session.party.erase(session.party.begin()+*member);
                        if (session.loaded_character == *member) session.loaded_character = 0;
                        else if (session.loaded_character > *member) --session.loaded_character;
                        if (session.party_memory && session.party_memory->selected_address) {
                            auto& selected = session.state.memory[*session.party_memory->selected_address];
                            if (selected == *member) selected = 0;
                            else if (selected > *member && selected <= session.party.size()) --selected;
                        }
                    }
                }
            } catch (const std::exception& error) {
                if (restart_requested || !running) throw;
                diagnostic = error.what();
            }
        }
    }
    void draw_original() {
        const auto& layout = original->layout;
        color(0,0,0); checked(SDL_RenderClear(renderer));
        checked(SDL_RenderTexture(renderer,frame_texture.get(),nullptr,nullptr));
        const auto& r = layout.scene;
        const SDL_Rect clip{r[0],r[1],r[2],r[3]};
        checked(SDL_SetRenderClipRect(renderer,&clip));
        if (graphics) {
            const auto frame = compose_wall_view(session,*graphics,backdrop);
            const auto palette = formats::ega_palette();
            std::vector<formats::Rgba> rgba;
            for (const auto p : frame.pixels) rgba.push_back(palette[p]);
            checked(SDL_UpdateTexture(texture.get(),nullptr,rgba.data(),88*4));
            const SDL_FRect target{float(r[0]),float(r[1]),float(r[2]),float(r[3])};
            checked(SDL_RenderTexture(renderer,texture.get(),nullptr,&target));
        }
        if (image_texture) {
            const auto& frame = *images->frame();
            if (images->palette() == formats::EgaPaletteKind::standard) {
                color(0,0,0); rectangle(r[0],r[1],r[2],r[3]);
            }
            const float sx = r[2]/88.0f, sy = r[3]/88.0f;
            const SDL_FRect target{r[0]+frame.x_offset*sx,r[1]+frame.y_offset*sy,
                frame.image.width*sx,frame.image.height*sy};
            checked(SDL_RenderTexture(renderer,image_texture.get(),nullptr,&target));
        }
        checked(SDL_SetRenderClipRect(renderer,nullptr));
        const auto& party = layout.party;
        color(255,255,255);
        label(party[0],party[1],"NAME");
        label(party[0]+party[2]-48,party[1],"AC");
        label(party[0]+party[2]-16,party[1],"HP");
        const auto row_height = layout.party_row_height;
        const auto palette = formats::ega_palette();
        for (std::size_t i = 0; i < session.party.size() && (i+1)*row_height+8 <= party[3]; ++i) {
            const auto ink = palette[party_name_color(session,i)];
            color(ink[0],ink[1],ink[2]);
            const float y = party[1]+float(i+1)*row_height;
            label(party[0],y,session.party[i].name.substr(0,(party[2]-56)/8));
            color(85,255,85);
            // Equipment/armor is not implemented; never invent an armor class.
            label(party[0]+party[2]-48,y,"?");
            const auto hp = session.party[i].hit_points;
            const auto health = hp ? std::to_string(hp->current) : "?";
            label(party[0]+party[2]-float(health.size()*8),y,health);
        }
        const char* directions[]{"N","E","S","W"};
        color(85,255,85);
        label(layout.position[0],layout.position[1],std::to_string(session.column)+","+
            std::to_string(session.row)+" "+directions[static_cast<unsigned>(session.facing)]);
        const auto& text = layout.dialogue;
        const SDL_Rect text_clip{text[0],text[1],text[2],text[3]};
        checked(SDL_SetRenderClipRect(renderer,&text_clip));
        const std::size_t count = text[3]/8;
        const auto first = dialogue.lines.size() > count ? dialogue.lines.size()-count : 0;
        for (std::size_t i = first; i < dialogue.lines.size(); ++i) {
            const auto& line = dialogue.lines[i];
            for (std::size_t x = 0; x < line.size(); ++x) {
                const auto ink = palette[dialogue.ink[i][x]];
                color(ink[0],ink[1],ink[2]);
                label(text[0]+float(x)*8,text[1]+float(i-first)*8,line.substr(x,1));
            }
        }
        checked(SDL_SetRenderClipRect(renderer,nullptr));
        const auto& commands = layout.commands;
        color(255,255,255);
        if (choices.size() == 1) {
            label(commands[0],commands[1],choices.front().substr(0,commands[2]/8));
        } else if (!choices.empty()) {
            color(0,0,0); rectangle(party[0],party[1],party[2],party[3]);
            const auto visible = std::max<std::size_t>(1,party[3]/16);
            const auto start = selected >= visible ? selected-visible+1 : 0;
            const std::size_t width = party[2]/8;
            for (std::size_t i = start; i < choices.size() && i < start+visible; ++i) {
                color(i == selected ? 85 : 255,255,i == selected ? 85 : 255);
                const auto option = (i == selected ? "> " : "  ")+choices[i];
                const float y = party[1]+float(i-start)*16;
                label(party[0],y,option.substr(0,width));
                if (option.size() > width) label(party[0],y+8,option.substr(width,width));
            }
            color(255,255,255); label(commands[0],commands[1],"UP/DOWN ENTER  F10 RESTART");
        } else label(commands[0],commands[1],"MOVE SEARCH REST  F5 SAVE F9 LOAD");
        checked(SDL_RenderPresent(renderer));
    }
    void draw() {
        if (original) { draw_original(); return; }
        color(13,18,24);
        checked(SDL_RenderClear(renderer));
        color(207,176,112);
        label(24,22,"GOLDBOX  /  EXPLORATION");
        color(145,157,167);
        label(24,42,graphics || images ? "Imported artwork | explicit resources; timing and parity unverified" :
            "Procedural preview | original artwork and campaign parity unverified");
        color(28,37,48); rectangle(24,72,600,180);
        color(49,43,37); rectangle(24,252,600,180);
        const double dx[]{0,1,0,-1}, dy[]{-1,0,1,0};
        const auto facing = static_cast<unsigned>(session.facing);
        if (graphics) {
            const auto frame = compose_wall_view(session,*graphics,backdrop);
            const auto palette = formats::ega_palette();
            std::vector<formats::Rgba> rgba;
            rgba.reserve(frame.pixels.size());
            for (const auto pixel : frame.pixels) rgba.push_back(palette[pixel]);
            checked(SDL_UpdateTexture(texture.get(),nullptr,rgba.data(),88*4));
            const SDL_FRect destination{144,72,360,360};
            checked(SDL_RenderTexture(renderer,texture.get(),nullptr,&destination));
        } else for (int x = 0; x < 600; ++x) {
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
        if (image_texture) {
            const auto* frame = images->frame();
            if (images->palette() == formats::EgaPaletteKind::standard) {
                color(0,0,0); rectangle(144,72,360,360);
            }
            constexpr float scale = 360.0f/88;
            const SDL_FRect destination{144+frame->x_offset*scale,72+frame->y_offset*scale,
                frame->image.width*scale,frame->image.height*scale};
            checked(SDL_RenderTexture(renderer,image_texture.get(),nullptr,&destination));
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
        if (!session.party.empty()) {
            label(664,416,"PARTY");
            for (std::size_t i = 0; i < session.party.size(); ++i)
                label(664,436+float(i)*20,(session.party[i].name+" / "+session.party[i].character_class).substr(0,35));
        }
        color(145,157,167);
        label(24,448,"ARROWS/WASD move + turn | SPACE search | R rest | I initialize");
        label(24,464,"F5 save | F9 load | F10 restart | ESC quit");
        color(222,221,204);
        const auto first = lines.size() > 7 ? lines.size() - 7 : 0;
        for (std::size_t i = first; i < lines.size(); ++i) label(24,492 + float(i-first)*16,lines[i]);
        if (!choices.empty()) {
            // Use the overview panel so choices never obscure portraits or event artwork.
            color(22,29,37); rectangle(648,72,288,360);
            color(207,176,112); label(664,88,"CHOOSE / F10 RESTART");
            label(664,104,"UP + DOWN, ENTER");
            const auto start = selected >= 8 ? selected - 7 : 0;
            for (std::size_t i = start; i < choices.size() && i < start + 8; ++i) {
                color(i == selected ? 233 : 160,i == selected ? 194 : 171,i == selected ? 104 : 180);
                const auto option = (i == selected ? "> " : "  ") + choices[i].substr(0,62);
                const float y = 132+float(i-start)*36;
                label(664,y,option.substr(0,32));
                if (option.size() > 32) label(664,y+16,option.substr(32));
            }
        }
        checked(SDL_RenderPresent(renderer));
    }
    std::optional<std::size_t> choose(std::span<const std::string> values, std::string_view prompt, bool cancelable) {
        if (values.empty()) throw std::runtime_error("Empty event menu");
        text(prompt,false);
        choices.assign(values.begin(),values.end());
        selected = 0;
        while (running) {
            animate();
            draw();
            SDL_Event event;
            if (!wait(event)) continue;
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE && cancelable) {
                choices.clear(); return std::nullopt;
            }
            if (event.type == SDL_EVENT_QUIT ||
                (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)) {
                running = false;
                break;
            }
            if (event.type != SDL_EVENT_KEY_DOWN || event.key.repeat) continue;
            if (event.key.key == SDLK_F10) {
                restart_requested = true;
                choices.clear();
                throw std::runtime_error("Restart requested during event menu");
            }
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
    std::size_t menu(std::span<const std::string> values, std::string_view prompt, bool) override {
        return *choose(values,prompt,false);
    }
};
} // namespace

int run_exploration_window(ExplorationSession& session, std::string_view identity,
    const std::filesystem::path& save, const formats::WallGraphics* graphics, const ImageArchives* images,
    const GameProfile::ColorBackdrop* backdrop, std::span<const formats::IndexedImage> font,
    const GameProfile::Portrait* portrait, const WallArchives* wall_archives,
    std::span<const formats::IndexedImage> startup_screens, const formats::SpeakerClip* preview_audio,
    std::function<formats::SpeakerClip(std::uint16_t)> sounds, std::uint32_t startup_title_period_ms,
    const formats::TandyClip* startup_music, const CampaignGraphics* campaign_graphics,
    const OriginalPresentation* original) {
    if (graphics) require_wall_bindings(session,*graphics);
    if (startup_title_period_ms > 60000) throw std::runtime_error("Invalid startup title period");
    if (startup_screens.size() > 64) throw std::runtime_error("Too many startup screens");
    if (original && (original->frame.width != original->layout.canvas[0] ||
        original->frame.height != original->layout.canvas[1] || original->frame.pixels.size() !=
        std::size_t(original->frame.width)*original->frame.height || original->layout.dialogue[2] < 8 ||
        original->layout.dialogue[3] < 8 || original->layout.party[2] < 64 || original->layout.party[3] < 16))
        throw std::runtime_error("Invalid original presentation");
    for (const auto& screen : startup_screens) {
        if (screen.width != 320 || screen.height != 200 || screen.pixels.size() != 320*200 ||
            std::any_of(screen.pixels.begin(),screen.pixels.end(),[](auto pixel) { return pixel > 15; }))
            throw std::runtime_error("Invalid startup screen");
    }
    SDL_SetMainReady();
    checked(SDL_Init(SDL_INIT_VIDEO));
    struct Quit { ~Quit() { SDL_Quit(); } } quit;
    AudioPlayer audio(preview_audio || bool(sounds) || startup_music);
    SDL_Window* raw_window{};
    SDL_Renderer* raw_renderer{};
    checked(SDL_CreateWindowAndRenderer(original ? "Goldbox Engine - original" : "Goldbox Engine - enhanced",960,
        original ? 600 : 640,
        SDL_WINDOW_RESIZABLE,&raw_window,&raw_renderer));
    std::unique_ptr<SDL_Window,decltype(&SDL_DestroyWindow)> window(raw_window,SDL_DestroyWindow);
    std::unique_ptr<SDL_Renderer,decltype(&SDL_DestroyRenderer)> renderer(raw_renderer,SDL_DestroyRenderer);
    checked(SDL_SetWindowPosition(raw_window,SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED));
    checked(SDL_SetRenderLogicalPresentation(raw_renderer,original ? original->layout.canvas[0] : 960,
        original ? original->layout.canvas[1] : 640,SDL_LOGICAL_PRESENTATION_LETTERBOX));
    bool campaign_assets_active = false;
    const auto create_host = [&] {
        const auto* active = campaign_assets_active ? campaign_graphics : nullptr;
        auto host = std::make_unique<WindowHost>(session,raw_renderer,active ? nullptr : graphics,
            active ? &active->images : images,backdrop,font,portrait,active ? &active->walls : wall_archives,original);
        host->character_assets = images;
        host->audio = &audio;
        host->preview_audio = preview_audio;
        host->startup_music = startup_music;
        host->sounds = sounds;
        return host;
    };
    const auto initial_session = session;
    while (true) {
        campaign_assets_active = false;
        auto window_host = create_host();
        if (!startup_screens.empty()) {
            std::string error;
            bool show_titles = true;
            while (true) {
                const auto action = window_host->startup(startup_screens,error,show_titles,startup_title_period_ms);
                show_titles = false;
                if (window_host->restart_requested) break;
                if (!action) return 0;
                const auto before_action = session;
                try {
                    if (action == 3) { window_host->manage_party(); continue; }
                    if (campaign_graphics && session.campaign_start) {
                        window_host.reset();
                        session.wall_banks.reset();
                        campaign_assets_active = true;
                        window_host = create_host();
                    }
                    if (action == 1) {
                        if (session.character_rules && session.party.empty())
                            throw std::runtime_error("Create at least one character before starting");
                        const auto result = session.campaign_start ? session.begin_campaign(*window_host) :
                            session.trigger(formats::EclEventKind::initialize,*window_host);
                        if (window_host->restart_requested) break;
                        if (!window_host->running) return 0;
                        if (!result.exited) throw std::runtime_error("Initialization stopped: " + result.diagnostic);
                    } else window_host->load_save(save,identity);
                    break;
                } catch (const std::exception& failure) {
                    if (window_host && window_host->restart_requested) break;
                    if (window_host && !window_host->running) return 0;
                    error = failure.what();
                    window_host.reset();
                    session = before_action;
                    campaign_assets_active = false;
                    window_host = create_host();
                }
            }
        }
        auto& host = *window_host;
        if (!original) host.text(startup_screens.empty() ?
            "Explore a map. Initialize runs ECL explicitly; campaign startup is not yet implemented." :
            "Exploration ready. Arrows move, Space searches, F5 saves, F9 loads; I reruns initialization.",false);
        const auto notify = [&](std::string_view value) { host.text(value,original != nullptr); };
        const auto trigger = [&](formats::EclEventKind kind) {
            const auto result = session.trigger(kind,host);
            if (!result.exited && !host.restart_requested) notify("Event stopped: " + result.diagnostic);
        };
        while (host.running && !host.restart_requested) {
            host.animate();
            host.draw();
            SDL_Event event;
            if (!host.wait(event)) continue;
            if (event.type == SDL_EVENT_QUIT) break;
            if (event.type != SDL_EVENT_KEY_DOWN || event.key.repeat) continue;
            const auto key = event.key.key;
            if (key == SDLK_ESCAPE) break;
            if (key == SDLK_F8 && preview_audio) { audio.play(*preview_audio); continue; }
            if (key == SDLK_F10) { host.restart_requested = true; break; }
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
                    else if (!original) notify("Movement blocked.");
                } else if (key == SDLK_SPACE) trigger(formats::EclEventKind::search);
                else if (key == SDLK_R) trigger(formats::EclEventKind::rest);
                else if (key == SDLK_I) trigger(formats::EclEventKind::initialize);
                else if (key == SDLK_F5) {
                    if (std::filesystem::exists(save) || std::filesystem::is_symlink(save))
                        throw std::runtime_error("Refusing to overwrite an existing save");
                    std::ofstream output(save,std::ios::binary);
                    output << serialize_exploration(session,identity) << '\n';
                    if (!output) throw std::runtime_error("Failed writing save");
                    notify("State saved.");
                } else if (key == SDLK_F9) {
                    host.load_save(save,identity);
                    notify("State restored.");
                }
            } catch (const std::exception& error) {
                notify(error.what());
            }
        }
        if (!host.restart_requested) return 0;
        window_host.reset();
        audio.stop();
        session = initial_session;
    }
}
} // namespace goldbox
