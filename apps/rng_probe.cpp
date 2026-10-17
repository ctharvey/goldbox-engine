// SPDX-License-Identifier: GPL-3.0-or-later
#include "goldbox/random.hpp"
#include <charconv>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {
std::uint32_t number(std::string_view text, std::uint32_t maximum) {
    std::uint32_t result{};
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || result > maximum)
        throw std::runtime_error("expected a bounded unsigned decimal integer");
    return result;
}
}

int main(int argc, char** argv) {
    try {
        if (argc != 6) throw std::runtime_error("usage: goldbox_rng_probe seed bound count multiplier increment");
        const auto seed = number(argv[1], UINT32_MAX);
        const auto bound = static_cast<std::uint16_t>(number(argv[2], UINT16_MAX));
        const auto count = number(argv[3], 64);
        if (!count) throw std::runtime_error("count must be 1..64");
        goldbox::HighWordLcg32 random(seed, number(argv[4], UINT32_MAX), number(argv[5], UINT32_MAX));
        std::cout << '[';
        for (std::uint32_t i = 0; i < count; ++i) {
            const auto before = random.state();
            const auto value = random.draw(bound);
            if (i) std::cout << ',';
            std::cout << "{\"index\":" << i + 1 << ",\"before\":" << before
                      << ",\"value\":" << value << ",\"after\":" << random.state() << '}';
        }
        std::cout << "]\n";
    } catch (const std::exception& error) {
        std::cerr << "rng probe: " << error.what() << '\n';
        return 1;
    }
}
