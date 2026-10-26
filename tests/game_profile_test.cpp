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
    assert(pool.arithmetic_dialect == goldbox::EclArithmeticDialect::unsigned16);
    assert(curse.arithmetic_dialect == goldbox::EclArithmeticDialect::disabled);
    assert(pool.original_display && !curse.original_display);
    assert((pool.original_display->scene == std::array<std::uint16_t,4>{24,24,88,88}));
    assert(pool.original_display->symbols_record == 202 && pool.original_display->frame_runs.size() == 26);
    assert(pool.original_display->party_row_height == 16 && pool.original_display->party[3] == 104);
    if (!pool.campaign_startup || curse.campaign_startup || pool.campaign_startup->map_record != 0 ||
        pool.campaign_startup->program_record != 0 || pool.campaign_startup->position_refresh_call != 0x2c90 ||
        pool.campaign_startup->map_archives.size() != 8 || pool.campaign_startup->program_archives.size() != 8 ||
        pool.campaign_startup->graphics_archives != std::vector<std::string>{"PIC3.DAX","SPRIT3.DAX","HEAD3.DAX",
            "BODY3.DAX","WALLDEF3.DAX","8X8D3.DAX","8X8D1.DAX"} ||
        pool.script_delay_ms != 400 || pool.sprite_replacement_ms != 1000) return 1;
    if (!pool.speaker_audio || curse.speaker_audio || pool.speaker_audio->segment_offset != 0x8ae0 ||
        pool.speaker_audio->effect_table != 0x55f || pool.speaker_audio->preview_entry != 8 ||
        pool.speaker_audio->tick_divisor != 5041) return 1;
    if (pool.speaker_audio->effect_count != 20 || !pool.sound_call || curse.sound_call ||
        pool.sound_call->function != 0xba03 || pool.sound_call->selector_address != 0x03de ||
        pool.sound_call->cases.size() != 2) return 1;
    if (!pool.character_rules || curse.character_rules || pool.character_rules->id != pool.rules_profile ||
        pool.character_rules->party_limit != 6) return 1;
    if (pool.picture_period_ms != 400 || curse.picture_period_ms != 0) return 1;
    if (pool.startup_title_records != std::vector<std::uint8_t>{1,2} || !curse.startup_title_records.empty()) return 1;
    if (pool.startup_title_period_ms != 5000 || curse.startup_title_period_ms != 0) return 1;
    if (!pool.tandy_music || curse.tandy_music || pool.tandy_music->fingerprint != "11213555104201169956" ||
        pool.tandy_music->file_segment_offset != 11168 || pool.tandy_music->entry != 12 ||
        pool.tandy_music->frequencies != 1011 || pool.tandy_music->durations != 186) return 1;
    assert(pool.id != curse.id);
    assert(pool.archive_format == "dax-v1");
    assert(pool.archive_format == curse.archive_format);
    assert((pool.event_dispatch->movement_addresses ==
        std::array<std::uint16_t,4>{0x6dc9,0x6dd5,0x49f0,0x49f1}));
    assert(!curse.event_dispatch->movement_addresses);
    assert((pool.event_dispatch->transfer_clear_ranges ==
        std::vector<std::array<std::uint16_t,2>>{{0x4a00,32},{0x6e79,10}}));
    assert(curse.event_dispatch->transfer_clear_ranges.empty());
    assert(pool.campaign_startup->position_advance_call &&
        pool.campaign_startup->position_advance_call->function == 0xc01e &&
        pool.campaign_startup->position_advance_call->wall_type_address == 0xc04e);
    assert(pool.campaign_startup->program_graphics.at(20) ==
        (std::vector<std::string>{"PIC2.DAX","SPRIT2.DAX","HEAD2.DAX","BODY2.DAX","WALLDEF2.DAX","8X8D2.DAX","8X8D1.DAX"}));
    assert(pool.original_display->clock_addresses == (std::array<std::uint16_t,3>{0x49c7,0x49c8,0x49c9}));
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
    auto arithmetic_prefix = prefix;
    arithmetic_prefix.replace(arithmetic_prefix.find("colorBackdrop"),13,"arithmeticDialect");
    for (const auto value : {"\"unknown\"","false","3"}) {
        { std::ofstream output(path); output << arithmetic_prefix << value << '}'; }
        bool rejected = false;
        try { (void)goldbox::read_game_profile(path); } catch (const std::runtime_error&) { rejected = true; }
        fs::remove(path); assert(rejected);
    }
    auto reset_prefix = prefix;
    reset_prefix.replace(reset_prefix.find("colorBackdrop"),13,"eventDispatch");
    reset_prefix += R"({"dialect":"ssi-five-entry-v1","codeBase":39168,"squareInfoAddress":49231,"transferClearRanges":)";
    for (const std::string ranges : {R"([[1]])",R"([[-1,1]])",R"([[65535,2]])",R"([[1,0]])",
        R"([[1,65536]])",R"([[1,2],[2,1]])",R"([[1,"2"]])"}) {
        { std::ofstream o(path); o << reset_prefix << ranges << "}}"; }
        rejected = false;
        try { (void)goldbox::read_game_profile(path); }
        catch (const std::runtime_error&) { rejected = true; }
        fs::remove(path);
        assert(rejected);
    }
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
    auto audio_prefix = prefix;
    audio_prefix.replace(audio_prefix.find("colorBackdrop"),13,"speakerAudio");
    auto call_prefix = prefix;
    const std::string valid_music = R"({"fingerprint":"11213555104201169956","fileSegmentOffset":11168,"effectTable":1035,"effectCount":12,"table":1195,"entry":12,"durations":186,"frequencies":1011,"tickDivisor":5041})";
    const auto music_prefix = audio_prefix +
        R"({"segmentOffset":0,"effectTable":0,"previewEntry":0,"tickDivisor":1},"tandyMusic":)";
    for (const auto& [before,after] : std::vector<std::pair<std::string,std::string>>{
        {"11213555104201169956","18446744073709551616"},{"11168","2097152"},
        {"1035","65535"},{"1195","65535"},{"186","65535"},{"1011","65535"},{"5041","0"}}) {
        auto value = valid_music;
        value.replace(value.find(before),before.size(),after);
        { std::ofstream o(path); o << music_prefix << value << '}'; }
        rejected = false;
        try { (void)goldbox::read_game_profile(path); }
        catch (const std::runtime_error&) { rejected = true; }
        fs::remove(path);
        if (!rejected) return 1;
    }
    call_prefix.replace(call_prefix.find("colorBackdrop"),13,"soundCall");
    for (const std::string value : {
        R"({"function":1,"selectorAddress":65535,"cases":[]})",
        R"({"function":1,"selectorAddress":1,"cases":[{"selector":1,"sound":2,"extra":0}]})",
        R"({"function":1,"selectorAddress":1,"cases":[{"selector":1,"sound":256}]})",
        R"({"function":1,"selectorAddress":1,"cases":[{"selector":1,"sound":2},{"selector":1,"sound":3}]})"}) {
        { std::ofstream o(path); o << call_prefix << value << '}'; }
        rejected = false;
        try { (void)goldbox::read_game_profile(path); }
        catch (const std::runtime_error&) { rejected = true; }
        fs::remove(path);
        if (!rejected) return 1;
    }
    for (const std::string value : {
        R"({"segmentOffset":0,"effectTable":0,"previewEntry":0,"tickDivisor":0})",
        R"({"segmentOffset":-1,"effectTable":0,"previewEntry":0,"tickDivisor":1})",
        R"({"segmentOffset":0,"effectTable":65535,"previewEntry":0,"tickDivisor":1})",
        R"({"segmentOffset":0,"effectTable":0,"previewEntry":8192,"tickDivisor":1})",
        R"({"segmentOffset":0,"effectTable":0,"previewEntry":0,"tickDivisor":true})",
        R"({"segmentOffset":0,"effectTable":0,"previewEntry":0,"tickDivisor":1,"extra":0})"}) {
        { std::ofstream o(path); o << audio_prefix << value << '}'; }
        rejected = false;
        try { (void)goldbox::read_game_profile(path); }
        catch (const std::runtime_error&) { rejected = true; }
        fs::remove(path);
        if (!rejected) return 1;
    }
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
    timing_prefix.replace(timing_prefix.find("colorBackdrop"),13,"startupTitlePeriodMs");
    for (const std::string value : {"-1","60001","1.5","true","\"5000\"","5000"}) {
        { std::ofstream o(path); o << timing_prefix << value << '}'; }
        rejected = false;
        try { (void)goldbox::read_game_profile(path); }
        catch (const std::runtime_error&) { rejected = true; }
        fs::remove(path);
        if (!rejected) return 1; // A positive period also requires title records.
    }
    timing_prefix = prefix;
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
    auto campaign_prefix = prefix;
    campaign_prefix.replace(campaign_prefix.find("colorBackdrop"),13,"campaignStartup");
    for (const std::string field : {
        R"({"mapRecord":256,"programRecord":0,"positionRefreshCall":1,"mapArchives":["GEO.DAX"],"programArchives":["ECL.DAX"]})",
        R"({"mapRecord":0,"programRecord":0,"positionRefreshCall":1,"mapArchives":["../GEO.DAX"],"programArchives":["ECL.DAX"]})",
        R"({"mapRecord":0,"programRecord":0,"positionRefreshCall":1,"mapArchives":["GEO.DAX","GEO.DAX"],"programArchives":["ECL.DAX"]})",
        R"({"mapRecord":0,"programRecord":0,"positionRefreshCall":1,"mapArchives":[],"programArchives":["ECL.DAX"]})"}) {
        { std::ofstream output(path); output << campaign_prefix << field << '}'; }
        rejected = false;
        try { (void)goldbox::read_game_profile(path); }
        catch (const std::runtime_error&) { rejected = true; }
        fs::remove(path);
        if (!rejected) return 1;
    }
    const std::string seven = R"(["PIC.DAX","SPRIT.DAX","HEAD.DAX","BODY.DAX","WALL.DAX","TILE.DAX","SHARED.DAX"])";
    const std::string graphics_prefix = campaign_prefix +
        R"({"mapRecord":0,"programRecord":0,"positionRefreshCall":1,"mapArchives":["GEO.DAX"],"programArchives":["ECL.DAX"],"graphicsArchives":)" + seven +
        R"(,"programGraphics":)";
    for (const auto& entries : std::vector<std::string>{
        "[{\"programRecord\":0,\"graphicsArchives\":"+seven+"}]",
        "[{\"programRecord\":256,\"graphicsArchives\":"+seven+"}]",
        "[{\"programRecord\":20,\"graphicsArchives\":[]}]",
        "[{\"programRecord\":20,\"graphicsArchives\":"+seven+"},{\"programRecord\":20,\"graphicsArchives\":"+seven+"}]"}) {
        { std::ofstream output(path); output << graphics_prefix << entries << "}}"; }
        rejected = false;
        try { (void)goldbox::read_game_profile(path); } catch (const std::runtime_error&) { rejected = true; }
        fs::remove(path); assert(rejected);
    }
    auto display_prefix = prefix;
    display_prefix.replace(display_prefix.find("colorBackdrop"),13,"originalDisplay");
    const std::string display = R"({"fontArchive":"FONT.DAX","fontRecord":1,"symbolsArchive":"SYMBOL.DAX",)"
        R"("symbolsRecord":2,"canvas":[320,200],"position":[144,120],"scene":[32,24,88,88],)"
        R"("party":[144,16,168,80],"dialogue":[8,136,304,48],"commands":[0,192,320,8],)"
        R"("frameRuns":[[8,8,0,3,0]]})";
    for (const auto& change : {std::pair{"FONT.DAX","../FONT.DAX"},std::pair{"[320,200]","[0,200]"},
        std::pair{"[32,24,88,88]","[312,24,88,88]"},std::pair{"[8,8,0,3,0]","[8,8,0,0,0]"},
        std::pair{"[8,8,0,3,0]","[8,8,0,3,2]"},std::pair{"[8,8,0,3,0]","[8,8,256,3,0]"}}) {
        auto invalid = display;
        invalid.replace(invalid.find(change.first),std::string_view(change.first).size(),change.second);
        { std::ofstream output(path); output << display_prefix << invalid << '}'; }
        bool rejected = false;
        try { (void)goldbox::read_game_profile(path); }
        catch (const std::runtime_error&) { rejected = true; }
        fs::remove(path); assert(rejected);
    }
    for (const auto addresses : {"[]","[1,2]","[1,2,3,4]","[-1,2,3]","[65536,2,3]","[true,2,3]","[1,1,3]"}) {
        auto invalid = display;
        invalid.insert(invalid.size()-1,std::string(",\"clockAddresses\":")+addresses);
        { std::ofstream output(path); output << display_prefix << invalid << '}'; }
        bool rejected = false;
        try { (void)goldbox::read_game_profile(path); } catch (const std::runtime_error&) { rejected = true; }
        fs::remove(path); assert(rejected);
    }
    auto movement_prefix = prefix;
    movement_prefix.replace(movement_prefix.find("colorBackdrop"),13,"eventDispatch");
    const std::string movement = R"({"dialect":"ssi-five-entry-v1","codeBase":39168,"squareInfoAddress":28931,"positionAddresses":[28928,28929,28930],"movementAddresses":)";
    for (const std::string addresses : {"[]","[1,2,3]","[1,2,3,4,5]","[-1,2,3,4]",
        "[65536,2,3,4]","[true,2,3,4]","[1,1,3,4]","[28928,2,3,4]","[28931,2,3,4]"}) {
        { std::ofstream output(path); output << movement_prefix << movement << addresses << "}}"; }
        bool rejected = false;
        try { (void)goldbox::read_game_profile(path); }
        catch (const std::runtime_error&) { rejected = true; }
        fs::remove(path); assert(rejected);
    }
    auto advance_prefix = prefix;
    advance_prefix.replace(advance_prefix.find("colorBackdrop"),13,"campaignStartup");
    const std::string advance_startup = R"({"mapRecord":0,"programRecord":0,"mapArchives":["MAP.DAX"],"programArchives":["ECL.DAX"],"positionRefreshCall":1,"positionAdvanceCall":)";
    for (const std::string call : {"null",R"({"function":-1,"wallTypeAddress":2})",
        R"({"function":65536,"wallTypeAddress":2})",R"({"function":1,"wallTypeAddress":65536})",
        R"({"function":1,"wallTypeAddress":true})",R"({"function":1})",
        R"({"function":1,"wallTypeAddress":2,"unknown":3})"}) {
        { std::ofstream output(path); output << advance_prefix << advance_startup << call << "}}"; }
        bool rejected = false;
        try { (void)goldbox::read_game_profile(path); }
        catch (const std::runtime_error&) { rejected = true; }
        fs::remove(path); assert(rejected);
    }
    return 0;
}
