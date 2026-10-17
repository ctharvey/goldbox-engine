// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>

namespace goldbox {
// Algorithm parameters belong to the caller's compatibility adapter. This
// primitive does not select a game, interpret ECL bounds or seed from the clock.
class HighWordLcg32 {
public:
    HighWordLcg32(std::uint32_t seed, std::uint32_t multiplier, std::uint32_t increment)
        : state_(seed), multiplier_(multiplier), increment_(increment) {}
    std::uint32_t state() const { return state_; }
    std::uint16_t draw(std::uint16_t bound) {
        state_ = static_cast<std::uint32_t>(
            static_cast<std::uint64_t>(state_) * multiplier_ + increment_);
        return bound ? static_cast<std::uint16_t>((state_ >> 16) % bound) : 0;
    }
private:
    std::uint32_t state_, multiplier_, increment_;
};
} // namespace goldbox
