#pragma once

#include <filesystem>
#include <cstdint>
#include <array>
#include <optional>
#include <string>
#include <vector>

namespace goldbox {

// Describes how to select *separate* format and rules modules for a title.
// This is configuration, not a promise of playable game compatibility.
struct GameProfile {
    std::string id;
    std::string title;
    std::string format_family;
    std::string archive_format;
    std::string event_dialect;
    std::string rules_profile;
    std::string validation_status;
    std::vector<std::string> resource_patterns;
    struct EventDispatch {
        std::string dialect;
        std::uint16_t code_base{};
        std::uint16_t square_info_address{};
        std::optional<std::array<std::uint16_t, 3>> position_addresses;
    };
    std::optional<EventDispatch> event_dispatch;
    std::string image_dialect{"disabled"};
};

[[nodiscard]] GameProfile read_game_profile(const std::filesystem::path& path);

} // namespace goldbox
