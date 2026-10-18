#include "goldbox/game_profile.hpp"
#include "json_support.hpp"

#include <regex>
#include <algorithm>
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
        "characterCreationRules", "partyMemory", "random", "characterCreationDice", "speakerAudio", "soundCall",
        "startupTitlePeriodMs", "tandyMusic", "campaignStartup", "scriptDelayMs", "spriteReplacementMs",
        "originalDisplay"}, "game profile");
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
    if (const auto* value = root.if_contains("originalDisplay")) {
        const auto& config = j::object(*value,"originalDisplay");
        j::fields_only(config,{"fontArchive","fontRecord","symbolsArchive","symbolsRecord","canvas","scene",
            "party","dialogue","commands","position","frameRuns","partyRowHeight"},"originalDisplay");
        GameProfile::OriginalDisplay display;
        const auto numbers = [&](const auto& value, auto& destination) {
            const auto& values = j::array(value,"display coordinates");
            if (values.size() != destination.size()) throw std::runtime_error("Invalid display coordinates");
            for (std::size_t i = 0; i < values.size(); ++i) {
                const auto n = j::integer(values[i],"display coordinate");
                if (n < 0 || n > 640) throw std::runtime_error("Display coordinate out of range");
                destination[i] = static_cast<std::uint16_t>(n);
            }
        };
        for (const auto& binding : {std::pair{"fontArchive",&display.font_archive},
            std::pair{"symbolsArchive",&display.symbols_archive}}) {
            *binding.second = j::string(j::field(config,binding.first,"originalDisplay"),binding.first);
            if (!std::regex_match(*binding.second,std::regex(R"([A-Za-z0-9_-]+\.[A-Za-z0-9]+)")))
                throw std::runtime_error("Unsafe display archive name");
        }
        for (const auto& binding : {std::pair{"fontRecord",&display.font_record},
            std::pair{"symbolsRecord",&display.symbols_record}}) {
            const auto n = j::integer(j::field(config,binding.first,"originalDisplay"),binding.first);
            if (n < 0 || n > 255) throw std::runtime_error("Invalid display record");
            *binding.second = static_cast<std::uint8_t>(n);
        }
        numbers(j::field(config,"canvas","originalDisplay"),display.canvas);
        numbers(j::field(config,"position","originalDisplay"),display.position);
        if (const auto* row = config.if_contains("partyRowHeight")) {
            const auto n = j::integer(*row,"partyRowHeight");
            if (n < 8 || n > 32 || n%8) throw std::runtime_error("Invalid party row height");
            display.party_row_height = static_cast<std::uint16_t>(n);
        }
        if (display.canvas[0] < 160 || display.canvas[1] < 100 || display.canvas[1] > 480)
            throw std::runtime_error("Invalid display canvas");
        for (const auto& binding : {std::pair{"scene",&display.scene},std::pair{"party",&display.party},
            std::pair{"dialogue",&display.dialogue},std::pair{"commands",&display.commands}}) {
            numbers(j::field(config,binding.first,"originalDisplay"),*binding.second);
            const auto& r = *binding.second;
            if (!r[2] || !r[3] || r[0]+r[2] > display.canvas[0] || r[1]+r[3] > display.canvas[1] ||
                r[2]%8 || r[3]%8) throw std::runtime_error("Invalid display region");
        }
        if (display.position[0]+8 > display.canvas[0] || display.position[1]+8 > display.canvas[1])
            throw std::runtime_error("Invalid display position label");
        if (display.party[2] < 64 || display.party[3] < 16)
            throw std::runtime_error("Original party panel is too small");
        const auto& runs = j::array(j::field(config,"frameRuns","originalDisplay"),"frameRuns");
        if (runs.empty() || runs.size() > 128) throw std::runtime_error("Invalid frame run count");
        for (const auto& run : runs) {
            std::array<std::uint16_t,5> r{}; numbers(run,r);
            if (r[2] > 255 || !r[3] || r[3] > 80 || r[4] > 1 ||
                r[0]+(r[4] ? 8 : r[3]*8) > display.canvas[0] ||
                r[1]+(r[4] ? r[3]*8 : 8) > display.canvas[1])
                throw std::runtime_error("Invalid display frame run");
            display.frame_runs.push_back(r);
        }
        profile.original_display = std::move(display);
    }
    for (const auto& binding : {std::pair{"scriptDelayMs",&profile.script_delay_ms},
        std::pair{"spriteReplacementMs",&profile.sprite_replacement_ms}}) {
        if (const auto* value = root.if_contains(binding.first)) {
            const auto n = j::integer(*value,binding.first);
            if (n < 0 || n > 60000) throw std::runtime_error("Invalid presentation interval");
            *binding.second = static_cast<std::uint32_t>(n);
        }
    }
    if (const auto* value = root.if_contains("campaignStartup")) {
        const auto& config = j::object(*value,"campaignStartup");
        j::fields_only(config,{"mapRecord","programRecord","mapArchives","programArchives","positionRefreshCall","graphicsArchives"},
            "campaignStartup");
        const auto byte = [&](const char* name, std::int64_t maximum) {
            const auto n = j::integer(j::field(config,name,"campaignStartup"),name);
            if (n < 0 || n > maximum) throw std::runtime_error("Invalid campaign startup binding");
            return n;
        };
        GameProfile::CampaignStartup startup;
        startup.map_record = static_cast<std::uint8_t>(byte("mapRecord",255));
        startup.program_record = static_cast<std::uint8_t>(byte("programRecord",255));
        startup.position_refresh_call = static_cast<std::uint16_t>(byte("positionRefreshCall",65535));
        const auto archives = [&](const char* name, auto& destination) {
            const auto& values = j::array(j::field(config,name,"campaignStartup"),name);
            if (values.empty() || values.size() > 64) throw std::runtime_error("Invalid campaign archive count");
            for (const auto& entry : values) {
                const auto filename = j::string(entry,name);
                if (!std::regex_match(filename,std::regex(R"([A-Za-z0-9_-]+\.[A-Za-z0-9]+)")) ||
                    std::find(destination.begin(),destination.end(),filename) != destination.end())
                    throw std::runtime_error("Invalid or duplicate campaign archive name");
                destination.push_back(filename);
            }
        };
        archives("mapArchives",startup.map_archives);
        archives("programArchives",startup.program_archives);
        if (config.if_contains("graphicsArchives")) {
            archives("graphicsArchives",startup.graphics_archives);
            if (startup.graphics_archives.size() != 7) throw std::runtime_error("Startup graphics requires seven archives");
        }
        profile.campaign_startup = std::move(startup);
    }
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
    if (const auto* value = root.if_contains("startupTitlePeriodMs")) {
        const auto period = j::integer(*value,"startupTitlePeriodMs");
        if (period < 0 || period > 60000 || (period && profile.startup_title_records.empty()))
            throw std::runtime_error("Invalid startup title period or missing title records");
        profile.startup_title_period_ms = static_cast<std::uint32_t>(period);
    }
    if (const auto* value = root.if_contains("characterCreationRules")) {
        const auto path = std::filesystem::path(j::string(*value,"characterCreationRules"));
        if (path.empty() || path.is_absolute()) throw std::runtime_error("Creation rules path must be relative");
        profile.character_rules = read_creation_rules(file.parent_path()/path);
        if (profile.character_rules->id != profile.rules_profile) throw std::runtime_error("Creation rules ID mismatch");
    }
    if (const auto* value = root.if_contains("random")) {
        const auto& object = j::object(*value,"random profile");
        j::fields_only(object,{"algorithm","multiplier","increment","seed","eclDialect"},"random profile");
        if (j::string(j::field(object,"algorithm","random"),"random algorithm") != "lcg32-high16-mod-v1")
            throw std::runtime_error("Unsupported random algorithm");
        const auto word = [&](const char* key) {
            const auto n = j::integer(j::field(object,key,"random"),key);
            if (n < 0 || n > UINT32_MAX) throw std::runtime_error("Random parameter outside uint32");
            return static_cast<std::uint32_t>(n);
        };
        profile.random.emplace(word("seed"),word("multiplier"),word("increment"));
        if (const auto* dialect = object.if_contains("eclDialect")) {
            const auto name = j::string(*dialect,"random eclDialect");
            if (name == "ssi-byte-upper-v1") profile.random_dialect = EclRandomDialect::byte_upper;
            else if (name != "disabled") throw std::runtime_error("Unsupported ECL random dialect");
        }
    }
    if (const auto* value = root.if_contains("characterCreationDice")) {
        if (!profile.character_rules) throw std::runtime_error("Creation dice require character rules");
        const auto& object = j::object(*value,"character creation dice");
        j::fields_only(object,{"count","sides","dropLowest","keepBestOf"},"character creation dice");
        const auto bounded = [&](const char* key, int minimum, int maximum) {
            const auto n = j::integer(j::field(object,key,"creation dice"),key);
            if (n < minimum || n > maximum) throw std::runtime_error("Invalid character creation dice");
            return static_cast<int>(n);
        };
        auto& rules = *profile.character_rules;
        rules.dice_count = bounded("count",1,10);
        rules.dice_sides = bounded("sides",2,100);
        rules.drop_lowest = bounded("dropLowest",0,rules.dice_count-1);
        rules.keep_best_of = bounded("keepBestOf",1,16);
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
    if (const auto* value = root.if_contains("speakerAudio")) {
        const auto& audio = j::object(*value,"speakerAudio");
        j::fields_only(audio,{"segmentOffset","effectTable","previewEntry","tickDivisor","effectCount"},"speakerAudio");
        const auto number = [&](const char* name, std::int64_t maximum) {
            const auto n = j::integer(j::field(audio,name,"speakerAudio"),name);
            if (n < 0 || n > maximum) throw std::runtime_error("Invalid speaker audio layout");
            return n;
        };
        const auto segment = number("segmentOffset",1024*1024-65536);
        const auto table = number("effectTable",65535);
        const auto entry = number("previewEntry",8191);
        const auto tick = number("tickDivisor",65535);
        const auto count = audio.if_contains("effectCount") ? number("effectCount",8192) : entry+1;
        if (!tick || !count || entry >= count || table+count*8 > 65536)
            throw std::runtime_error("Invalid speaker audio timing/table");
        profile.speaker_audio = GameProfile::SpeakerAudio{static_cast<std::uint32_t>(segment),
            static_cast<std::uint16_t>(table),static_cast<std::uint16_t>(entry),static_cast<std::uint16_t>(tick),
            static_cast<std::uint16_t>(count)};
    }
    if (const auto* value = root.if_contains("tandyMusic")) {
        const auto& music = j::object(*value,"tandyMusic");
        j::fields_only(music,{"fingerprint","fileSegmentOffset","effectTable","effectCount",
            "table","entry","durations","frequencies","tickDivisor"},"tandyMusic");
        const auto number = [&](const char* name, std::int64_t maximum) {
            const auto n = j::integer(j::field(music,name,"tandyMusic"),name);
            if (n < 0 || n > maximum) throw std::runtime_error("Invalid Tandy music layout");
            return n;
        };
        const auto fingerprint = j::string(j::field(music,"fingerprint","tandyMusic"),"fingerprint");
        const auto offset = number("fileSegmentOffset",2*1024*1024-1);
        const auto effect = number("effectTable",65535), count = number("effectCount",8192);
        const auto table = number("table",65535), entry = number("entry",8191);
        const auto durations = number("durations",65535), frequencies = number("frequencies",65535);
        const auto tick = number("tickDivisor",65535);
        if (!std::regex_match(fingerprint,std::regex("[0-9]{1,20}")) ||
            (fingerprint.size() == 20 && fingerprint > "18446744073709551615") || !tick || !count ||
            effect+count*8 > 65536 || table+(entry+1)*8 > 65536 || durations+19 > 65536 ||
            frequencies+24 > 65536 || !profile.speaker_audio || profile.speaker_audio->preview_entry >= count)
            throw std::runtime_error("Invalid Tandy music tables/fingerprint");
        profile.tandy_music = GameProfile::TandyMusic{fingerprint,static_cast<std::uint32_t>(offset),
            static_cast<std::uint16_t>(effect),static_cast<std::uint16_t>(count),
            static_cast<std::uint16_t>(table),static_cast<std::uint16_t>(entry),
            static_cast<std::uint16_t>(durations),static_cast<std::uint16_t>(frequencies),static_cast<std::uint16_t>(tick)};
    }
    if (const auto* value = root.if_contains("soundCall")) {
        const auto& call = j::object(*value,"soundCall");
        j::fields_only(call,{"function","selectorAddress","cases"},"soundCall");
        const auto number = [&](const auto& object, const char* name, std::int64_t maximum) {
            const auto n = j::integer(j::field(object,name,"soundCall"),name);
            if (n < 0 || n > maximum) throw std::runtime_error("Invalid sound-call configuration");
            return static_cast<std::uint16_t>(n);
        };
        EclSoundCall config{number(call,"function",65535),number(call,"selectorAddress",65534),{}};
        const auto& cases = j::array(j::field(call,"cases","soundCall"),"soundCall.cases");
        if (cases.empty() || cases.size() > 16) throw std::runtime_error("Invalid sound-call case count");
        for (const auto& item : cases) {
            const auto& route = j::object(item,"soundCall.case");
            j::fields_only(route,{"selector","sound"},"soundCall.case");
            EclSoundCall::Case entry{number(route,"selector",65535),number(route,"sound",255)};
            for (const auto& previous : config.cases)
                if (previous.selector == entry.selector)
                    throw std::runtime_error("Conflicting sound-call cases");
            config.cases.push_back(entry);
        }
        profile.sound_call = std::move(config);
    }
    return profile;
}

} // namespace goldbox
