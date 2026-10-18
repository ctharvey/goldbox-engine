// SPDX-License-Identifier: GPL-3.0-or-later
#include "goldbox/exploration.hpp"
#include "goldbox/game_profile.hpp"
#ifdef GOLDBOX_RENDERER
#include "view.hpp"
#endif

#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <sstream>

#include <boost/json.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace bj = boost::json;
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

// Protocol failure that deliberately does not derive from std::exception, so
// the ECL VM's own error handling cannot swallow or rewrite harness protocol
// errors; run_harness converts it into a JSON error event.
struct ProtocolError {
    ProtocolError(std::string message) : message(std::move(message)) {}
    std::string message;
};

// One JSONL line, bounded before any further accumulation; a line that would
// exceed 64KiB or an EOF both fail the protocol immediately.
std::string bounded_line(std::istream& input) {
    constexpr std::size_t kLineLimit = 64 * 1024;
    std::string line;
    while (true) {
        const auto character = input.get();
        if (character == std::istream::traits_type::eof()) {
            throw ProtocolError("EOF before quit");
        }
        if (character == '\n') {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            return line;
        }
        if (line.size() >= kLineLimit) {
            throw ProtocolError("Input line exceeds 64 KiB");
        }
        line.push_back(static_cast<char>(character));
    }
}

// Flushed JSONL emission; diagnostics in harness mode never mix into stdout.
void emit_json(const bj::value& event) {
    std::cout << bj::serialize(event) << '\n';
    std::cout.flush();
    if (!std::cout) throw ProtocolError("Harness output stream failed");
}

// Strict integer fields: JSON integer types only, never bools, doubles or
// strings; negatives (and for some fields zero) are rejected by the caller.
std::uint64_t required_unsigned(const bj::object& fields, std::string_view key) {
    const auto found = fields.find(key);
    if (found == fields.end()) throw ProtocolError(std::string(key) + " field is required");
    const auto& value = found->value();
    if (value.is_int64()) {
        if (value.get_int64() < 0) throw ProtocolError(std::string(key) + " must be nonnegative");
        return static_cast<std::uint64_t>(value.get_int64());
    }
    if (value.is_uint64()) return value.get_uint64();
    throw ProtocolError(std::string(key) + " must be an integer");
}

bj::value protocol_request(std::istream& input) {
    try {
        return bj::parse(bounded_line(input));
    } catch (const std::exception& error) {
        throw ProtocolError(std::string("Invalid JSON: ") + error.what());
    }
}

// Host callbacks over the flushed JSONL protocol. The VM drives every call;
// no OS input is read except the single menu response line below.
class HarnessHost final : public goldbox::EclHost {
public:
    explicit HarnessHost(std::istream& input) : input_(input) {}
    std::uint64_t current_id = 0;
    std::uint64_t next_menu_id = 1;

    void text(std::string_view text, bool clear) override {
        emit_json(bj::object{{"type", "text"}, {"id", current_id},
            {"text", text}, {"clear", clear}});
    }

    std::size_t menu(std::span<const std::string> choices, std::string_view prompt, bool vertical) override {
        bj::array items;
        for (const auto& choice : choices) items.emplace_back(choice);
        const auto menu_id = next_menu_id++;
        emit_json(bj::object{{"type", "menu"}, {"id", current_id}, {"menu_id", menu_id},
            {"prompt", std::string(prompt)}, {"choices", std::move(items)},
            {"vertical", vertical}});
        const auto fields = protocol_request(input_);
        if (!fields.is_object()) throw ProtocolError("Menu response must be a JSON object");
        const auto& object = fields.get_object();
        if (object.size() != 3) throw ProtocolError("Menu response must contain exactly id, menu_id and choice");
        const auto id = required_unsigned(object, "id");
        const auto menu = required_unsigned(object, "menu_id");
        const auto choice = required_unsigned(object, "choice");
        if (id != current_id) throw ProtocolError("Menu response id must match the pending action id");
        if (menu != menu_id) throw ProtocolError("Menu response menu_id must match the pending menu id");
        if (choice >= choices.size()) throw ProtocolError("Menu choice outside displayed menu range");
        return static_cast<std::size_t>(choice);
    }

private:
    std::istream& input_;
};

// Machine-facing exploration driver: JSONL requests in, flushed JSONL out.
// Reuses the same session/serialize/restore engine as the terminal pilot; no
// VM behavior or budget changes, and no original-parity claim.
int run_harness(goldbox::ExplorationSession& session, const std::string& source_identity,
    const fs::path& save) {
    HarnessHost host(std::cin);
    emit_json(bj::object{{"type", "ready"}, {"protocol", 1},
        {"source_identity", source_identity}, {"row", unsigned(session.row)},
        {"column", unsigned(session.column)}, {"facing", unsigned(session.facing)},
        {"baseline", "unverified-empty"}});

    const auto complete = [&](std::string status, std::optional<goldbox::EclExecution> execution) {
        if (execution.has_value() && !execution->exited) status = "unsupported";
        const auto& run = execution.has_value() ? *execution : goldbox::EclExecution{};
        bj::array routes;
        for (const auto& dispatch : run.dispatches) routes.push_back(bj::object{
            {"table_offset", dispatch.table_offset}, {"selector", dispatch.selector},
            {"target_offset", dispatch.target_offset}, {"subroutine", dispatch.subroutine}});
        auto document = bj::object{{"type", "complete"}, {"id", host.current_id},
            {"status", status}, {"row", unsigned(session.row)},
            {"column", unsigned(session.column)}, {"facing", unsigned(session.facing)},
            {"steps", run.steps}, {"bytecode_offset", run.bytecode_offset},
            {"dispatches", std::move(routes)}, {"diagnostic", run.diagnostic}};
        document["state"] = bj::parse(goldbox::serialize_exploration(session, source_identity));
        emit_json(document);
    };

    try {
        std::uint64_t previous_id = 0;
        while (true) {
            const auto request = protocol_request(std::cin);
            if (!request.is_object()) throw ProtocolError("Request must be a JSON object");
            auto current_id = required_unsigned(request.get_object(), "id");
            if (current_id == 0) throw ProtocolError("Request id must be positive");
            if (current_id <= previous_id) throw ProtocolError("Request id must strictly increase");
            previous_id = current_id;
            host.current_id = current_id;
            const auto action_field = request.get_object().find("action");
            if (action_field == request.get_object().end() || !action_field->value().is_string()) {
                throw ProtocolError("action must be a JSON string");
            }
            const auto& action_name = action_field->value().get_string();
            const std::string action(action_name.data(), action_name.size());

            if (action == "quit") {
                complete("ok", std::nullopt);
                return 0;
            }
            if (action == "north" || action == "east" || action == "south" || action == "west") {
                const auto direction = action == "north" ? goldbox::Direction::north :
                    action == "east" ? goldbox::Direction::east :
                    action == "south" ? goldbox::Direction::south : goldbox::Direction::west;
                if (session.move(direction)) {
                    complete("ok", session.trigger(goldbox::formats::EclEventKind::move, host));
                } else {
                    complete("blocked", std::nullopt);
                }
            } else if (action == "search") {
                complete("ok", session.trigger(goldbox::formats::EclEventKind::search, host));
            } else if (action == "rest") {
                complete("ok", session.trigger(goldbox::formats::EclEventKind::rest, host));
            } else if (action == "init") {
                complete("ok", session.trigger(goldbox::formats::EclEventKind::initialize, host));
            } else if (action == "save") {
                if (fs::exists(save) || fs::is_symlink(save)) {
                    throw std::runtime_error("Refusing to overwrite an existing save");
                }
                std::ofstream output(save, std::ios::binary);
                output << goldbox::serialize_exploration(session, source_identity) << '\n';
                if (!output) throw std::runtime_error("Failed writing save");
                complete("ok", std::nullopt);
            } else if (action == "load") {
                if (fs::is_symlink(save) || !fs::is_regular_file(save) || fs::file_size(save) > 4 * 1024 * 1024) {
                    throw std::runtime_error("Invalid save file");
                }
                std::ifstream input(save, std::ios::binary);
                if (!input) throw std::runtime_error("Cannot read save");
                const std::string json{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
                goldbox::restore_exploration(session, json, source_identity);
                complete("ok", std::nullopt);
            } else {
                throw ProtocolError("Unknown harness action: " + action);
            }
        }
    } catch (const ProtocolError& error) {
        auto document = bj::object{{"type", "error"}, {"message", error.message}};
        if (host.current_id != 0) document["id"] = host.current_id;
        emit_json(document);
        return 1;
    } catch (const std::exception& error) {
        auto document = bj::object{{"type", "error"}, {"message", error.what()}};
        if (host.current_id != 0) document["id"] = host.current_id;
        emit_json(document);
        return 1;
    }
}
} // namespace

int main(int argc, char** argv) {
    const bool window = argc > 1 && std::string_view(argv[1]) == "--window";
    if (window) { --argc; ++argv; }
    const bool harness = argc > 1 && std::string_view(argv[1]) == "--harness";
    if (harness) { --argc; ++argv; }
    try {
        if (window && harness) throw std::runtime_error("Window and harness modes are separate");
        std::optional<std::uint32_t> seed;
        for (int i = 1; i < argc; ++i) if (std::string_view(argv[i]) == "--seed") {
            if (seed || i+1 >= argc) throw std::runtime_error("--seed requires one unsigned 32-bit value");
            std::string_view text(argv[i+1]);
            int base = 10;
            if (text.starts_with("0x") || text.starts_with("0X")) { text.remove_prefix(2); base = 16; }
            std::uint32_t value{};
            const auto result = std::from_chars(text.data(),text.data()+text.size(),value,base);
            if (text.empty() || result.ec != std::errc{} || result.ptr != text.data()+text.size())
                throw std::runtime_error("Invalid --seed value");
            seed = value;
            for (int j = i; j+2 < argc; ++j) argv[j] = argv[j+2];
            argc -= 2; --i;
        }

#ifndef GOLDBOX_RENDERER
        if (window) throw std::runtime_error("Build with GOLDBOX_BUILD_RENDERER=ON for --window");
#endif
#ifdef GOLDBOX_RENDERER
        std::optional<goldbox::formats::WallGraphics> graphics;
        std::optional<goldbox::WallArchives> wall_archives;
        std::optional<std::array<std::uint8_t,3>> initial_banks;
        std::optional<goldbox::ImageArchives> images;
        std::optional<goldbox::PortraitArchives> portraits;
        std::vector<goldbox::formats::IndexedImage> font;
        std::optional<goldbox::formats::DaxArchive> startup_archive;
        std::optional<std::filesystem::path> audio_file;
        while (window && argc > 1 && (std::string_view(argv[1]) == "--walls" ||
            std::string_view(argv[1]) == "--images" || std::string_view(argv[1]) == "--font" ||
            std::string_view(argv[1]) == "--portraits" || std::string_view(argv[1]) == "--startup" ||
            std::string_view(argv[1]) == "--audio")) {
            if (std::string_view(argv[1]) == "--audio") {
                if (argc < 3 || audio_file) throw std::runtime_error("--audio requires one executable");
                audio_file = argv[2];
                argc -= 2; argv += 2;
                continue;
            }
            if (std::string_view(argv[1]) == "--startup") {
                if (argc < 3 || startup_archive) throw std::runtime_error("--startup requires one title archive");
                startup_archive.emplace(goldbox::formats::DaxArchive::from_file(argv[2]));
                argc -= 2; argv += 2;
                continue;
            }
            if (std::string_view(argv[1]) == "--portraits") {
                if (argc < 4 || portraits) throw std::runtime_error("--portraits requires HEAD and BODY archives");
                portraits.emplace(goldbox::PortraitArchives{goldbox::formats::DaxArchive::from_file(argv[2]),
                    goldbox::formats::DaxArchive::from_file(argv[3])});
                argc -= 3; argv += 3;
                continue;
            }
            if (std::string_view(argv[1]) == "--font") {
                if (argc < 4 || !font.empty()) throw std::runtime_error("--font requires one archive and record ID");
                font = goldbox::formats::decode_mono8_tiles(
                    goldbox::formats::DaxArchive::from_file(argv[2]).extract(record_id(argv[3])));
                (void)goldbox::compose_font_atlas(font);
                argc -= 3; argv += 3;
                continue;
            }
            if (std::string_view(argv[1]) == "--images") {
                if (argc < 5 || images) throw std::runtime_error("--images requires one PIC and one SPRIT archive");
                images.emplace(goldbox::ImageArchives{goldbox::formats::DaxArchive::from_file(argv[2]),
                    goldbox::formats::DaxArchive::from_file(argv[3])});
                argc -= 3; argv += 3;
                continue;
            }
            if (graphics) throw std::runtime_error("Duplicate --walls selection");
            if (argc < 9) throw std::runtime_error("--walls requires definitions, tiles, shared tiles and three bank IDs");
            wall_archives.emplace(goldbox::WallArchives{
                goldbox::formats::DaxArchive::from_file(argv[2]),
                goldbox::formats::DaxArchive::from_file(argv[3]),
                goldbox::formats::DaxArchive::from_file(argv[4])});
            initial_banks = std::array<std::uint8_t,3>{record_id(argv[5]),record_id(argv[6]),record_id(argv[7])};
            graphics = goldbox::formats::load_wall_graphics(wall_archives->definitions,wall_archives->tiles,
                wall_archives->shared,*initial_banks);
            argc -= 7;
            argv += 7;
        }
#endif
        if (argc != 7) {
            if (harness) throw ProtocolError("Harness mode requires six resource/save arguments");
            std::cout << "Usage: goldbox_explore [--harness|--window] <profile.json> <GEO.DAX> <geo-id> <ECL.DAX> <ecl-id> <local-save.json>\n"
                         "Optional --seed <decimal-or-0x-uint32> overrides the profile startup seed.\n"
                         "Explicit map/ECL pairing required. Unverified exploration pilot; no complete game support.\n"
                         "With --harness as the first argument, the same six arguments run the JSONL protocol instead.\n";
            std::cout << "With --window, optional --walls <WALLDEF.DAX> <8X8D.DAX> <shared-8X8D.DAX> <bank1> <bank2> <bank3> precedes the six arguments.\n";
            std::cout << "Optional --images <PIC.DAX> <SPRIT.DAX> also precedes the six resource arguments.\n";
            std::cout << "Optional --font <8X8D.DAX> <record-id> selects SSI-order monochrome text glyphs.\n";
            std::cout << "Optional --portraits <HEAD.DAX> <BODY.DAX> enables profile-based portraits with --images.\n";
            std::cout << "Optional --startup <TITLE.DAX> opens profile-selected titles and the startup menu.\n";
            std::cout << "Optional --audio <START.EXE> enables profile-selected speaker effects; F8 auditions one.\n";
            return argc == 1 ? 0 : 1;
        }
        const auto profile = goldbox::read_game_profile(argv[1]);
#ifdef GOLDBOX_RENDERER
        if (images) images->picture_period_ms = profile.picture_period_ms;
        std::optional<goldbox::formats::SpeakerClip> preview_audio;
        std::function<goldbox::formats::SpeakerClip(std::uint16_t)> sounds;
        if (audio_file) {
            if (!profile.speaker_audio) throw std::runtime_error("Profile has no speaker audio adapter");
            std::ifstream input(*audio_file,std::ios::binary | std::ios::ate);
            if (!input || input.tellg() < 0 || input.tellg() > 2*1024*1024)
                throw std::runtime_error("Invalid audio executable size");
            std::vector<std::uint8_t> executable(static_cast<std::size_t>(input.tellg()));
            input.seekg(0);
            if (!input.read(reinterpret_cast<char*>(executable.data()),executable.size()))
                throw std::runtime_error("Failed reading audio executable");
            const auto resident = goldbox::formats::unpack_exepack(executable);
            const auto& layout = *profile.speaker_audio;
            if (layout.segment_offset >= resident.size()) throw std::runtime_error("Audio segment is missing");
            const auto segment = std::span<const std::uint8_t>(resident).subspan(layout.segment_offset);
            preview_audio = goldbox::formats::decode_ssi_speaker(segment.first(std::min<std::size_t>(segment.size(),65536)),
                layout.effect_table,layout.preview_entry,layout.tick_divisor);
            auto data = std::make_shared<std::vector<std::uint8_t>>(segment.begin(),
                segment.begin()+std::min<std::size_t>(segment.size(),65536));
            sounds = [data,layout](std::uint16_t id) {
                if (id < 2 || id > layout.effect_count)
                    throw std::runtime_error("Unsupported SSI sound ID/control");
                return goldbox::formats::decode_ssi_speaker(*data,layout.effect_table,id-1,layout.tick_divisor);
            };
        }
        const auto startup_screens = startup_archive ?
            goldbox::load_startup_screens(*startup_archive,profile.startup_title_records) :
            std::vector<goldbox::formats::IndexedImage>{};
        if (portraits) {
            if (!images || !profile.portrait) throw std::runtime_error("Portraits require --images and a portrait profile");
            images->portraits = std::move(portraits);
        }
#endif
        if (!profile.event_dispatch || profile.archive_format != "dax-v1") throw std::runtime_error("Unsupported exploration profile");
        const auto& config = *profile.event_dispatch;
        const auto geo_id = record_id(argv[3]), ecl_id = record_id(argv[5]);
        const auto geo_archive = std::make_shared<goldbox::formats::DaxArchive>(goldbox::formats::DaxArchive::from_file(argv[2]));
        const auto raw_map = geo_archive->extract(geo_id);
        auto ecl = goldbox::formats::DaxArchive::from_file(argv[4]).extract(ecl_id);
        goldbox::ExplorationSession session;
        session.sound_call = profile.sound_call;
        session.character_rules = profile.character_rules;
        session.initial_random = profile.random;
        if (seed) {
            if (!profile.random) throw std::runtime_error("--seed requires a profile random generator");
            session.initial_random.emplace(*seed,profile.random->multiplier(),profile.random->increment());
        }
        session.state.random = session.initial_random;
        session.random_dialect = profile.random_dialect;

        session.party_memory = profile.party_memory;
#ifdef GOLDBOX_RENDERER
        session.wall_banks = initial_banks;
#endif
        session.map = goldbox::formats::decode_geo16_record(raw_map, geo_id);
        session.initial_map_record = geo_id;
        session.maps = goldbox::MapResources{
            [geo_archive](std::uint8_t id) { return goldbox::formats::extract_geo16_map(*geo_archive,id); },
            "dax-fnv1a64:" + std::to_string(checksum(geo_archive->source_bytes()))};
        session.ecl_record = std::move(ecl);
        session.dialect = {config.dialect, config.code_base, config.square_info_address, true, config.position_addresses};
        session.opcodes = goldbox::minimal_vm_opcodes(profile.image_dialect == "ssi-images-3-v1" ?
            goldbox::EclImageDialect::sprite3 : goldbox::EclImageDialect::disabled,
            profile.area_dialect == "ssi-area-map3-v1" ? goldbox::EclAreaDialect::map3 : goldbox::EclAreaDialect::disabled,
            profile.party_memory ? goldbox::EclPartyDialect::load_character : goldbox::EclPartyDialect::disabled);
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
        if (harness) {
            return run_harness(session, source_identity, save);
        }
#ifdef GOLDBOX_RENDERER
        if (window) return goldbox::run_exploration_window(session, source_identity, save,
            graphics ? &*graphics : nullptr, images ? &*images : nullptr,
            profile.color_backdrop ? &*profile.color_backdrop : nullptr,font,
            images && images->portraits ? &*profile.portrait : nullptr,wall_archives ? &*wall_archives : nullptr,
            startup_screens,preview_audio ? &*preview_audio : nullptr,sounds,profile.startup_title_period_ms);
#endif
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
    } catch (const ProtocolError& error) {
        if (!harness) {
            std::cerr << "Error: " << error.message << "\n";
            return 1;
        }
        try {
            emit_json(bj::object{{"type", "error"}, {"message", error.message}});
        } catch (...) {
            std::cerr << "Error: " << error.message << "\n";
        }
        return 1;
    } catch (const std::exception& error) {
        if (harness) {
            try {
                emit_json(bj::object{{"type", "error"}, {"message", error.what()}});
            } catch (...) {
                std::cerr << "Error: " << error.what() << "\n";
            }
        } else {
            std::cerr << "Error: " << error.what() << "\n";
        }
        return 1;
    }
}
