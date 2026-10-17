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
        "eventDialect", "rulesProfile", "validationStatus", "resourcePatterns", "eventDispatch", "imageDialect",
        "colorBackdrop"}, "game profile");
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
        {}, std::nullopt, "disabled", std::nullopt
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
    if (const auto* value = root.if_contains("eventDispatch")) {
        const auto& dispatch = j::object(*value, "eventDispatch");
        j::fields_only(dispatch, {"dialect", "codeBase", "squareInfoAddress", "positionAddresses"}, "eventDispatch");
        const auto dialect = j::string(j::field(dispatch, "dialect", "eventDispatch"), "dispatch.dialect");
        const auto base = j::integer(j::field(dispatch, "codeBase", "eventDispatch"), "dispatch.codeBase");
        const auto square = j::integer(j::field(dispatch, "squareInfoAddress", "eventDispatch"), "dispatch.squareInfoAddress");
        if (dialect != "ssi-five-entry-v1" || base < 0 || base > 65535 || square < 0 || square > 65535) {
            throw std::runtime_error("unsupported or invalid ECL dispatch configuration");
        }
        profile.event_dispatch = GameProfile::EventDispatch{dialect,
            static_cast<std::uint16_t>(base), static_cast<std::uint16_t>(square), std::nullopt};
        if (const auto* positions = dispatch.if_contains("positionAddresses")) {
            const auto& values = j::array(*positions, "positionAddresses");
            if (values.size() != 3) throw std::runtime_error("positionAddresses requires X/Y/direction");
            std::array<std::uint16_t, 3> addresses{};
            for (std::size_t i = 0; i < 3; ++i) {
                const auto address = j::integer(values[i], "position address");
                if (address < 0 || address > 65535) throw std::runtime_error("Invalid position address");
                addresses[i] = static_cast<std::uint16_t>(address);
            }
            profile.event_dispatch->position_addresses = addresses;
        }
    }
    if (const auto* images = root.if_contains("imageDialect")) {
        profile.image_dialect = j::string(*images,"imageDialect");
        if (profile.image_dialect != "disabled" && profile.image_dialect != "ssi-images-3-v1")
            throw std::runtime_error("Unsupported image dialect");
    }
    if (const auto* value = root.if_contains("colorBackdrop")) {
        const auto& backdrop = j::object(*value,"colorBackdrop");
        j::fields_only(backdrop,{"skyColorAddresses"},"colorBackdrop");
        const auto& values = j::array(j::field(backdrop,"skyColorAddresses","colorBackdrop"),"skyColorAddresses");
        if (values.size() != 2) throw std::runtime_error("skyColorAddresses requires outdoors/indoors");
        GameProfile::ColorBackdrop config{};
        for (std::size_t i = 0; i < 2; ++i) {
            const auto address = j::integer(values[i],"sky color address");
            if (address < 0 || address > 65535) throw std::runtime_error("Invalid sky color address");
            config.sky_color_addresses[i] = static_cast<std::uint16_t>(address);
        }
        profile.color_backdrop = config;
    }
    return profile;
}

} // namespace goldbox
