#include "goldbox/game_profile.hpp"
#include "json_support.hpp"

#include <regex>
#include <stdexcept>

namespace goldbox {

GameProfile read_game_profile(const std::filesystem::path& file) {
    if (std::filesystem::is_symlink(file)) {
        throw std::runtime_error("game profile must not be a symlink");
    }
    namespace j = json_support;
    const auto parsed = j::read_json(file);
    const auto& root = j::object(parsed, "game profile");
    j::fields_only(root, {"schemaVersion", "id", "title", "formatFamily", "archiveFormat",
        "eventDialect", "rulesProfile", "validationStatus", "resourcePatterns"}, "game profile");
    if (j::integer(j::field(root, "schemaVersion", "game profile"), "schemaVersion") != 1) {
        throw std::runtime_error("unsupported game profile schemaVersion");
    }
    GameProfile profile{
        j::string(j::field(root, "id", "game profile"), "profile.id"),
        j::string(j::field(root, "title", "game profile"), "profile.title"),
        j::string(j::field(root, "formatFamily", "game profile"), "profile.formatFamily"),
        j::string(j::field(root, "archiveFormat", "game profile"), "profile.archiveFormat"),
        j::string(j::field(root, "eventDialect", "game profile"), "profile.eventDialect"),
        j::string(j::field(root, "rulesProfile", "game profile"), "profile.rulesProfile"),
        j::string(j::field(root, "validationStatus", "game profile"), "profile.validationStatus"),
        {}
    };
    static const std::regex id_pattern(R"(^[a-z0-9][a-z0-9._-]{0,127}$)");
    if (!std::regex_match(profile.id, id_pattern) || profile.id.find("..") != std::string::npos ||
        profile.title.empty() || profile.title.size() > 256 || profile.format_family.empty() ||
        (profile.archive_format != "dax-v1" && profile.archive_format != "glb-v1") ||
        profile.event_dialect.empty() || profile.rules_profile.empty() ||
        (profile.validation_status != "unverified" && profile.validation_status != "verified")) {
        throw std::runtime_error("invalid or unsupported game profile metadata");
    }
    const auto& patterns = j::array(j::field(root, "resourcePatterns", "game profile"), "profile.resourcePatterns");
    if (patterns.size() > 64) throw std::runtime_error("too many resource patterns");
    for (const auto& value : patterns) {
        const auto pattern = j::string(value, "profile.resourcePatterns element");
        if (pattern.empty() || pattern.size() > 128 || pattern.find('/') != std::string::npos ||
            pattern.find('\\') != std::string::npos || pattern.find("..") != std::string::npos) {
            throw std::runtime_error("unsafe or invalid resource pattern");
        }
        profile.resource_patterns.push_back(pattern);
    }
    return profile;
}

} // namespace goldbox
