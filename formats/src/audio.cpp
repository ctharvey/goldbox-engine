// SPDX-License-Identifier: GPL-3.0-or-later
#include "goldbox/formats/audio.hpp"
#include <array>
#include <stdexcept>

namespace goldbox::formats {
namespace {
std::uint16_t word(std::span<const std::uint8_t> bytes, std::size_t offset) {
    if (offset >= bytes.size() || bytes.size()-offset < 2) throw std::runtime_error("Truncated audio word");
    return bytes[offset] | (std::uint16_t(bytes[offset+1]) << 8);
}
}
std::vector<std::uint8_t> unpack_exepack(std::span<const std::uint8_t> exe) {
    if (exe.size() < 28 || exe.size() > 2*1024*1024 || word(exe,0) != 0x5a4d || word(exe,20) != 18)
        throw std::runtime_error("Unsupported EXEPACK executable");
    const std::size_t header = word(exe,8)*16u;
    const auto stub = header + word(exe,22)*16u;
    if (header < 28 || stub < header || stub+18 > exe.size() || word(exe,stub+16) != 0x4252 ||
        word(exe,stub+14) != 1) throw std::runtime_error("Invalid EXEPACK header");
    const std::size_t size = word(exe,stub+12)*16u;
    if (!size || size > 1024*1024) throw std::runtime_error("Invalid EXEPACK output size");
    std::vector<std::uint8_t> out(size);
    auto source = stub, target = size;
    while (source > header && exe[source-1] == 255) --source;
    for (std::size_t steps = 0; steps < 65536; ++steps) {
        if (source-header < 3) throw std::runtime_error("Truncated EXEPACK command");
        const auto command = exe[--source];
        source -= 2;
        const auto count = word(exe,source);
        if (!count || count > target) throw std::runtime_error("Invalid EXEPACK run");
        if ((command & 254) == 176) {
            if (source == header) throw std::runtime_error("Truncated EXEPACK fill");
            const auto value = exe[--source];
            for (std::size_t i = 0; i < count; ++i) out[--target] = value;
        } else if ((command & 254) == 178) {
            if (count > source-header) throw std::runtime_error("Truncated EXEPACK literals");
            for (std::size_t i = 0; i < count; ++i) out[--target] = exe[--source];
        } else throw std::runtime_error("Unsupported EXEPACK command");
        if (command & 1) {
            if (target != source-header) throw std::runtime_error("Invalid EXEPACK prefix");
            for (std::size_t i = 0; i < target; ++i) out[i] = exe[header+i];
            return out;
        }
    }
    throw std::runtime_error("EXEPACK command limit exceeded");
}
SpeakerClip decode_ssi_speaker(std::span<const std::uint8_t> bytes,
    std::uint16_t table, std::uint16_t entry, std::uint16_t tick_divisor) {
    if (bytes.size() > 65536 || !tick_divisor) throw std::runtime_error("Invalid speaker segment/timing");
    struct Channel { std::size_t pc{}; std::uint16_t duration{}, divisor{}, output{}, gate{}; };
    std::array<Channel,4> channels{};
    for (std::size_t i = 0; i < channels.size(); ++i) {
        channels[i].pc = word(bytes,std::size_t(table)+std::size_t(entry)*8+i*2);
        channels[i].duration = channels[i].pc ? 1 : 0;
    }
    SpeakerClip clip{1193181,tick_divisor,{}};
    std::size_t commands = 0;
    for (std::size_t tick = 0; tick < 2400; ++tick) {
        for (auto& channel : channels) {
            if (!channel.duration) continue;
            channel.output = channel.divisor;
            if (--channel.duration) continue;
            while (true) {
                if (++commands > 4096) throw std::runtime_error("Speaker command limit exceeded");
                if (channel.pc >= bytes.size() || bytes.size()-channel.pc < 4)
                    throw std::runtime_error("Truncated speaker command");
                if (bytes[channel.pc++] != 255) throw std::runtime_error("Unsupported speaker command");
                const auto field = bytes[channel.pc++];
                const auto value = word(bytes,channel.pc);
                channel.pc += 2;
                if (field == 0) { channel.duration = value; break; }
                if (field == 4) channel.divisor = value;
                else if (field == 8) channel.output = value;
                else if (field == 10) channel.gate = value;
                else throw std::runtime_error("Unsupported speaker state field");
            }
        }
        SpeakerFrame frame{};
        bool active = false, selected = false;
        for (const auto& channel : channels) {
            active |= channel.duration != 0;
            if (!selected && channel.duration && channel.gate) {
                frame = {channel.output,(channel.gate & 3) == 3};
                selected = true;
            }
        }
        clip.frames.push_back(frame);
        if (!active) return clip;
    }
    throw std::runtime_error("Speaker duration limit exceeded");
}
std::vector<float> render_speaker(const SpeakerClip& clip, std::uint32_t rate) {
    if (rate < 8000 || rate > 192000 || clip.clock_hz < 100000 || clip.clock_hz > 10000000 ||
        !clip.tick_divisor || clip.frames.size() > 2400) throw std::runtime_error("Invalid speaker clip");
    const auto total = std::uint64_t(clip.frames.size())*clip.tick_divisor*rate/clip.clock_hz;
    if (total > std::uint64_t(rate)*10) throw std::runtime_error("Speaker sample limit exceeded");
    std::vector<float> pcm;
    pcm.reserve(total);
    std::uint64_t phase = 0;
    for (std::size_t tick = 0; tick < clip.frames.size(); ++tick) {
        const auto end = std::uint64_t(tick+1)*clip.tick_divisor*rate/clip.clock_hz;
        const auto frame = clip.frames[tick];
        const std::uint64_t period = std::uint64_t(frame.divisor ? frame.divisor : 65536)*rate;
        phase %= period;
        while (pcm.size() < end) {
            pcm.push_back(frame.enabled ? (phase < period/2 ? 0.12f : -0.12f) : 0.0f);
            phase = (phase+clip.clock_hz)%period;
        }
    }
    return pcm;
}
} // namespace goldbox::formats
