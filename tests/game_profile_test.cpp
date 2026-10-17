#include "goldbox/game_profile.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <functional>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

int main(int argc, char** argv) {
    assert(argc == 3);
    const auto pool = goldbox::read_game_profile(argv[1]);
    const auto curse = goldbox::read_game_profile(argv[2]);
    if (pool.picture_period_ms != 400 || curse.picture_period_ms != 0) return 1;
    if (pool.startup_title_records != std::vector<std::uint8_t>{1,2} || !curse.startup_title_records.empty()) return 1;
    assert(pool.id != curse.id);
    assert(pool.archive_format == "dax-v1");
    assert(pool.archive_format == curse.archive_format);
    assert(pool.event_dialect == "unverified" && curse.event_dialect == "unverified");
    assert(pool.validation_status == "unverified");
    assert(pool.resource_patterns.size() == 4);
    assert(pool.image_dialect == "ssi-images-3-v1" && curse.image_dialect == "disabled");
    if (pool.area_dialect != "ssi-area-map3-v1" || curse.area_dialect != "disabled") return 1;
    if (!pool.color_backdrop || curse.color_backdrop ||
        pool.color_backdrop->sky_color_addresses != std::array<std::uint16_t,2>{0x49fd,0x49fe}) return 1;
    if (!pool.portrait || curse.portrait || pool.portrait->head_id_address != 0x6de1 ||
        pool.portrait->layout.body_offset[1] != 40) return 1;
    const auto path = fs::temp_directory_path() / "goldbox_invalid_profile_fixture.json";
    {
        std::ofstream o(path, std::ios::trunc);
        o << R"({"schemaVersion":1,"id":"unsafe/../profile","title":"Bad","formatFamily":"goldbox-dos","archiveFormat":"dax-v1","eventDialect":"unknown","rulesProfile":"unknown","validationStatus":"unverified","resourcePatterns":[]})";
    }
    bool rejected = false;
    try { (void)goldbox::read_game_profile(path); }
    catch (const std::runtime_error&) { rejected = true; }
    fs::remove(path);
    assert(rejected);
    {
        std::ofstream o(path, std::ios::trunc);
        o << R"({"schemaVersion":1,"id":"test","title":"Test","formatFamily":"goldbox-dos","archiveFormat":"dax-v1","eventDialect":"unverified","rulesProfile":"test","validationStatus":"unverified","resourcePatterns":[],"imageDialect":"ssi-images-4-v1"})";
    }
    rejected = false;
    try { (void)goldbox::read_game_profile(path); }
    catch (const std::runtime_error&) { rejected = true; }
    fs::remove(path);
    if (!rejected) return 1;
    const std::string prefix = R"({"schemaVersion":1,"id":"test","title":"Test","formatFamily":"goldbox-dos","archiveFormat":"dax-v1","eventDialect":"unverified","rulesProfile":"test","validationStatus":"unverified","resourcePatterns":[],"colorBackdrop":)";
    for (const std::string field : {R"({"skyColorAddresses":[1]})", R"({"skyColorAddresses":[-1,2]})",
        R"({"skyColorAddresses":[1,65536]})", R"({"skyColorAddresses":[1,"2"]})",
        R"({"skyColorAddresses":[1,2],"unknown":0})"}) {
        { std::ofstream o(path); o << prefix << field << '}'; }
        rejected = false;
        try { (void)goldbox::read_game_profile(path); }
        catch (const std::runtime_error&) { rejected = true; }
        fs::remove(path);
        if (!rejected) return 1;
    }
    auto portrait_prefix = prefix;
    auto startup_prefix = prefix;
    startup_prefix.replace(startup_prefix.find("colorBackdrop"),13,"startupTitleRecords");
    for (const std::string value : {"[]","[-1]","[256]","[\"1\"]","true"}) {
        { std::ofstream o(path); o << startup_prefix << value << '}'; }
        rejected = false;
        try { (void)goldbox::read_game_profile(path); }
        catch (const std::runtime_error&) { rejected = true; }
        fs::remove(path);
        if (!rejected) return 1;
    }
    auto timing_prefix = prefix;
    timing_prefix.replace(timing_prefix.find("colorBackdrop"),13,"picturePeriodMs");
    for (const std::string value : {"-1","60001","1.5","true","\"400\""}) {
        { std::ofstream o(path); o << timing_prefix << value << '}'; }
        rejected = false;
        try { (void)goldbox::read_game_profile(path); }
        catch (const std::runtime_error&) { rejected = true; }
        fs::remove(path);
        if (!rejected) return 1;
    }
    portrait_prefix.replace(portrait_prefix.find("colorBackdrop"),13,"portrait");
    for (const std::string field : {
        R"({"headIdAddress":65536,"width":88,"height":88,"headOffset":[0,0],"bodyOffset":[0,40]})",
        R"({"headIdAddress":1,"width":0,"height":88,"headOffset":[0,0],"bodyOffset":[0,40]})",
        R"({"headIdAddress":1,"width":88,"height":201,"headOffset":[0,0],"bodyOffset":[0,40]})",
        R"({"headIdAddress":1,"width":88,"height":88,"headOffset":[-1,0],"bodyOffset":[0,40]})",
        R"({"headIdAddress":1,"width":88,"height":88,"headOffset":[0],"bodyOffset":[0,40]})",
        R"({"headIdAddress":1,"width":88,"height":40,"headOffset":[0,0],"bodyOffset":[0,40]})"}) {
        { std::ofstream o(path); o << portrait_prefix << field << '}'; }
        rejected = false;
        try { (void)goldbox::read_game_profile(path); }
        catch (const std::runtime_error&) { rejected = true; }
        fs::remove(path);
        if (!rejected) return 1;
    }
    { std::ofstream o(path); o << portrait_prefix << R"({"headIdAddress":4660,"width":16,"height":16,"headOffset":[0,0],"bodyOffset":[0,8]}})"; }
    const auto custom = goldbox::read_game_profile(path);
    fs::remove(path);
    if (!custom.portrait || custom.portrait->head_id_address != 0x1234 || custom.portrait->layout.width != 16) return 1;
    auto area_prefix = prefix;
    area_prefix.replace(area_prefix.find("colorBackdrop"),13,"areaDialect");
    { std::ofstream o(path); o << area_prefix << R"("unknown-area"})"; }
    rejected = false;
    try { (void)goldbox::read_game_profile(path); }
    catch (const std::runtime_error&) { rejected = true; }
    fs::remove(path);
    if (!rejected) return 1;
    return 0;
}
