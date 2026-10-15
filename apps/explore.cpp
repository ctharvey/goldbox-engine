// SPDX-License-Identifier: GPL-3.0-or-later
#include "goldbox/exploration.hpp"
#include "goldbox/game_profile.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <sstream>

namespace fs = std::filesystem;
namespace {
std::uint8_t record_id(const std::string& text) {
    std::size_t consumed = 0;
    const auto id = std::stoul(text, &consumed);
    if (text.empty() || text.front() == '-' || consumed != text.size() || id > 255) {
        throw std::runtime_error("Record ID must be an unsigned byte");
    }
    return static_cast<std::uint8_t>(id);
}

// Non-cryptographic change detector for saves, not a SHA-256 inventory claim.
std::uint64_t checksum(std::span<const std::uint8_t> bytes) {
    std::uint64_t hash = 14695981039346656037ull;
    for (const auto byte : bytes) hash = (hash ^ byte) * 1099511628211ull;
    return hash;
}

class TerminalHost final : public goldbox::EclHost {
public:
    void text(std::string_view text, bool clear) override {
        if (clear) std::cout << "\n";
        std::cout << text << "\n";
    }
    std::size_t menu(std::span<const std::string> choices, std::string_view prompt, bool) override {
        std::cout << prompt << "\n";
        for (std::size_t i = 0; i < choices.size(); ++i) std::cout << i << ": " << choices[i] << "\n";
        std::string input;
        if (!std::getline(std::cin, input)) throw std::runtime_error("Menu input ended");
        if (!input.empty() && input.back() == '\r') input.pop_back();
        std::size_t consumed = 0;
        const auto choice = std::stoul(input, &consumed);
        if (consumed != input.size()) throw std::runtime_error("Invalid menu choice");
        return choice;
    }
};
} // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 7) {
            std::cout << "Usage: goldbox_explore <profile.json> <GEO.DAX> <geo-id> <ECL.DAX> <ecl-id> <local-save.json>\n"
                         "Explicit map/ECL pairing required. Unverified exploration pilot; no complete game support.\n";
            return argc == 1 ? 0 : 1;
        }
        const auto profile = goldbox::read_game_profile(argv[1]);
        if (!profile.event_dispatch || profile.archive_format != "dax-v1") throw std::runtime_error("Unsupported exploration profile");
        const auto& config = *profile.event_dispatch;
        const auto geo_id = record_id(argv[3]), ecl_id = record_id(argv[5]);
        const auto raw_map = goldbox::formats::DaxArchive::from_file(argv[2]).extract(geo_id);
        auto ecl = goldbox::formats::DaxArchive::from_file(argv[4]).extract(ecl_id);
        goldbox::ExplorationSession session;
        session.map = goldbox::formats::decode_geo16_record(raw_map, geo_id);
        session.ecl_record = std::move(ecl);
        session.dialect = {config.dialect, config.code_base, config.square_info_address, true, config.position_addresses};
        session.opcodes = goldbox::minimal_vm_opcodes();
        (void)goldbox::formats::decode_ecl_program_entries(session.ecl_record, session.dialect, session.opcodes);
        std::ostringstream identity;
        identity << profile.id << ':' << fs::path(argv[2]).filename().string() << ':' << unsigned(geo_id) << ':'
                 << fs::path(argv[4]).filename().string() << ':' << unsigned(ecl_id) << ':'
                 << checksum(raw_map) << ':' << checksum(session.ecl_record) << ':'
                 << config.code_base << ':' << config.square_info_address;
        if (config.position_addresses) for (const auto address : *config.position_addresses) identity << ':' << address;
        const auto source_identity = identity.str();
        const auto save = fs::absolute(argv[6]);
        if (save == fs::absolute(argv[1]) || save == fs::absolute(argv[2]) || save == fs::absolute(argv[4])) {
            throw std::runtime_error("Save path must not be an input resource");
        }
        TerminalHost host;
        const auto trigger = [&](goldbox::formats::EclEventKind kind) {
            const auto result = session.trigger(kind, host);
            if (!result.exited) std::cout << "Event stopped at offset " << result.bytecode_offset << ": " << result.diagnostic << "\n";
            else std::cout << "Event exited after " << result.steps << " steps\n";
        };
        std::cout << "Unverified exploration pilot. Starts at (0,0); use init explicitly if supported.\n"
                     "Commands: north east south west search rest init save load quit\n";
        std::string command;
        while (std::cout << "(" << unsigned(session.row) << ',' << unsigned(session.column) << ")> " &&
               std::getline(std::cin, command)) {
            if (!command.empty() && command.back() == '\r') command.pop_back();
            if (command == "quit") break;
            if (command == "save") {
                if (fs::exists(save) || fs::is_symlink(save)) throw std::runtime_error("Refusing to overwrite an existing save");
                std::ofstream output(save, std::ios::binary);
                output << goldbox::serialize_exploration(session, source_identity) << '\n';
                if (!output) throw std::runtime_error("Failed writing save");
                std::cout << "State saved\n";
            } else if (command == "load") {
                if (fs::is_symlink(save) || !fs::is_regular_file(save) || fs::file_size(save) > 4 * 1024 * 1024) {
                    throw std::runtime_error("Invalid save file");
                }
                std::ifstream input(save, std::ios::binary);
                if (!input) throw std::runtime_error("Cannot read save");
                const std::string json{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
                goldbox::restore_exploration(session, json, source_identity);
                std::cout << "State restored\n";
            } else if (command == "init") trigger(goldbox::formats::EclEventKind::initialize);
            else if (command == "search") trigger(goldbox::formats::EclEventKind::search);
            else if (command == "rest") trigger(goldbox::formats::EclEventKind::rest);
            else {
                std::optional<goldbox::Direction> direction;
                if (command == "north") direction = goldbox::Direction::north;
                if (command == "east") direction = goldbox::Direction::east;
                if (command == "south") direction = goldbox::Direction::south;
                if (command == "west") direction = goldbox::Direction::west;
                if (!direction) { std::cout << "Unknown command\n"; continue; }
                if (session.move(*direction)) trigger(goldbox::formats::EclEventKind::move);
                else std::cout << "Movement blocked\n";
            }
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << "\n";
        return 1;
    }
}
