// SPDX-License-Identifier: GPL-3.0-or-later
#include "goldbox/mods.hpp"
#include "goldbox/image_presentation.hpp"
#ifdef GOLDBOX_RENDERER
#include "image_replacement.hpp"
#define SDL_MAIN_HANDLED
#include <SDL3/SDL_main.h>
#endif
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <fstream>
#include <functional>
#include <iostream>

using namespace goldbox;
namespace fs = std::filesystem;
namespace {
void rejects(const std::function<void()>& fn) {
    bool failed = false;
    try { fn(); } catch (const std::exception&) { failed = true; }
    assert(failed);
}
void write(const fs::path& file, const std::string& text) { std::ofstream(file) << text; }
std::string binding(std::string file = "new.bmp", std::string archive = "pic3.dax") {
    return R"({"game":"synthetic.game","archive":")" + archive +
        R"(","kind":"picture","record":7,"frame":0,"file":")" + file + "\"}";
}
void manifest(const fs::path& directory, std::string id, std::string entries, std::string extra = "") {
    fs::create_directories(directory);
    write(directory/"mod.json",R"({"schemaVersion":1,"id":")" + id +
        R"(","version":"1.0.0","images":[)" + entries + "]" + extra + "}");
}
formats::DaxArchive archive() {
    std::vector<std::uint8_t> raw{3};
    for (unsigned i = 0; i < 3; ++i) {
        const auto at = raw.size(); raw.resize(at+25,0);
        raw[at+4] = 1; raw[at+6] = 1; raw[at+21] = 0x12+i;
    }
    std::vector<std::uint8_t> bytes{9,0,7,0,0,0,0,static_cast<std::uint8_t>(raw.size()),0,
        static_cast<std::uint8_t>(raw.size()+1),0,static_cast<std::uint8_t>(raw.size()-1)};
    bytes.insert(bytes.end(),raw.begin(),raw.end());
    return formats::DaxArchive::parse(bytes);
}
}
int main(int argc, char** argv) {
    assert(argc == 2);
    const fs::path root(argv[1]);
    assert(!fs::exists(root)); fs::create_directories(root);
    const auto a = root/"a", b = root/"b";
    manifest(a,"a",binding()); write(a/"new.bmp","synthetic placeholder");
    manifest(b,"b",binding("new.bmp","PIC3.DAX"),R"(,"requires":["a"],"priority":-10)");
    write(b/"new.bmp","synthetic placeholder");
    const auto mods = resolve_mods(discover_mods(root),{"synthetic.game","synthetic",{}});
    assert(mods.ordered.size() == 2 && mods.ordered[1].id == "b" && mods.warnings.size() == 1);
    auto library = std::make_shared<ImageLibrary>(build_image_library(mods));
    ImageKey key{"synthetic.game","pic3.dax","picture",7,0};
    assert(library->find(key,true)->mod == "b" && !library->find(key,false));
    for (auto different : {ImageKey{"other.game","PIC3.DAX","picture",7,0},
        ImageKey{"synthetic.game","PIC1.DAX","picture",7,0},
        ImageKey{"synthetic.game","PIC3.DAX","sprite",7,0},
        ImageKey{"synthetic.game","PIC3.DAX","picture",8,0},
        ImageKey{"synthetic.game","PIC3.DAX","picture",7,1}}) assert(!library->find(different,true));
    manifest(a,"a",binding()+","+binding("new.bmp","PIC3.DAX"));
    rejects([&] { (void)discover_mods(root); });
    for (const auto file : {"../outside.bmp","/absolute.bmp","new.png","missing.bmp","sub/../../outside.bmp"}) {
        manifest(a,"a",binding(file)); rejects([&] { (void)discover_mods(root); });
    }
    for (const auto bad : {"../PIC3.DAX","a/b.dax","C:pic.dax"}) {
        manifest(a,"a",binding("new.bmp",bad)); rejects([&] { (void)discover_mods(root); });
    }
    manifest(a,"a",binding());
#ifndef _WIN32
    fs::create_symlink(b/"new.bmp",a/"linked.bmp");
    manifest(a,"a",binding("linked.bmp")); rejects([&] { (void)discover_mods(root); });
    fs::remove(a/"linked.bmp"); manifest(a,"a",binding());
#endif
    ImageArchives assets{archive(),archive()};
    assets.game_id = "synthetic.game"; assets.picture_archive = "PIC3.DAX";
    assets.sprite_archive = "SPRIT3.DAX"; assets.library = library;
    assets.picture_period_ms = 10; assets.sprite_replacement_ms = 20;
    ImagePresentation images(assets);
    images.picture(7);
    auto canonical_key = key; canonical_key.archive = "PIC3.DAX";
    assert(images.source() == canonical_key);
    assert(images.replacement(true)->mod == "b" && !images.replacement(false));
    assert(images.advance_time(10) && images.source()->frame == 1 && !images.replacement(true));
    ExplorationSession sprite_session; sprite_session.row = sprite_session.column = 3;
    images.sprite_start({7,2,7},sprite_session);
    assert(images.source()->kind == "sprite" && images.source()->frame == 2);
    images.sprite_advance(); images.sprite_advance();
    assert(images.advance_time(20) && images.source()->kind == "picture" && images.source()->frame == 0);
    assert(images.replacement(true)->mod == "b");
    images.clear(); assert(!images.source() && !images.replacement(true));
    images.picture(255); assert(!images.source());
#ifdef GOLDBOX_RENDERER
    SDL_SetMainReady(); assert(SDL_Init(SDL_INIT_VIDEO));
    auto* surface = SDL_CreateSurface(176,176,SDL_PIXELFORMAT_RGBA32); assert(surface);
    assert(SDL_FillSurfaceRect(surface,nullptr,SDL_MapSurfaceRGBA(surface,123,45,67,255)));
    assert(SDL_SaveBMP(surface,(b/"new.bmp").string().c_str())); SDL_DestroySurface(surface);
    auto* target = SDL_CreateSurface(176,176,SDL_PIXELFORMAT_RGBA32); assert(target);
    auto* renderer = SDL_CreateSoftwareRenderer(target); assert(renderer);
    auto* texture = load_image_replacement(renderer,library->find(key,true)->file); assert(texture);
    float width,height; assert(SDL_GetTextureSize(texture,&width,&height));
    assert(width == 176 && height == 176);
    assert(SDL_RenderTexture(renderer,texture,nullptr,nullptr)); assert(SDL_RenderPresent(renderer));
    Uint8 r,g,blue,alpha; assert(SDL_ReadSurfacePixel(target,88,88,&r,&g,&blue,&alpha));
    assert(r == 123 && g == 45 && blue == 67 && alpha == 255);
    SDL_DestroyTexture(texture);
    rejects([&] { (void)load_image_replacement(renderer,a/"new.bmp"); });
    std::vector<unsigned char> malformed(54); malformed[0]='B'; malformed[1]='M'; malformed[14]=40;
    malformed[18]=1; malformed[22]=1; malformed[20]=1; // Width exceeds the allocation limit.
    { std::ofstream out(a/"new.bmp",std::ios::binary); out.write(reinterpret_cast<char*>(malformed.data()),54); }
    rejects([&] { (void)load_image_replacement(renderer,a/"new.bmp"); });
    SDL_DestroyRenderer(renderer); SDL_DestroySurface(target); SDL_Quit();
#endif
    fs::remove_all(root);
    std::cout << "Image library identity, overlays, path validation and presentation passed\n";
}
