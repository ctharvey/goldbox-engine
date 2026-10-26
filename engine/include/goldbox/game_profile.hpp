#pragma once
#include "goldbox/formats/graphics.hpp"
#include "goldbox/party.hpp"
#include "goldbox/party_memory.hpp"
#include "goldbox/ecl_vm.hpp"

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
        // Block, exit attempt, previous X, previous Y; opts into source-cell MOVE.
        std::optional<std::array<std::uint16_t, 4>> movement_addresses{};
        // Numeric RAM ranges [start, length] cleared before transferred-program initialization.
        std::vector<std::array<std::uint16_t, 2>> transfer_clear_ranges;
    };
    std::optional<EventDispatch> event_dispatch;
    std::string image_dialect{"disabled"};
    struct ColorBackdrop {
        std::array<std::uint16_t,2> sky_color_addresses; // Outdoors, indoors.
    };
    std::optional<ColorBackdrop> color_backdrop;
    struct Portrait {
        std::uint16_t head_id_address{};
        formats::EgaPortraitLayout layout;
    };
    std::optional<Portrait> portrait;
    std::string area_dialect{"disabled"};
    std::uint32_t picture_period_ms{};
    std::vector<std::uint8_t> startup_title_records; // Ordered screens; presentation controls the final backdrop.
    std::optional<CreationRules> character_rules{};
    std::optional<PartyMemoryLayout> party_memory{};
    std::optional<PartyStrengthBinding> party_strength{};
    std::optional<HighWordLcg32> random{};
    EclRandomDialect random_dialect{EclRandomDialect::disabled};
    struct SpeakerAudio {
        std::uint32_t segment_offset{};
        std::uint16_t effect_table{}, preview_entry{}, tick_divisor{};
        std::uint16_t effect_count{};
    };
    std::optional<SpeakerAudio> speaker_audio{};
    struct TandyMusic {
        std::string fingerprint; // FNV-1a64 edition selector, not a security checksum.
        std::uint32_t file_segment_offset{};
        std::uint16_t effect_table{}, effect_count{}, table{}, entry{}, durations{}, frequencies{}, tick_divisor{};
    };
    std::optional<TandyMusic> tandy_music{};
    std::optional<EclSoundCall> sound_call{};
    // Zero preserves the legacy final-title menu; timed titles precede a separate menu.
    std::uint32_t startup_title_period_ms{};
    struct CampaignStartup {
        std::uint8_t map_record{}, program_record{};
        std::vector<std::string> map_archives, program_archives;
        std::uint16_t position_refresh_call{};
        // Pictures, sprites, heads, bodies, wall definitions, wall tiles, shared tiles.
        std::vector<std::string> graphics_archives;
        std::optional<EclPositionAdvanceCall> position_advance_call{};
        std::map<std::uint8_t,std::vector<std::string>> program_graphics{};
    };
    std::optional<CampaignStartup> campaign_startup{};
    std::uint32_t script_delay_ms{}, sprite_replacement_ms{};
    struct OriginalDisplay {
        std::string font_archive, symbols_archive;
        std::uint8_t font_record{}, symbols_record{};
        std::array<std::uint16_t,2> canvas{}, position{};
        std::array<std::uint16_t,4> scene{}, party{}, dialogue{}, commands{};
        std::uint16_t party_row_height{8};
        // Script globals: minute units, minute tens, hours. Presentation is read-only.
        std::optional<std::array<std::uint16_t,3>> clock_addresses{};
        // X, Y, symbol index, repeat count, vertical (0/1); symbols are 8x8.
        std::vector<std::array<std::uint16_t,5>> frame_runs;
    };
    std::optional<OriginalDisplay> original_display{};
    EclArithmeticDialect arithmetic_dialect{EclArithmeticDialect::disabled};
};

[[nodiscard]] GameProfile read_game_profile(const std::filesystem::path& path);

} // namespace goldbox
