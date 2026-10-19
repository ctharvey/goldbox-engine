// SPDX-License-Identifier: GPL-3.0-or-later
// Attribution-ID: sdl3
#pragma once
#include <SDL3/SDL.h>
#include <filesystem>
#include <cstdint>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <vector>

namespace goldbox {
// Bound the allocation before handing the BMP bytes to the existing SDL dependency.
// The returned texture retains full color/resolution; geometry comes from the original frame.
inline SDL_Texture* load_image_replacement(SDL_Renderer* renderer, const std::filesystem::path& file) {
    const auto size = std::filesystem::file_size(file);
    if (size < 54 || size > 32u*1024u*1024u) throw std::runtime_error("Replacement BMP size out of range");
    std::vector<unsigned char> bytes(static_cast<std::size_t>(size));
    std::ifstream stream(file,std::ios::binary);
    if (!stream.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(size)))
        throw std::runtime_error("Cannot read replacement BMP");
    const auto u32 = [&](std::size_t p) {
        return std::uint32_t(bytes[p]) | std::uint32_t(bytes[p+1])<<8 |
            std::uint32_t(bytes[p+2])<<16 | std::uint32_t(bytes[p+3])<<24;
    };
    const auto width = u32(18), height = u32(22);
    const auto bounded_height = height & 0x80000000u ? std::uint64_t(0x100000000ull-height) : height;
    if (bytes[0] != 'B' || bytes[1] != 'M' || u32(14) < 40 || u32(14) > size-14 ||
        !width || width > 2048 || !bounded_height || bounded_height > 2048)
        throw std::runtime_error("Replacement BMP must have a Windows header and dimensions 1..2048");
    auto* input = SDL_IOFromConstMem(bytes.data(),bytes.size());
    if (!input) throw std::runtime_error(SDL_GetError());
    std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)> surface(SDL_LoadBMP_IO(input,true),SDL_DestroySurface);
    if (!surface) throw std::runtime_error(SDL_GetError());
    auto* texture = SDL_CreateTextureFromSurface(renderer,surface.get());
    if (!texture) throw std::runtime_error(SDL_GetError());
    if (!SDL_SetTextureScaleMode(texture,SDL_SCALEMODE_LINEAR) ||
        !SDL_SetTextureBlendMode(texture,SDL_BLENDMODE_BLEND)) {
        SDL_DestroyTexture(texture); throw std::runtime_error(SDL_GetError());
    }
    return texture;
}
} // namespace goldbox
