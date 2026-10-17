// SPDX-License-Identifier: GPL-3.0-or-later
#include "goldbox/exploration.hpp"
#include "json_support.hpp"

#include <stdexcept>

namespace goldbox {
namespace {
std::optional<std::uint8_t> area_record(std::array<std::uint8_t,3> values) {
    if (values == std::array<std::uint8_t,3>{0,0,0}) return 0;
    if (values[0] != 127 && values[0] != 255 &&
        (values[1] == 2 || values[1] == 127 || values[1] == 255)) return values[0];
    if (values[0] == 255 && values[1] == 2) return std::nullopt; // Keep current dungeon.
    throw std::runtime_error("Unsupported LOAD_AREA_MAP mode (dungeon GEO16 only)");
}
void require_maps(const std::optional<MapResources>& maps) {
    if (!maps || !maps->resolve || maps->source_identity.empty() || maps->source_identity.size() > 4096)
        throw std::runtime_error("Map resource resolver/identity unavailable");
}
formats::GeoMap resolve_map(const MapResources& maps, std::uint8_t id) {
    auto map = maps.resolve(id);
    if (map.dax_record_id != id) throw std::runtime_error("Map resolver returned the wrong record");
    return map;
}
class SessionHost final : public EclHost {
    ExplorationSession& session;
    EclHost& delegate;
public:
    SessionHost(ExplorationSession& value, EclHost& host) : session(value), delegate(host) {}
    void text(std::string_view value, bool clear) override { delegate.text(value,clear); }
    std::size_t menu(std::span<const std::string> choices, std::string_view prompt, bool vertical) override {
        return delegate.menu(choices,prompt,vertical);
    }
    void combat(EclState& state) override { delegate.combat(state); }
    void picture(std::uint8_t id) override { delegate.picture(id); }
    void sprite_start(EclSpriteRequest r) override { delegate.sprite_start(r); }
    void sprite_advance() override { delegate.sprite_advance(); }
    void clear_images() override { delegate.clear_images(); }
    void wall_banks(std::array<std::uint8_t,3> banks) override {
        delegate.wall_banks(banks);
        session.state.memory[session.dialect.square_info_address] = session.map.cell(session.row,session.column).raw_event;
    }
    void load_area(std::array<std::uint8_t,3> values) override { session.load_area(values); }
};
} // namespace

void ExplorationSession::load_area(std::array<std::uint8_t,3> values) {
    const auto record = area_record(values);
    require_maps(maps);
    auto next = record ? resolve_map(*maps,*record) : map;
    auto next_row = row, next_column = column;
    auto next_facing = facing;
    if (dialect.position_addresses) {
        const auto& addresses = *dialect.position_addresses;
        next_column = state.memory[addresses[0]];
        next_row = state.memory[addresses[1]];
        next_facing = static_cast<Direction>(state.memory[addresses[2]]);
    }
    (void)next.cell(next_row,next_column);
    if (static_cast<unsigned>(next_facing) > 3) throw std::runtime_error("Invalid map-load direction");
    // Lookup/validation failures publish no map transition. Prior VM writes
    // retain the existing partial-progress semantics.
    if (!initial_map_record) initial_map_record = map.dax_record_id;
    map = std::move(next);
    row = next_row; column = next_column; facing = next_facing;
    area = values;
}

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
    SessionHost session_host(*this,host);
    return execute_ecl(ecl_record, kind, dialect, opcodes, state, session_host, limits);
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
    auto object = bj::object{{"schemaVersion", 1}, {"sourceIdentity", source_identity},
        {"dialect", session.dialect.id}, {"row", session.row}, {"column", session.column},
        {"facing", static_cast<unsigned>(session.facing)}, {"memory", std::move(memory)}, {"strings", std::move(strings)}};
    if (session.character_rules) {
        object["party"] = bj::parse(serialize_party(session.party,*session.character_rules));
    } else if (!session.party.empty()) throw std::runtime_error("Party requires creation rules");
    if (session.wall_banks) {
        bj::array banks;
        for (const auto id : *session.wall_banks) banks.push_back(id);
        object["wallBanks"] = std::move(banks);
    }
    if (session.area) {
        require_maps(session.maps);
        const auto record = area_record(*session.area);
        if (record && *record != session.map.dax_record_id) throw std::runtime_error("Area/map record mismatch");
        bj::array values;
        for (const auto value : *session.area) values.push_back(value);
        object["area"] = std::move(values);
        object["mapRecord"] = session.map.dax_record_id;
        object["mapSource"] = session.maps->source_identity;
    }
    const auto json = bj::serialize(object);
    if (json.size() > 4 * 1024 * 1024) throw std::runtime_error("Save exceeds 4 MiB limit");
    return json;
}

void restore_exploration(ExplorationSession& session, std::string_view json, std::string_view source_identity) {
    namespace j = json_support;
    if (json.size() > 4 * 1024 * 1024) throw std::runtime_error("Save exceeds 4 MiB limit");
    const auto parsed = boost::json::parse(json);
    const auto& root = j::object(parsed, "exploration save");
    j::fields_only(root, {"schemaVersion", "sourceIdentity", "dialect", "row", "column", "facing", "memory", "strings", "wallBanks",
        "area", "mapRecord", "mapSource", "party"}, "save");
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
    std::optional<std::array<std::uint8_t,3>> banks;
    if (const auto* value = root.if_contains("wallBanks")) {
        const auto& a = j::array(*value,"wallBanks");
        if (a.size() != 3) throw std::runtime_error("Saved wall banks require three IDs");
        banks.emplace();
        for (std::size_t i = 0; i < 3; ++i) (*banks)[i] = static_cast<std::uint8_t>(bounded(a[i],255));
    }
    auto map = session.map;
    std::optional<std::array<std::uint8_t,3>> area;
    const auto* area_value = root.if_contains("area");
    const auto* record_value = root.if_contains("mapRecord");
    const auto* source_value = root.if_contains("mapSource");
    if (area_value || record_value || source_value) {
        if (!area_value || !record_value || !source_value) throw std::runtime_error("Incomplete saved map binding");
        require_maps(session.maps);
        if (j::string(*source_value,"mapSource") != session.maps->source_identity)
            throw std::runtime_error("Saved map resource source mismatch");
        const auto& values = j::array(*area_value,"area");
        if (values.size() != 3) throw std::runtime_error("Saved area requires three selectors");
        area.emplace();
        for (std::size_t i = 0; i < 3; ++i) (*area)[i] = static_cast<std::uint8_t>(bounded(values[i],255));
        const auto id = static_cast<std::uint8_t>(bounded(*record_value,255));
        const auto selected = area_record(*area);
        if (selected && *selected != id) throw std::runtime_error("Saved area/map record mismatch");
        map = resolve_map(*session.maps,id);
    } else if (session.initial_map_record && *session.initial_map_record != map.dax_record_id) {
        require_maps(session.maps);
        map = resolve_map(*session.maps,*session.initial_map_record);
    }
    Party party;
    if (const auto* value = root.if_contains("party")) {
        if (!session.character_rules) throw std::runtime_error("Saved party requires matching creation rules");
        party = read_party(boost::json::serialize(*value),*session.character_rules);
    }
    (void)map.cell(row,column);
    // Publish only after validating the entire save; failed loads change nothing.
    if (!session.initial_map_record) session.initial_map_record = session.map.dax_record_id;
    session.map = std::move(map);
    session.area = area;
    session.wall_banks = banks;
    session.state = std::move(state);
    session.party = std::move(party);
    session.row = static_cast<std::uint8_t>(row);
    session.column = static_cast<std::uint8_t>(column);
    session.facing = static_cast<Direction>(facing);
}
} // namespace goldbox
