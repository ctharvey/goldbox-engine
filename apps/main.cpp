#include "goldbox/content.hpp"
#include "goldbox/mods.hpp"
#include "goldbox/game_profile.hpp"
#include "goldbox/formats/dax.hpp"
#include "goldbox/formats/geo.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

namespace {

struct Options {
    fs::path content;
    fs::path mods;
    fs::path save_meta;
    fs::path scan_dax;
    fs::path scan_geo;
    fs::path ecl_dax;
    fs::path profile;
    std::optional<std::uint8_t> geo_record;
    std::optional<std::uint8_t> ecl_record;
    goldbox::JournalId entry = 17;
    bool enhanced = true;
};

void usage() {
    std::cout << "Goldbox Engine M2: bounded resource inspection and journal overlays (not playable yet)\n"
                 "Usage: goldbox_cli --content <journals.json> [--mods <directory>]\n"
                 "        [--journal <positive-id>] [--mode classic|enhanced] [--save-meta <file>]\n"
                 "       goldbox_cli --scan-dax <archive.DAX> [--game-profile <profile.json>]\n"
                 "       goldbox_cli --scan-geo <GEO.DAX> [--geo-record <id>]\n"
                 "           [--game-profile <profile.json>]\n"
                 "           [--ecl-dax <ECL.DAX> --ecl-record <id> --geo-record <id>]\n";
}

Options parse(int argc, char** argv) {
    Options opts;
    for (int i = 1; i < argc; ++i) {
        const std::string arg(argv[i]);
        if (arg == "--help") { usage(); std::exit(0); }
        if (i + 1 >= argc) throw std::runtime_error("missing value after " + arg);
        const std::string value(argv[++i]);
        if (arg == "--content") opts.content = value;
        else if (arg == "--mods") opts.mods = value;
        else if (arg == "--save-meta") opts.save_meta = value;
        else if (arg == "--scan-dax") opts.scan_dax = value;
        else if (arg == "--scan-geo") opts.scan_geo = value;
        else if (arg == "--ecl-dax") opts.ecl_dax = value;
        else if (arg == "--geo-record" || arg == "--ecl-record") {
            std::size_t consumed = 0;
            unsigned long long id = 0;
            try { id = std::stoull(value, &consumed); }
            catch (const std::exception&) { throw std::runtime_error("invalid record id for " + arg); }
            if (value.empty() || value.front() == '-' || consumed != value.size() || id > 255) {
                throw std::runtime_error(arg + " must be an unsigned 8-bit record id");
            }
            const auto byte_id = static_cast<std::uint8_t>(id);
            if (arg == "--geo-record") opts.geo_record = byte_id;
            else opts.ecl_record = byte_id;
        }
        else if (arg == "--game-profile") opts.profile = value;
        else if (arg == "--mode") {
            if (value != "classic" && value != "enhanced") throw std::runtime_error("mode must be classic or enhanced");
            opts.enhanced = value == "enhanced";
        } else if (arg == "--journal") {
            std::size_t consumed = 0;
            unsigned long long id = 0;
            try { id = std::stoull(value, &consumed); }
            catch (const std::exception&) { throw std::runtime_error("invalid journal id"); }
            if (value.empty() || value.front() == '-' || consumed != value.size() || id == 0 ||
                id > std::numeric_limits<goldbox::JournalId>::max()) {
                throw std::runtime_error("journal must be a positive 32-bit integer");
            }
            opts.entry = static_cast<goldbox::JournalId>(id);
        } else throw std::runtime_error("unknown argument: " + arg);
    }
    if (!opts.scan_dax.empty() && !opts.scan_geo.empty()) {
        throw std::runtime_error("--scan-dax and --scan-geo are mutually exclusive");
    }
    if (!opts.scan_dax.empty() || !opts.scan_geo.empty()) {
        if (!opts.content.empty() || !opts.mods.empty() || !opts.save_meta.empty()) {
            throw std::runtime_error("resource scans cannot be combined with content/mod options");
        }
        if (!opts.scan_dax.empty() && (opts.geo_record || !opts.ecl_dax.empty() || opts.ecl_record)) {
            throw std::runtime_error("GEO/ECL record options require --scan-geo");
        }
        if (opts.ecl_dax.empty() != !opts.ecl_record.has_value()) {
            throw std::runtime_error("--ecl-dax and --ecl-record must be specified together");
        }
        if (!opts.ecl_dax.empty() && !opts.geo_record) {
            throw std::runtime_error("ECL pairing requires a specific --geo-record");
        }
        return opts;
    }
    if (!opts.profile.empty() || !opts.ecl_dax.empty() || opts.geo_record || opts.ecl_record) {
        throw std::runtime_error("resource scan options require --scan-geo or --scan-dax");
    }
    if (opts.content.empty()) throw std::runtime_error("--content is required");
    return opts;
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 1) { usage(); return 0; }
        const auto opts = parse(argc, argv);
        if (!opts.scan_dax.empty() || !opts.scan_geo.empty()) {
            if (!opts.profile.empty()) {
                const auto profile = goldbox::read_game_profile(opts.profile);
                if (profile.archive_format != "dax-v1") {
                    throw std::runtime_error("game profile requires a different archive decoder");
                }
                std::cout << "Game profile: " << profile.title
                          << " [" << profile.validation_status << "]\n";
            }
            if (!opts.scan_geo.empty()) {
                const auto archive = goldbox::formats::DaxArchive::from_file(opts.scan_geo);
                std::optional<goldbox::formats::DaxArchive> ecl_archive;
                if (!opts.ecl_dax.empty()) {
                    ecl_archive = goldbox::formats::DaxArchive::from_file(opts.ecl_dax);
                }
                std::size_t scanned = 0;
                for (const auto& record : archive.records()) {
                    if (opts.geo_record && record.id != *opts.geo_record) continue;
                    const auto map = goldbox::formats::extract_geo16_map(archive, record.id);
                    const auto references = ecl_archive
                        ? goldbox::formats::bind_geo_events_to_ecl(map, *ecl_archive, *opts.ecl_record)
                        : goldbox::formats::geo_event_references(map);
                    const auto flagged = std::count_if(map.cells.begin(), map.cells.end(),
                        [](const goldbox::formats::GeoCell& cell) { return cell.event_high_bit(); });
                    std::cout << "GEO #" << static_cast<unsigned>(map.dax_record_id)
                              << ": 16x16 cells, " << references.size() << " raw event triggers"
                              << ", " << flagged << " flagged cells"
                              << ", " << map.trailing_bytes << " trailing bytes";
                    if (ecl_archive) {
                        std::cout << ", paired ECL #" << static_cast<unsigned>(*opts.ecl_record)
                                  << " (entry addresses unresolved)";
                    }
                    std::cout << "\n";
                    ++scanned;
                }
                if (opts.geo_record && scanned == 0) {
                    throw std::runtime_error("requested GEO record not found");
                }
                std::cout << "GEO scan: " << scanned << " map records checked (unverified format)\n";
                return 0;
            }
            const auto archive = goldbox::formats::DaxArchive::from_file(opts.scan_dax);
            std::cout << "DAX index: " << archive.records().size() << " records"
                      << " (data offset " << archive.data_offset() << ")\n";
            for (const auto& record : archive.records()) {
                std::cout << "  #" << static_cast<unsigned>(record.id)
                          << " offset=" << record.relative_offset
                          << " packed=" << record.stored_size
                          << " unpacked=" << record.unpacked_size << "\n";
            }
            return 0;
        }
        const auto content = goldbox::SyntheticJsonImporter{}.load(opts.content);
        const auto mods = opts.mods.empty() ? goldbox::ModResolution{} :
            goldbox::resolve_mods(goldbox::discover_mods(opts.mods), content);
        const auto journal = goldbox::build_journal(content, mods, opts.enhanced);
        for (const auto& warning : mods.warnings) std::cerr << "Warning: " << warning << "\n";
        const auto entry = journal.resolve(opts.entry);
        if (!entry) throw std::runtime_error("journal #" + std::to_string(opts.entry) + " not found");
        std::cout << "Game: " << content.game_id << " | Mode: " << (opts.enhanced ? "enhanced" : "classic")
                  << " | Mods: " << (opts.enhanced ? mods.ordered.size() : 0) << "\n"
                  << "Journal #" << entry->id << ": " << entry->title << "\n" << entry->body << "\n";
        if (!opts.save_meta.empty()) {
            if (fs::exists(opts.save_meta)) throw std::runtime_error("refusing to overwrite existing session metadata");
            std::ofstream output(opts.save_meta, std::ios::binary | std::ios::trunc);
            if (!output) throw std::runtime_error("cannot write session metadata");
            output << goldbox::make_session_metadata(content, mods, opts.enhanced) << '\n';
            if (!output) throw std::runtime_error("failed writing session metadata");
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << "\n";
        return 1;
    }
}
