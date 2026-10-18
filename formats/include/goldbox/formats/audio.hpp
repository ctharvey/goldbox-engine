// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <span>
#include <vector>

namespace goldbox::formats {
struct SpeakerFrame {
    std::uint16_t divisor{};
    bool enabled{};
    bool operator==(const SpeakerFrame&) const = default;
};
struct SpeakerClip {
    std::uint32_t clock_hz{1193181};
    std::uint16_t tick_divisor{};
    std::vector<SpeakerFrame> frames;
};
// Bounded EXEPACK B0/B2 resident-image extraction, without running game code.
// Relocations are not applied: this adapter reads resident data, not code pointers.
std::vector<std::uint8_t> unpack_exepack(std::span<const std::uint8_t> executable);
// SSI effect subset: four priority channels, duration/pitch/gate stores,
// signed sweeps, modulation and bounded counter loops. Notes/music and other
// state fields fail explicitly; no opcode scanning.
SpeakerClip decode_ssi_speaker(std::span<const std::uint8_t> segment,
    std::uint16_t table, std::uint16_t entry, std::uint16_t tick_divisor);
// Shared, deterministic mono synthesis. This is not analog speaker/filter emulation.
std::vector<float> render_speaker(const SpeakerClip& clip, std::uint32_t sample_rate = 48000);
} // namespace goldbox::formats
