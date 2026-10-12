#pragma once

#include <filesystem>
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
};

[[nodiscard]] GameProfile read_game_profile(const std::filesystem::path& path);

} // namespace goldbox
