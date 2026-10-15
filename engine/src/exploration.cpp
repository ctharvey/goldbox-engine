// SPDX-License-Identifier: GPL-3.0-or-later
#include "goldbox/exploration.hpp"
#include "json_support.hpp"

#include <stdexcept>

namespace goldbox {
bool ExplorationSession::move(Direction direction) {
    const auto& cell = map.cell(row, column);
    const std::uint8_t walls[]{cell.north, cell.east, cell.south, cell.west};
    const auto index = static_cast<std::size_t>(direction);
    if (index > 3) throw std::runtime_error("Invalid exploration direction");
    facing = direction;
    if (walls[index] != 0) return false;
    const int next_row = row + (direction == Direction::north ? -1 : direction == Direction::south ? 1 : 0);
    const int next_column = column + (direction == Direction::west ? -1 : direction == Direction::east ? 1 : 0);
    if (next_row < 0 || next_row >= 16 || next_column < 0 || next_column >= 16) return false;
    const auto& next = map.cell(static_cast<std::size_t>(next_row), static_cast<std::size_t>(next_column));
    const std::uint8_t opposite[]{next.south, next.west, next.north, next.east};
    if (opposite[index] != 0) return false;
    row = static_cast<std::uint8_t>(next_row);
    column = static_cast<std::uint8_t>(next_column);
    return true;
}

EclExecution ExplorationSession::trigger(formats::EclEventKind kind, EclHost& host, EclLimits limits) {
    // The raw byte is the host context. Scripts perform their own masks.
    state.memory[dialect.square_info_address] = map.cell(row, column).raw_event;
    if (dialect.position_addresses) {
        const auto& addresses = *dialect.position_addresses;
        state.memory[addresses[0]] = column;
        state.memory[addresses[1]] = row;
        state.memory[addresses[2]] = static_cast<std::uint8_t>(facing);
    }
    return execute_ecl(ecl_record, kind, dialect, opcodes, state, host, limits);
}

std::string serialize_exploration(const ExplorationSession& session, std::string_view source_identity) {
    namespace bj = boost::json;
    if (source_identity.empty() || source_identity.size() > 4096) throw std::runtime_error("Invalid save source identity");
    (void)session.map.cell(session.row, session.column);
    if (static_cast<unsigned>(session.facing) > 3) throw std::runtime_error("Invalid save direction");
    bj::array memory;
    for (const auto value : session.state.memory) memory.push_back(value);
    bj::array strings;
    for (const auto& [address, text] : session.state.strings) strings.push_back(bj::object{{"address", address}, {"text", text}});
    const auto json = bj::serialize(bj::object{{"schemaVersion", 1}, {"sourceIdentity", source_identity},
        {"dialect", session.dialect.id}, {"row", session.row}, {"column", session.column},
        {"facing", static_cast<unsigned>(session.facing)}, {"memory", std::move(memory)}, {"strings", std::move(strings)}});
    if (json.size() > 4 * 1024 * 1024) throw std::runtime_error("Save exceeds 4 MiB limit");
    return json;
}

void restore_exploration(ExplorationSession& session, std::string_view json, std::string_view source_identity) {
    namespace j = json_support;
    if (json.size() > 4 * 1024 * 1024) throw std::runtime_error("Save exceeds 4 MiB limit");
    const auto parsed = boost::json::parse(json);
    const auto& root = j::object(parsed, "exploration save");
    j::fields_only(root, {"schemaVersion", "sourceIdentity", "dialect", "row", "column", "facing", "memory", "strings"}, "save");
    if (j::integer(j::field(root, "schemaVersion", "save"), "schemaVersion") != 1 ||
        j::string(j::field(root, "sourceIdentity", "save"), "sourceIdentity") != source_identity ||
        j::string(j::field(root, "dialect", "save"), "dialect") != session.dialect.id) {
        throw std::runtime_error("Save schema/source/dialect mismatch");
    }
    const auto bounded = [&](const boost::json::value& value, std::int64_t maximum) {
        const auto number = j::integer(value, "save value");
        if (number < 0 || number > maximum) throw std::runtime_error("Save value out of range");
        return number;
    };
    const auto row = bounded(j::field(root, "row", "save"), 15);
    const auto column = bounded(j::field(root, "column", "save"), 15);
    const auto facing = bounded(j::field(root, "facing", "save"), 3);
    const auto& memory = j::array(j::field(root, "memory", "save"), "save memory");
    if (memory.size() != session.state.memory.size()) throw std::runtime_error("Save memory size mismatch");
    EclState state;
    for (std::size_t i = 0; i < memory.size(); ++i) state.memory[i] = static_cast<std::uint8_t>(bounded(memory[i], 255));
    for (const auto& value : j::array(j::field(root, "strings", "save"), "save strings")) {
        const auto& item = j::object(value, "saved string");
        j::fields_only(item, {"address", "text"}, "saved string");
        const auto address = static_cast<std::uint16_t>(bounded(j::field(item, "address", "saved string"), 65535));
        const auto text = j::string(j::field(item, "text", "saved string"), "saved text");
        if (!state.strings.emplace(address, text).second) throw std::runtime_error("Duplicate saved string address");
    }
    // Publish only after validating the entire save; failed loads change nothing.
    session.state = std::move(state);
    session.row = static_cast<std::uint8_t>(row);
    session.column = static_cast<std::uint8_t>(column);
    session.facing = static_cast<Direction>(facing);
}
} // namespace goldbox
