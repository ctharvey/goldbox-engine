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
        "colorBackdrop", "portrait", "areaDialect", "picturePeriodMs", "startupTitleRecords",
        "characterCreationRules", "partyMemory"}, "game profile");
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
        {}, std::nullopt, "disabled", std::nullopt, std::nullopt, "disabled", 0, {}
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
    if (const auto* value = root.if_contains("startupTitleRecords")) {
        const auto& records = j::array(*value,"startupTitleRecords");
        if (records.empty() || records.size() > 64) throw std::runtime_error("Startup requires 1..64 title records");
        for (const auto& record : records) {
            const auto id = j::integer(record,"startup title record");
            if (id < 0 || id > 255) throw std::runtime_error("Invalid startup title record");
            profile.startup_title_records.push_back(static_cast<std::uint8_t>(id));
        }
    }
    if (const auto* value = root.if_contains("characterCreationRules")) {
        const auto path = std::filesystem::path(j::string(*value,"characterCreationRules"));
        if (path.empty() || path.is_absolute()) throw std::runtime_error("Creation rules path must be relative");
        profile.character_rules = read_creation_rules(file.parent_path()/path);
        if (profile.character_rules->id != profile.rules_profile) throw std::runtime_error("Creation rules ID mismatch");
    }
    if (const auto* value = root.if_contains("picturePeriodMs")) {
        const auto period = j::integer(*value,"picturePeriodMs");
        if (period < 0 || period > 60000) throw std::runtime_error("Invalid picture animation period");
        profile.picture_period_ms = static_cast<std::uint32_t>(period);
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
    if (const auto* value = root.if_contains("portrait")) {
        const auto& object = j::object(*value,"portrait");
        j::fields_only(object,{"headIdAddress","width","height","headOffset","bodyOffset"},"portrait");
        const auto bounded = [&](const char* name, std::int64_t minimum, std::int64_t maximum) {
            const auto n = j::integer(j::field(object,name,"portrait"),name);
            if (n < minimum || n > maximum) throw std::runtime_error("Invalid portrait configuration");
            return static_cast<std::uint16_t>(n);
        };
        GameProfile::Portrait config{};
        config.head_id_address = bounded("headIdAddress",0,65535);
        config.layout.width = bounded("width",1,320);
        config.layout.height = bounded("height",1,200);
        const auto offset = [&](const char* name) {
            const auto& a = j::array(j::field(object,name,"portrait"),name);
            if (a.size() != 2) throw std::runtime_error("Portrait offset requires X/Y");
            std::array<std::uint16_t,2> result{};
            for (std::size_t i = 0; i < 2; ++i) {
                const auto n = j::integer(a[i],name);
                const auto limit = i ? config.layout.height : config.layout.width;
                if (n < 0 || n >= limit) throw std::runtime_error("Invalid portrait offset");
                result[i] = static_cast<std::uint16_t>(n);
            }
            return result;
        };
        config.layout.head_offset = offset("headOffset");
        config.layout.body_offset = offset("bodyOffset");
        profile.portrait = config;
    }
    if (const auto* value = root.if_contains("areaDialect")) {
        profile.area_dialect = j::string(*value,"areaDialect");
        if (profile.area_dialect != "disabled" && profile.area_dialect != "ssi-area-map3-v1")
            throw std::runtime_error("Unsupported area dialect");
    }
    if (const auto* value = root.if_contains("partyMemory")) {
        if (!profile.character_rules) throw std::runtime_error("Party memory requires character creation rules");
        const auto& o = j::object(*value,"party memory");
        j::fields_only(o,{"dialect","recordBase","recordSize","countAddress","abilities","selectedAddress","status"},"party memory");
        const auto address = [&](const char* key) {
            const auto n = j::integer(j::field(o,key,"party memory"),key);
            if (n < 0 || n > 65535) throw std::runtime_error("Invalid party memory address");
            return static_cast<std::uint16_t>(n);
        };
        PartyMemoryLayout layout{j::string(j::field(o,"dialect","party memory"),"party dialect"),
            address("recordBase"),address("recordSize"),address("countAddress"),{}};
        if (o.if_contains("selectedAddress")) layout.selected_address = address("selectedAddress");
        if (const auto* value = o.if_contains("status")) {
            const auto& status = j::object(*value,"party status binding");
            j::fields_only(status,{"offset","values"},"party status binding");
            const auto offset = j::integer(j::field(status,"offset","party status binding"),"status offset");
            if (offset < 0 || offset > 65535) throw std::runtime_error("Invalid party status offset");
            PartyStatusBinding binding{static_cast<std::uint16_t>(offset),{}};
            const auto& values = j::object(j::field(status,"values","party status binding"),"status values");
            if (values.size() > 64) throw std::runtime_error("Too many status encodings");
            for (const auto& entry : values) {
                const auto code = j::integer(entry.value(),"status code");
                if (code < 0 || code > 255) throw std::runtime_error("Invalid status code");
                binding.values.emplace(std::string(entry.key()),static_cast<std::uint8_t>(code));
            }
            layout.status = std::move(binding);
        }
        const auto& fields = j::array(j::field(o,"abilities","party memory"),"party ability bindings");
        if (fields.size() > 64) throw std::runtime_error("Too many party ability bindings");
        for (const auto& field : fields) {
            const auto& item = j::object(field,"party ability binding");
            j::fields_only(item,{"offset","ability","percentile"},"party ability binding");
            const auto offset = j::integer(j::field(item,"offset","party ability binding"),"party offset");
            const auto& percentile = j::field(item,"percentile","party ability binding");
            if (offset < 0 || offset > 65535 || !percentile.is_bool())
                throw std::runtime_error("Invalid party ability binding");
            layout.abilities.push_back({static_cast<std::uint16_t>(offset),
                j::string(j::field(item,"ability","party ability binding"),"party ability"),percentile.as_bool()});
        }
        validate_party_memory_layout(layout,*profile.character_rules);
        profile.party_memory = std::move(layout);
    }
    return profile;
}

} // namespace goldbox
