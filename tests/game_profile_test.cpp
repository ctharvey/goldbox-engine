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
    assert(pool.id != curse.id);
    assert(pool.archive_format == "dax-v1");
    assert(pool.archive_format == curse.archive_format);
    assert(pool.event_dialect == "unverified" && curse.event_dialect == "unverified");
    assert(pool.validation_status == "unverified");
    assert(pool.resource_patterns.size() == 4);
    assert(pool.image_dialect == "ssi-images-3-v1" && curse.image_dialect == "disabled");
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
    return 0;
}
