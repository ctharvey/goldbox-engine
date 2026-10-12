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
    return 0;
}
