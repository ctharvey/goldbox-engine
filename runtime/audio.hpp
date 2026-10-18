// SPDX-License-Identifier: GPL-3.0-or-later
// Attribution-ID: sdl3
// Uses separately installed SDL3, without copying upstream implementation.
#pragma once
#include "goldbox/formats/audio.hpp"
#include <SDL3/SDL.h>
#include <memory>
#include <stdexcept>

namespace goldbox {
class AudioPlayer {
    std::unique_ptr<SDL_AudioStream,decltype(&SDL_DestroyAudioStream)> stream_{nullptr,SDL_DestroyAudioStream};
public:
    explicit AudioPlayer(bool enabled) {
        if (!enabled) return;
        if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) throw std::runtime_error(SDL_GetError());
        const SDL_AudioSpec spec{SDL_AUDIO_F32,1,48000};
        stream_.reset(SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,&spec,nullptr,nullptr));
        if (!stream_ || !SDL_ResumeAudioStreamDevice(stream_.get())) throw std::runtime_error(SDL_GetError());
    }
    void stop() {
        if (stream_ && !SDL_ClearAudioStream(stream_.get())) throw std::runtime_error(SDL_GetError());
    }
    void play(const formats::SpeakerClip& clip) {
        if (!stream_) throw std::runtime_error("Audio device is not configured");
        const auto pcm = formats::render_speaker(clip);
        stop(); // Replacement is bounded; repeated keys cannot build a long queue.
        if (!SDL_PutAudioStreamData(stream_.get(),pcm.data(),static_cast<int>(pcm.size()*sizeof(float))))
            throw std::runtime_error(SDL_GetError());
    }
};
} // namespace goldbox
