#include "goldbox/content.hpp"
#include "goldbox/mods.hpp"
#include "goldbox/game_profile.hpp"
#include "goldbox/formats/dax.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

namespace {

struct Options {
    fs::path content;
    fs::path mods;
    fs::path save_meta;
    fs::path scan_dax;
    fs::path profile;
    goldbox::JournalId entry = 17;
    bool enhanced = true;
};

void usage() {
    std::cout << "Goldbox Engine M1: journal import and mod overlays (no playable game yet)\n"
                 "Usage: goldbox_cli --content <journals.json> [--mods <directory>]\n"
                 "        [--journal <positive-id>] [--mode classic|enhanced] [--save-meta <file>]\n"
                 "       goldbox_cli --scan-dax <archive.DAX> [--game-profile <profile.json>]\n";
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
    if (!opts.scan_dax.empty()) {
        if (!opts.content.empty() || !opts.mods.empty() || !opts.save_meta.empty()) {
            throw std::runtime_error("--scan-dax cannot be combined with content/mod options");
        }
        return opts;
    }
    if (!opts.profile.empty()) throw std::runtime_error("--game-profile requires --scan-dax");
    if (opts.content.empty()) throw std::runtime_error("--content is required");
    return opts;
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 1) { usage(); return 0; }
        const auto opts = parse(argc, argv);
        if (!opts.scan_dax.empty()) {
            if (!opts.profile.empty()) {
                const auto profile = goldbox::read_game_profile(opts.profile);
                if (profile.archive_format != "dax-v1") {
                    throw std::runtime_error("game profile requires a different archive decoder");
                }
                std::cout << "Game profile: " << profile.title
                          << " [" << profile.validation_status << "]\n";
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
