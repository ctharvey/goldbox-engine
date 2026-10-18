// SPDX-License-Identifier: GPL-3.0-or-later
#include "goldbox/formats/audio.hpp"
#include <array>
#include <cmath>
#include <optional>
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
    struct Channel { std::size_t pc{}; std::array<std::uint16_t,24> state{}; };
    std::array<Channel,4> channels{};
    for (std::size_t i = 0; i < channels.size(); ++i) {
        channels[i].pc = word(bytes,std::size_t(table)+std::size_t(entry)*8+i*2);
        channels[i].state[0] = channels[i].pc ? 1 : 0;
    }
    SpeakerClip clip{1193181,tick_divisor,{}};
    std::size_t commands = 0;
    for (std::size_t tick = 0; tick < 2400; ++tick) {
        for (auto& channel : channels) {
            auto& state = channel.state;
            if (!state[0]) continue;
            state[5] += state[6];
            state[2] += state[3];
            std::int32_t modulation = 0;
            auto phase = std::uint16_t(state[15]+state[16]);
            if (phase) {
                if (phase >= state[18]) phase -= state[18];
                state[15] = phase;
                const auto address = std::uint16_t((std::int16_t(phase) >> 4)+state[14]);
                if (address >= bytes.size()) throw std::runtime_error("Speaker modulation outside segment");
                const auto sample = std::int16_t(std::uint16_t(bytes[address]) << 8);
                modulation = (std::int32_t(sample)*std::int16_t(state[17])) >> 16;
            }
            state[4] = std::uint16_t(state[2]+modulation);
            if (--state[0]) continue;
            while (true) {
                if (++commands > 4096) throw std::runtime_error("Speaker command limit exceeded");
                if (channel.pc >= bytes.size() || bytes.size()-channel.pc < 4)
                    throw std::runtime_error("Truncated speaker command");
                const auto command = bytes[channel.pc++];
                const auto field = bytes[channel.pc++];
                const auto value = word(bytes,channel.pc);
                channel.pc += 2;
                if (field % 2 || field > 46) throw std::runtime_error("Invalid speaker state field");
                if (command == 254) {
                    if (field != 38 && field != 40) throw std::runtime_error("Unsupported speaker loop field");
                    auto& counter = state[field/2];
                    if (counter && !--counter) continue;
                    channel.pc = value;
                    continue;
                }
                if (command != 255) throw std::runtime_error("Unsupported speaker command");
                if (field != 0 && field != 4 && field != 6 && field != 8 && field != 10 && field != 12 &&
                    field != 28 && field != 30 && field != 32 && field != 34 && field != 36 &&
                    field != 38 && field != 40) throw std::runtime_error("Unsupported speaker state field");
                state[field/2] = value;
                if (!field) break;
            }
        }
        SpeakerFrame frame{};
        bool active = false, selected = false;
        for (const auto& channel : channels) {
            active |= channel.state[0] != 0;
            if (!selected && channel.state[0] && channel.state[5]) {
                frame = {channel.state[4],(channel.state[5] & 3) == 3};
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
TandyClip decode_ssi_tandy(std::span<const std::uint8_t> bytes,
    std::uint16_t table, std::uint16_t entry, std::uint16_t durations,
    std::uint16_t frequencies, std::uint16_t tick_divisor) {
    if (bytes.size() > 65536 || !tick_divisor) throw std::runtime_error("Invalid Tandy segment/timing");
    if (std::size_t(durations)+19 > bytes.size() || std::size_t(frequencies)+24 > bytes.size())
        throw std::runtime_error("Truncated Tandy note tables");
    struct Channel { std::size_t pc{}; std::array<std::uint16_t,24> state{}; };
    std::array<Channel,4> channels{};
    for (std::size_t i = 0; i < channels.size(); ++i) {
        channels[i].pc = word(bytes,std::size_t(table)+std::size_t(entry)*8+i*2);
        channels[i].state[0] = channels[i].pc ? 1 : 0;
    }
    std::optional<std::size_t> return_address;
    std::size_t operations = 0;
    const auto budget = [&] {
        if (++operations > 200000) throw std::runtime_error("Tandy operation limit exceeded");
    };
    const auto byte = [&](std::size_t p) {
        if (p >= bytes.size()) throw std::runtime_error("Truncated Tandy byte");
        return bytes[p];
    };
    TandyClip clip{tick_divisor,{}};
    for (std::size_t tick = 0; tick < 30000 &&
        std::uint64_t(tick+1)*tick_divisor <= 120u*1193181u; ++tick) {
        for (std::size_t index = 0; index < channels.size(); ++index) {
            auto& original = channels[index];
            auto& state = original.state;
            if (!state[0]) continue;
            state[5] += state[6];
            state[2] += state[3];
            std::int32_t modulation = 0;
            auto phase = std::uint16_t(state[15]+state[16]);
            if (phase) {
                if (phase >= state[18]) phase -= state[18];
                state[15] = phase;
                const auto address = std::uint16_t((std::int16_t(phase) >> 4)+state[14]);
                const auto sample = std::int16_t(std::uint16_t(byte(address)) << 8);
                modulation = (std::int32_t(sample)*std::int16_t(state[17])) >> 16;
            }
            state[4] = std::uint16_t(state[2]+modulation);
            if (state[10] && !--state[10]) { state[12] = 16; state[13] = 1; }
            if (!--state[0] && original.pc) {
                std::size_t pc = original.pc, selected = index;
                bool yielded = false;
                while (!yielded) {
                    budget();
                    const auto command = byte(pc++);
                    auto& current = channels[selected].state;
                    if (command == 250 || command == 253) {
                        if (command == 253) {
                            const auto offset = word(bytes,pc); pc += 2;
                            if (offset % 48 || offset > 144)
                                throw std::runtime_error("Invalid Tandy channel selector");
                            selected = offset/48;
                        }
                        auto& reset = channels[selected].state;
                        for (const auto field : {2,3,4,5,6,8,9,11,12,13,14,15,16,17,18}) reset[field] = 0;
                    } else if (command == 251) {
                        if (!return_address) throw std::runtime_error("Tandy return without call");
                        pc = *return_address; return_address.reset();
                    } else if (command == 252) {
                        if (return_address) throw std::runtime_error("Unsupported nested Tandy call");
                        const auto target = word(bytes,pc);
                        return_address = pc+2; pc = target;
                    } else if (command == 254 || command == 255) {
                        const auto field = byte(pc++);
                        const auto value = word(bytes,pc); pc += 2;
                        if (field % 2 || field > 46 || field == 2 || field == 42 || field == 44 || field == 46)
                            throw std::runtime_error("Unsupported Tandy state field");
                        if (command == 254) {
                            if (field != 38 && field != 40) throw std::runtime_error("Unsupported Tandy loop field");
                            auto& counter = current[field/2];
                            if (!counter || --counter) pc = value;
                        } else {
                            current[field/2] = value;
                            yielded = field == 0;
                        }
                    } else {
                        // A chord groups duration/pitch pairs until the pitch's high bit ends it.
                        auto duration = command;
                        while (true) {
                            budget();
                            const auto pitch = byte(pc++);
                            const auto voice = duration >> 5;
                            if (voice > 2) throw std::runtime_error("Unsupported Tandy noise note");
                            if ((duration & 31) >= 19) throw std::runtime_error("Invalid Tandy note duration");
                            const auto speed = current[7] & 255;
                            current[0] = std::uint16_t((speed ? speed : 1)*
                                byte(std::size_t(durations)+(duration & 31)));
                            if (!current[0]) throw std::runtime_error("Zero Tandy note duration");
                            if ((pitch & 127) != 127) {
                                auto& tone = channels[voice].state;
                                tone[0] = current[0];
                                tone[10] = std::uint16_t(current[0]-tone[8]);
                                tone[12] = 0; tone[13] = 1;
                                const auto note = std::uint16_t((pitch & 127)+tone[9]);
                                const auto octave = note/12;
                                const auto divisor = word(bytes,std::size_t(frequencies)+(note%12)*2);
                                tone[2] = tone[4] = octave < 16 ? divisor >> octave : 0;
                            }
                            if (pitch & 128) break;
                            duration = byte(pc++);
                        }
                        yielded = true;
                    }
                }
                original.pc = state[0] ? pc : 0;
            }
            if (state[13] && !--state[13]) {
                while (true) {
                    budget();
                    const auto address = std::uint16_t(state[11]+state[12]);
                    const auto value = word(bytes,address), duration = word(bytes,std::size_t(address)+2);
                    state[12] += 4;
                    if (duration == 65535) {
                        state[5] = value;
                        if (!value) state[6] = 0;
                    } else {
                        // Zero disables further envelope steps in the original driver.
                        state[6] = value; state[13] = duration;
                        break;
                    }
                }
            }
        }
        std::array<TandyTone,3> frame{};
        bool active = false;
        for (std::size_t i = 0; i < channels.size(); ++i) {
            const auto& state = channels[i].state;
            active |= state[0] != 0;
            const auto attenuation = std::uint8_t((65535-state[5]) >> 12);
            if (i == 3) {
                if (attenuation != 15) throw std::runtime_error("Unsupported Tandy noise output");
            } else frame[i] = {std::uint16_t(state[4] >> 6),attenuation};
        }
        clip.frames.push_back(frame);
        if (!active) return clip;
    }
    throw std::runtime_error("Tandy duration limit exceeded");
}
std::vector<float> render_tandy(const TandyClip& clip, std::uint32_t rate) {
    const auto total = std::uint64_t(clip.frames.size())*clip.tick_divisor*rate/1193181;
    if (rate < 8000 || rate > 192000 || !clip.tick_divisor || clip.frames.size() > 30000 ||
        total > std::uint64_t(rate)*120) throw std::runtime_error("Invalid Tandy clip");
    std::array<float,16> levels{};
    for (std::size_t i = 0; i < 15; ++i) levels[i] = 0.04f*std::pow(10.0f,-float(i)/10.0f);
    std::array<std::uint64_t,3> phase{};
    std::vector<float> pcm;
    pcm.reserve(total);
    for (std::size_t tick = 0; tick < clip.frames.size(); ++tick) {
        const auto end = std::uint64_t(tick+1)*clip.tick_divisor*rate/1193181;
        std::array<std::uint64_t,3> periods{};
        for (std::size_t i = 0; i < 3; ++i) {
            const auto tone = clip.frames[tick][i];
            if (tone.period > 1023 || tone.attenuation > 15) throw std::runtime_error("Invalid Tandy tone");
            periods[i] = std::uint64_t(tone.period ? tone.period : 1024)*32*rate;
            phase[i] %= periods[i];
        }
        while (pcm.size() < end) {
            float sample = 0;
            for (std::size_t i = 0; i < 3; ++i) {
                const auto level = levels[clip.frames[tick][i].attenuation];
                sample += phase[i] < periods[i]/2 ? level : -level;
                phase[i] = (phase[i]+3579545)%periods[i];
            }
            pcm.push_back(sample);
        }
    }
    return pcm;
}
} // namespace goldbox::formats
