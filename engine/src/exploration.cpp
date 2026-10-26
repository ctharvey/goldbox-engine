// SPDX-License-Identifier: GPL-3.0-or-later
#include "goldbox/exploration.hpp"
#include "json_support.hpp"

#include <algorithm>
#include <deque>
#include <stdexcept>

namespace goldbox {
namespace {
void validate_monsters(const MonsterSetup& setup, const ExplorationSession& session) {
    if (session.monster_dialect != EclMonsterDialect::setup_v1 || !session.monster_resources ||
        !session.monster_resources->validate || session.monster_resources->source_identity.empty() ||
        session.monster_resources->source_identity.size() > 4096 || setup.groups.size() > 63 ||
        setup.next_group != 8+setup.groups.size()) throw std::runtime_error("Invalid monster setup binding/state");
    unsigned total = 0;
    for (std::size_t i = 0; i < setup.groups.size(); ++i) {
        const auto& group = setup.groups[i];
        if (!group.count || group.group != 8+i || (total += group.count) > 63)
            throw std::runtime_error("Invalid prepared monster group");
        session.monster_resources->validate(group.program,group.resource);
    }
}
std::optional<std::uint8_t> area_record(std::array<std::uint8_t,3> values) {
    if (values == std::array<std::uint8_t,3>{0,0,0}) return 0;
    if (values == std::array<std::uint8_t,3>{255,255,127}) return std::nullopt; // No active dungeon.
    if (values[0] != 127 && values[0] != 255 &&
        (values[1] == 2 || values[1] == 127 || values[1] == 255)) return values[0];
    if (values[0] == 255 && values[1] == 2) return std::nullopt; // Keep current dungeon.
    throw std::runtime_error("Unsupported LOAD_AREA_MAP mode (dungeon GEO16 only): " +
        std::to_string(values[0]) + "," + std::to_string(values[1]) + "," + std::to_string(values[2]));
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
ProgramResource resolve_program(const std::optional<ProgramResources>& resources, std::uint8_t id,
    const formats::EclDispatchDialect& dialect, std::span<const formats::EclOpcodeSpec> opcodes) {
    if (!resources || !resources->resolve || resources->source_identity.empty() ||
        resources->source_identity.size() > 4096) throw std::runtime_error("Program resource resolver/identity unavailable");
    auto program = resources->resolve(id);
    if (program.record_id != id) throw std::runtime_error("Program resolver returned the wrong record");
    (void)formats::decode_ecl_program_entries(program.bytes,dialect,opcodes);
    return program;
}
class SessionHost final : public EclHost {
    ExplorationSession& session;
    EclHost& delegate;
public:
    SessionHost(ExplorationSession& value, EclHost& host) : session(value), delegate(host) {
        if (session.party_class_check) validate_party_class_check(*session.party_class_check);
        if (session.party_strength) {
            if (!session.character_rules) throw std::runtime_error("Party strength requires creation rules");
            validate_party_strength(*session.party_strength,*session.character_rules);
        }
        if (session.transfer_clear_ranges.size() > 32) throw std::runtime_error("Too many transfer clear ranges");
        for (std::size_t i = 0; i < session.transfer_clear_ranges.size(); ++i) {
            const auto [start,length] = session.transfer_clear_ranges[i];
            const auto end = std::size_t(start)+length;
            if (!length || end > session.state.memory.size()) throw std::runtime_error("Invalid transfer clear range");
            const auto contains = [&](std::uint16_t address) { return start <= address && address < end; };
            if (contains(session.dialect.square_info_address) ||
                (start < std::size_t(session.dialect.code_base)+session.ecl_record.size() &&
                    session.dialect.code_base < end) ||
                (session.party_memory && party_memory_intersects(*session.party_memory,start,length)) ||
                (session.position_advance_call && contains(session.position_advance_call->wall_type_address)) ||
                (session.sound_call && (contains(session.sound_call->selector_address) ||
                    contains(session.sound_call->selector_address+1))))
                throw std::runtime_error("Transfer clear range overlaps host memory");
            if (session.dialect.position_addresses)
                for (const auto address : *session.dialect.position_addresses)
                    if (contains(address)) throw std::runtime_error("Transfer clear range overlaps position memory");
            if (session.movement_addresses)
                for (const auto address : *session.movement_addresses)
                    if (contains(address)) throw std::runtime_error("Transfer clear range overlaps movement memory");
            for (std::size_t j = 0; j < i; ++j) {
                const auto& previous = session.transfer_clear_ranges[j];
                if (start < std::size_t(previous[0])+previous[1] && previous[0] < end)
                    throw std::runtime_error("Overlapping transfer clear ranges");
            }
        }
        if (session.position_advance_call) {
            const auto& call = *session.position_advance_call;
            if (!session.dialect.position_addresses || (session.position_refresh_call &&
                call.function == *session.position_refresh_call) ||
                (session.sound_call && call.function == session.sound_call->function))
                throw std::runtime_error("Invalid or conflicting position advance call");
            const auto address = call.wall_type_address;
            const auto& positions = *session.dialect.position_addresses;
            if (address == session.dialect.square_info_address ||
                std::find(positions.begin(),positions.end(),address) != positions.end() ||
                (session.movement_addresses && std::find(session.movement_addresses->begin(),
                    session.movement_addresses->end(),address) != session.movement_addresses->end()) ||
                (address >= session.dialect.code_base &&
                    std::size_t(address) < std::size_t(session.dialect.code_base)+session.ecl_record.size()) ||
                (session.party_memory && party_memory_intersects(*session.party_memory,address,1)) ||
                (session.sound_call && (address == session.sound_call->selector_address ||
                    address == std::size_t(session.sound_call->selector_address)+1)))
                throw std::runtime_error("Position advance wall register overlaps host memory");
        }
        if (session.movement_addresses) {
            if (!session.dialect.position_addresses) throw std::runtime_error("Movement requires position bindings");
            std::vector<std::uint16_t> used{session.dialect.square_info_address};
            const auto add = [&](std::uint16_t address) {
                if (std::find(used.begin(),used.end(),address) != used.end())
                    throw std::runtime_error("Conflicting movement context bindings");
                used.push_back(address);
            };
            for (const auto address : *session.dialect.position_addresses) add(address);
            for (const auto address : *session.movement_addresses) {
                add(address);
                if (address >= session.dialect.code_base &&
                    std::size_t(address) < std::size_t(session.dialect.code_base)+session.ecl_record.size())
                    throw std::runtime_error("Movement register overlaps ECL program");
                if (session.party_memory && party_memory_intersects(*session.party_memory,address,1))
                    throw std::runtime_error("Movement register overlaps party memory");
                if (session.sound_call && (address == session.sound_call->selector_address ||
                    address == std::size_t(session.sound_call->selector_address)+1))
                    throw std::runtime_error("Movement register overlaps sound selector");
            }
        }
        if (session.position_refresh_call && session.sound_call &&
            *session.position_refresh_call == session.sound_call->function)
            throw std::runtime_error("Conflicting native call bindings");
        if (session.party_memory) {
            if (!session.character_rules) throw std::runtime_error("Party memory requires creation rules");
            const auto& layout = *session.party_memory;
            validate_party_memory_layout(layout,*session.character_rules);
            if (layout.selected_address && (*layout.selected_address == session.dialect.square_info_address ||
                (session.dialect.position_addresses && std::find(session.dialect.position_addresses->begin(),
                    session.dialect.position_addresses->end(),*layout.selected_address) != session.dialect.position_addresses->end())))
                throw std::runtime_error("Selected character address overlaps event context");
            if (party_memory_intersects(layout,session.dialect.square_info_address,1))
                throw std::runtime_error("Party memory overlaps event context");
            if (session.dialect.position_addresses) for (const auto address : *session.dialect.position_addresses)
                if (party_memory_intersects(layout,address,1))
                    throw std::runtime_error("Party memory overlaps event position");
            const auto begin = std::size_t(session.dialect.code_base);
            const auto end = begin+session.ecl_record.size(); // Conservative program footprint including headers.
            if ((layout.record_base < end && std::size_t(layout.record_base)+layout.record_size > begin) ||
                (layout.count_address >= begin && layout.count_address < end) ||
                (layout.selected_address && *layout.selected_address >= begin && *layout.selected_address < end))
                throw std::runtime_error("Party memory overlaps ECL program");
        }
    }
    void load_character(std::uint8_t index) override {
        if (!session.party_memory) { delegate.load_character(index); return; }
        if (index >= session.party.size()) throw std::runtime_error("Character index outside current party");
        session.loaded_character = index;
    }
    void select_character(std::string_view prompt) override {
        if (!session.party_memory) { delegate.select_character(prompt); return; }
        const auto address = session.party_memory->selected_address;
        if (!address) throw std::runtime_error("WHO requires a selected character address");
        if (session.party.empty() || session.party.size() > 255)
            throw std::runtime_error("WHO requires a nonempty byte-indexed party");
        std::vector<std::string> choices;
        for (const auto& character : session.party) choices.push_back(character.name);
        const auto selected = delegate.menu(choices,prompt,true);
        if (selected >= session.party.size()) throw std::runtime_error("WHO host returned invalid character index");
        session.state.memory[*address] = static_cast<std::uint8_t>(selected);
        session.loaded_character = static_cast<std::uint8_t>(selected);
    }
    std::optional<std::uint8_t> memory_byte(std::uint16_t address) override {
        if (session.party_memory) {
            if (const auto value = party_memory_byte(*session.party_memory,*session.character_rules,
                session.party,session.loaded_character,address)) return value;
        }
        return delegate.memory_byte(address);
    }
    bool write_memory(std::uint16_t address, std::uint32_t value, std::size_t width) override {
        if (session.party_memory && party_memory_intersects(*session.party_memory,address,width))
            throw std::runtime_error("Script writes to party memory are unsupported");
        if (session.sound_call) {
            const auto selector = session.sound_call->selector_address;
            if (selector == 65535) throw std::runtime_error("Sound-call word outside memory");
            if (address == selector) {
                if (session.party_memory && party_memory_intersects(*session.party_memory,address,2))
                    throw std::runtime_error("Sound selector overlaps party memory");
                // A virtual native word register: even byte destinations store AX.
                session.state.memory[address] = static_cast<std::uint8_t>(value);
                session.state.memory[address+1] = static_cast<std::uint8_t>(value >> 8);
                return true;
            }
            if (address == selector+1 || (width == 2 && address+1 == selector))
                throw std::runtime_error("Partial sound selector writes unsupported");
        }
        return delegate.write_memory(address,value,width);
    }
    std::optional<std::string> string_at(std::uint16_t address) override {
        if (session.party_memory) {
            if (const auto value = party_memory_string(*session.party_memory,session.party,
                session.loaded_character,address)) return value;
        }
        return delegate.string_at(address);
    }
    bool write_string(std::uint16_t address, std::string_view value) override {
        if (session.party_memory && party_memory_intersects(*session.party_memory,address,1))
            throw std::runtime_error("Script writes to party strings are unsupported");
        return delegate.write_string(address,value);
    }
    void text(std::string_view value, bool clear) override { delegate.text(value,clear); }
    void delay() override { delegate.delay(); }
    std::size_t menu(std::span<const std::string> choices, std::string_view prompt, bool vertical) override {
        return delegate.menu(choices,prompt,vertical);
    }
    void combat(EclState& state) override { delegate.combat(state); }
    void clear_monsters() override {
        if (session.monster_dialect != EclMonsterDialect::setup_v1)
            throw std::runtime_error("Monster setup adapter unavailable");
        session.monster_setup = MonsterSetup{};
    }
    void load_monsters(EclMonsterRequest request) override {
        if (session.monster_dialect != EclMonsterDialect::setup_v1 || !session.monster_setup)
            throw std::runtime_error("Monster setup state unavailable; CLEAR_MON required");
        auto& setup = *session.monster_setup;
        unsigned total = 0;
        for (const auto& group : setup.groups) total += group.count;
        if (total >= 63) return; // Native cap skips resource loading and RNG consumption.
        if (!session.current_program_record || !session.monster_resources || !session.monster_resources->validate)
            throw std::runtime_error("Monster resource binding unavailable");
        if (!session.state.random) throw std::runtime_error("Monster initialization requires saved RNG state");
        session.monster_resources->validate(*session.current_program_record,request.resource);
        auto random = *session.state.random;
        const auto seed = random.state();
        for (unsigned i = 0; i < 5; ++i) (void)random.draw(6);
        const auto count = std::min(std::max(unsigned(request.count),1u),63-total);
        setup.groups.push_back({*session.current_program_record,request.resource,static_cast<std::uint8_t>(count),
            setup.next_group,request.selector,seed});
        ++setup.next_group;
        session.state.random = random;
    }
    std::uint8_t party_strength() override {
        if (!session.party_strength) throw std::runtime_error("Party strength adapter unavailable");
        return calculate_party_strength(session.party,*session.party_strength);
    }
    bool party_class_present() override {
        if (!session.party_class_check) throw std::runtime_error("Party class check adapter unavailable");
        return goldbox::party_class_present(session.party,*session.party_class_check);
    }
    std::array<std::uint8_t,2> party_movement() override { return delegate.party_movement(); }
    std::uint8_t encounter_distance() override {
        if (!session.encounter) throw std::runtime_error("Encounter adapter unavailable");
        const auto address = session.encounter->view_mode_address;
        if (memory_byte(address).value_or(session.state.memory[address]) == 0) return 2;
        if (!session.has_dungeon_map() || !session.dialect.position_addresses)
            throw std::runtime_error("Encounter visibility requires an active dungeon and position binding");
        const auto& positions = *session.dialect.position_addresses;
        int x = session.state.memory[positions[0]], y = session.state.memory[positions[1]];
        const auto direction = session.state.memory[positions[2]];
        if (direction > 3) throw std::runtime_error("Invalid encounter direction");
        const int dx[]{0,1,0,-1}, dy[]{-1,0,1,0};
        for (std::uint8_t distance = 0; distance < 2; ++distance) {
            if (x < 0 || x >= 16 || y < 0 || y >= 16)
                throw std::runtime_error("Encounter visibility across map boundaries is unverified");
            const auto& cell = session.map.cell(y,x);
            const std::uint8_t walls[]{cell.north,cell.east,cell.south,cell.west};
            // Native visibility tests the current edge's wall graphic, including open doors.
            if (walls[direction] != 0) return distance;
            x += dx[direction]; y += dy[direction];
        }
        return 2;
    }
    void native_call(std::uint16_t function) override {
        if (session.position_advance_call && function == session.position_advance_call->function) {
            const auto& addresses = *session.dialect.position_addresses;
            const auto x = session.state.memory[addresses[0]], y = session.state.memory[addresses[1]];
            const auto dir = session.state.memory[addresses[2]];
            (void)session.map.cell(y,x);
            if (dir > 3) throw std::runtime_error("Invalid forced-step direction");
            const int dx[]{0,1,0,-1}, dy[]{-1,0,1,0};
            const auto column = static_cast<std::uint8_t>((int(x)+dx[dir]+16)%16);
            const auto row = static_cast<std::uint8_t>((int(y)+dy[dir]+16)%16);
            const auto& cell = session.map.cell(row,column);
            session.column = column; session.row = row; session.facing = static_cast<Direction>(dir);
            session.state.memory[addresses[0]] = column; session.state.memory[addresses[1]] = row;
            session.state.memory[session.dialect.square_info_address] = session.has_dungeon_map() ? cell.raw_event : 0;
            const std::uint8_t walls[]{cell.north,cell.east,cell.south,cell.west};
            session.state.memory[session.position_advance_call->wall_type_address] = session.has_dungeon_map() ? walls[dir] : 0;
            return;
        }
        if (session.position_refresh_call && function == *session.position_refresh_call) {
            if (!session.dialect.position_addresses) throw std::runtime_error("Position refresh requires position bindings");
            const auto& addresses = *session.dialect.position_addresses;
            const auto column = session.state.memory[addresses[0]], row = session.state.memory[addresses[1]];
            const auto direction = session.state.memory[addresses[2]];
            const auto selector = session.map.cell(row,column).raw_event;
            if (direction > 3) throw std::runtime_error("Invalid refreshed direction");
            session.row = row; session.column = column; session.facing = static_cast<Direction>(direction);
            session.state.memory[session.dialect.square_info_address] = session.has_dungeon_map() ? selector : 0;
            return;
        }
        if (!session.sound_call || function != session.sound_call->function) {
            delegate.native_call(function);
            return;
        }
        const auto read = [&](std::uint16_t address) {
            if (address == 65535) throw std::runtime_error("Sound-call word outside memory");
            return std::uint16_t(session.state.memory[address] | (session.state.memory[address+1] << 8));
        };
        const auto selector = read(session.sound_call->selector_address);
        for (const auto& route : session.sound_call->cases)
            if (selector == route.selector) { delegate.sound(route.sound); return; }
        // Verified dialect behavior: other selector values do not request a sound.
    }
    void picture(std::uint8_t id) override { delegate.picture(id); }
    void sprite_start(EclSpriteRequest r) override { delegate.sprite_start(r); }
    void sprite_advance() override { delegate.sprite_advance(); }
    void clear_images() override { delegate.clear_images(); }
    void wall_banks(std::array<std::uint8_t,3> banks) override {
        delegate.wall_banks(banks);
        session.state.memory[session.dialect.square_info_address] = session.has_dungeon_map() ?
            session.map.cell(session.row,session.column).raw_event : 0;
    }
    void load_area(std::array<std::uint8_t,3> values) override {
        session.load_area(values);
        if (!session.has_dungeon_map()) {
            session.state.memory[session.dialect.square_info_address] = 0;
            if (session.position_advance_call) session.state.memory[session.position_advance_call->wall_type_address] = 0;
            delegate.clear_images();
        }
    }
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
    if (!has_dungeon_map()) return false;
    const auto& cell = map.cell(row, column);
    const auto index = static_cast<std::size_t>(direction);
    if (index > 3) throw std::runtime_error("Invalid exploration direction");
    facing = direction;
    if (cell.blocks_movement(index)) return false;
    const int next_row = row + (direction == Direction::north ? -1 : direction == Direction::south ? 1 : 0);
    const int next_column = column + (direction == Direction::west ? -1 : direction == Direction::east ? 1 : 0);
    if (next_row < 0 || next_row >= 16 || next_column < 0 || next_column >= 16) return false;
    const auto& next = map.cell(static_cast<std::size_t>(next_row), static_cast<std::size_t>(next_column));
    if (next.blocks_movement((index+2)%4)) return false;
    row = static_cast<std::uint8_t>(next_row);
    column = static_cast<std::uint8_t>(next_column);
    return true;
}

ExplorationMove ExplorationSession::move(Direction direction, EclHost& host, EclLimits limits) {
    if (!movement_addresses) {
        if (!move(direction)) return {};
        return {true,trigger(formats::EclEventKind::move,host,limits)};
    }
    const auto previous_row = row, previous_column = column, previous_map = map.dax_record_id;
    EclExecution result;
    try {
        (void)SessionHost(*this,host); // Validate before publishing host registers.
        if (!has_dungeon_map()) throw std::runtime_error("Dungeon movement requires an active map");
        const auto index = static_cast<std::size_t>(direction);
        if (index > 3) throw std::runtime_error("Invalid exploration direction");
        const auto& bindings = *movement_addresses;
        const auto& source = map.cell(row,column);
        const std::uint8_t walls[]{source.north,source.east,source.south,source.west};
        const int dy[]{-1,0,1,0}, dx[]{0,1,0,-1};
        const int y = row+dy[index], x = column+dx[index];
        facing = direction;
        state.memory[bindings[0]] = 0;
        state.memory[bindings[1]] = (walls[index] == 0 || source.door_state(index) != 0) &&
            (x < 0 || x > 15 || y < 0 || y > 15);
        result = trigger(formats::EclEventKind::move,host,limits);
        if (result.exited && !result.program_transfers) {
            if (!has_dungeon_map()) throw std::runtime_error("Movement continuation requires an active map");
            // MOVE may reposition or load a map. Use its resulting source and facing.
            const auto heading = static_cast<std::size_t>(facing);
            state.memory[bindings[2]] = column; state.memory[bindings[3]] = row;
            if (state.memory[bindings[0]] < 255 && !map.cell(row,column).blocks_movement(heading)) {
                column = static_cast<std::uint8_t>((int(column)+dx[heading]+16)%16);
                row = static_cast<std::uint8_t>((int(row)+dy[heading]+16)%16);
            } else if (state.memory[bindings[0]] == 255) state.memory[bindings[0]] = 0;
            const auto& positions = *dialect.position_addresses;
            state.memory[positions[0]] = column; state.memory[positions[1]] = row;
            state.memory[positions[2]] = static_cast<std::uint8_t>(facing);
            state.memory[dialect.square_info_address] = map.cell(row,column).raw_event;
            if (position_advance_call) {
                const auto& cell = map.cell(row,column);
                const std::uint8_t walls[]{cell.north,cell.east,cell.south,cell.west};
                state.memory[position_advance_call->wall_type_address] = walls[static_cast<std::size_t>(facing)];
            }
            if (result.steps >= limits.steps) throw std::runtime_error("Movement ECL step limit exceeded");
            auto remaining = limits; remaining.steps -= result.steps;
            auto search = trigger(formats::EclEventKind::search,host,remaining);
            result.steps += search.steps; result.bytecode_offset = search.bytecode_offset;
            result.program_transfers += search.program_transfers;
            result.dispatches.insert(result.dispatches.end(),search.dispatches.begin(),search.dispatches.end());
            result.exited = search.exited; result.diagnostic = std::move(search.diagnostic);
        }
    } catch (const std::exception& error) {
        result.exited = false; result.diagnostic = error.what();
    }
    return {row != previous_row || column != previous_column || map.dax_record_id != previous_map ||
        result.program_transfers != 0,std::move(result)};
}

EclExecution ExplorationSession::begin_campaign(EclHost& host, EclLimits limits) {
    if (!campaign_start) throw std::runtime_error("Campaign startup adapter unavailable");
    if (party.empty()) throw std::runtime_error("Campaign startup requires a party");
    require_maps(maps);
    auto next_map = resolve_map(*maps,(*campaign_start)[0]);
    auto next_program = resolve_program(programs,(*campaign_start)[1],dialect,opcodes);
    // Resolve and validate before publishing. Execution then has the same explicit
    // partial-progress semantics as trigger(); callers may restore their snapshot.
    auto candidate = *this;
    candidate.map = std::move(next_map); candidate.ecl_record = std::move(next_program.bytes);
    candidate.initial_map_record = candidate.map.dax_record_id;
    candidate.current_program_record = next_program.record_id;
    candidate.state = {}; candidate.state.random = state.random;
    candidate.row = candidate.column = candidate.loaded_character = 0;
    candidate.facing = Direction::north; candidate.area.reset(); candidate.wall_banks.reset();
    candidate.monster_setup.reset();
    (void)SessionHost(candidate,host);
    host.program_changed(*candidate.current_program_record);
    *this = std::move(candidate);
    return trigger(formats::EclEventKind::initialize,host,limits);
}

EclExecution ExplorationSession::trigger(formats::EclEventKind kind, EclHost& host, EclLimits limits) {
    // The raw byte is the host context. Scripts perform their own masks.
    state.memory[dialect.square_info_address] = has_dungeon_map() ? map.cell(row, column).raw_event : 0;
    if (dialect.position_addresses) {
        const auto& addresses = *dialect.position_addresses;
        state.memory[addresses[0]] = column;
        state.memory[addresses[1]] = row;
        state.memory[addresses[2]] = static_cast<std::uint8_t>(facing);
    }
    EclExecution result;
    try {
        std::deque<formats::EclEventKind> events{kind};
        std::size_t transfers = 0;
        while (!events.empty()) {
            if (result.steps >= limits.steps) throw std::runtime_error("Campaign ECL step limit exceeded");
            SessionHost session_host(*this,host);
            auto remaining = limits; remaining.steps -= result.steps;
            auto run = execute_ecl(ecl_record,events.front(),dialect,opcodes,state,session_host,remaining,
                random_dialect,arithmetic_dialect,surprise,encounter,monster_dialect);
            result.steps += run.steps; result.bytecode_offset = run.bytecode_offset;
            result.dispatches.insert(result.dispatches.end(),run.dispatches.begin(),run.dispatches.end());
            if (!run.exited) { result.diagnostic = std::move(run.diagnostic); return result; }
            if (movement_addresses) {
                const auto& addresses = *dialect.position_addresses;
                const auto x = state.memory[addresses[0]], y = state.memory[addresses[1]], dir = state.memory[addresses[2]];
                (void)map.cell(y,x);
                if (dir > 3) throw std::runtime_error("Invalid event direction");
                column = x; row = y; facing = static_cast<Direction>(dir);
                state.memory[dialect.square_info_address] = has_dungeon_map() ? map.cell(row,column).raw_event : 0;
            }
            events.pop_front();
            if (run.next_program) {
                if (++transfers > 32) throw std::runtime_error("Campaign program transfer limit exceeded");
                auto program = resolve_program(programs,*run.next_program,dialect,opcodes);
                auto candidate = *this; candidate.ecl_record = program.bytes;
                (void)SessionHost(candidate,host); // Validate bindings before publishing the new program.
                host.program_changed(program.record_id);
                ecl_record = std::move(program.bytes); current_program_record = program.record_id;
                ++result.program_transfers;
                for (const auto& [start,length] : transfer_clear_ranges)
                    std::fill_n(state.memory.begin()+start,length,0);
                if (movement_addresses) {
                    state.memory[(*movement_addresses)[0]] = 0;
                    state.memory[(*movement_addresses)[1]] = 0;
                }
                events = {formats::EclEventKind::initialize,formats::EclEventKind::move,formats::EclEventKind::search};
            }
        }
        result.exited = true;
        return result;
    } catch (const std::exception& error) {
        result.exited = false; result.diagnostic = error.what(); return result;
    }
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
    if (session.monster_setup) {
        validate_monsters(*session.monster_setup,session);
        bj::array groups;
        for (const auto& group : session.monster_setup->groups) groups.push_back(bj::object{
            {"program",group.program},{"resource",group.resource},{"count",group.count},{"group",group.group},
            {"selector",group.selector},{"templateSeed",group.template_seed}});
        object["monsterSetup"] = bj::object{{"dialect","ssi-monster-setup-v1"},
            {"sourceIdentity",session.monster_resources->source_identity},{"nextGroup",session.monster_setup->next_group},
            {"groups",std::move(groups)}};
    }
    if (session.state.random) {
        const auto& rng = *session.state.random;
        if (session.random_dialect != EclRandomDialect::disabled &&
            session.random_dialect != EclRandomDialect::byte_upper) throw std::runtime_error("Unknown RNG dialect");
        if (!session.initial_random || rng.multiplier() != session.initial_random->multiplier() ||
            rng.increment() != session.initial_random->increment()) throw std::runtime_error("RNG profile mismatch");
        object["random"] = bj::object{{"algorithm","lcg32-high16-mod-v1"},{"multiplier",rng.multiplier()},
            {"increment",rng.increment()},{"state",rng.state()},
            {"eclDialect",session.random_dialect == EclRandomDialect::byte_upper ? "ssi-byte-upper-v1" : "disabled"}};
    }
    if (session.character_rules) {
        object["party"] = bj::parse(serialize_party(session.party,*session.character_rules));
    } else if (!session.party.empty()) throw std::runtime_error("Party requires creation rules");
    if (session.party_memory) {
        if ((session.party.empty() && session.loaded_character != 0) ||
            (!session.party.empty() && session.loaded_character >= session.party.size()))
            throw std::runtime_error("Invalid loaded character in save");
        object["loadedCharacter"] = session.loaded_character;
    } else if (session.loaded_character) throw std::runtime_error("Loaded character requires party memory adapter");
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
    if (session.programs || session.current_program_record) {
        if (!session.current_program_record) throw std::runtime_error("Missing current program binding");
        (void)resolve_program(session.programs,*session.current_program_record,session.dialect,session.opcodes);
        object["programRecord"] = *session.current_program_record;
        object["programSource"] = session.programs->source_identity;
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
        "area", "mapRecord", "mapSource", "party", "loadedCharacter", "random", "programRecord", "programSource", "monsterSetup"}, "save");
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
    std::optional<MonsterSetup> monsters;
    if (const auto* value = root.if_contains("monsterSetup")) {
        const auto& object = j::object(*value,"monsterSetup");
        j::fields_only(object,{"dialect","sourceIdentity","nextGroup","groups"},"monsterSetup");
        if (!session.monster_resources || j::string(j::field(object,"dialect","monsterSetup"),"monster dialect") !=
            "ssi-monster-setup-v1" || j::string(j::field(object,"sourceIdentity","monsterSetup"),"monster source") !=
            session.monster_resources->source_identity) throw std::runtime_error("Saved monster resource/dialect mismatch");
        monsters.emplace();
        monsters->next_group = static_cast<std::uint8_t>(bounded(j::field(object,"nextGroup","monsterSetup"),255));
        const auto& groups = j::array(j::field(object,"groups","monsterSetup"),"monster groups");
        if (groups.size() > 63) throw std::runtime_error("Too many prepared monster groups");
        for (const auto& value : groups) {
            const auto& group = j::object(value,"monster group");
            j::fields_only(group,{"program","resource","count","group","selector","templateSeed"},"monster group");
            const auto byte = [&](const char* key) {
                return static_cast<std::uint8_t>(bounded(j::field(group,key,"monster group"),255));
            };
            monsters->groups.push_back({byte("program"),byte("resource"),byte("count"),byte("group"),byte("selector"),
                static_cast<std::uint32_t>(bounded(j::field(group,"templateSeed","monster group"),UINT32_MAX))});
        }
        validate_monsters(*monsters,session);
    }
    const auto column = bounded(j::field(root, "column", "save"), 15);
    const auto facing = bounded(j::field(root, "facing", "save"), 3);
    const auto& memory = j::array(j::field(root, "memory", "save"), "save memory");
    if (memory.size() != session.state.memory.size()) throw std::runtime_error("Save memory size mismatch");
    EclState state;
    if (const auto* value = root.if_contains("random")) {
        const auto& object = j::object(*value,"saved RNG");
        j::fields_only(object,{"algorithm","multiplier","increment","state","eclDialect"},"saved RNG");
        if (!session.initial_random ||
            j::string(j::field(object,"algorithm","saved RNG"),"RNG algorithm") != "lcg32-high16-mod-v1" ||
            bounded(j::field(object,"multiplier","saved RNG"),UINT32_MAX) != session.initial_random->multiplier() ||
            bounded(j::field(object,"increment","saved RNG"),UINT32_MAX) != session.initial_random->increment() ||
            j::string(j::field(object,"eclDialect","saved RNG"),"RNG dialect") !=
                (session.random_dialect == EclRandomDialect::byte_upper ? "ssi-byte-upper-v1" : "disabled"))
            throw std::runtime_error("Saved RNG profile mismatch");
        state.random.emplace(static_cast<std::uint32_t>(bounded(j::field(object,"state","saved RNG"),UINT32_MAX)),
            session.initial_random->multiplier(),session.initial_random->increment());
    } // Legacy saves remain readable, with unknown rather than invented RNG state.
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
    auto program = session.ecl_record;
    auto program_id = session.current_program_record;
    const auto* program_value = root.if_contains("programRecord");
    const auto* program_source = root.if_contains("programSource");
    if (program_value || program_source || session.programs) {
        if (!program_value || !program_source || !session.programs)
            throw std::runtime_error("Incomplete saved program binding");
        if (j::string(*program_source,"programSource") != session.programs->source_identity)
            throw std::runtime_error("Saved program resource source mismatch");
        program_id = static_cast<std::uint8_t>(bounded(*program_value,255));
        program = resolve_program(session.programs,*program_id,session.dialect,session.opcodes).bytes;
        auto candidate = session; candidate.ecl_record = program;
        struct ValidationHost final : EclHost { void text(std::string_view,bool) override {} } validation;
        (void)SessionHost(candidate,validation);
    }
    Party party;
    if (const auto* value = root.if_contains("party")) {
        if (!session.character_rules) throw std::runtime_error("Saved party requires matching creation rules");
        party = read_party(boost::json::serialize(*value),*session.character_rules);
    }
    std::uint8_t loaded_character = 0;
    if (const auto* value = root.if_contains("loadedCharacter")) {
        if (!session.party_memory) throw std::runtime_error("Saved loaded character requires party memory adapter");
        loaded_character = static_cast<std::uint8_t>(bounded(*value,255));
        if ((party.empty() && loaded_character != 0) || (!party.empty() && loaded_character >= party.size()))
            throw std::runtime_error("Saved loaded character outside current party");
    }
    (void)map.cell(row,column);
    // Publish only after validating the entire save; failed loads change nothing.
    if (!session.initial_map_record) session.initial_map_record = session.map.dax_record_id;
    session.map = std::move(map);
    session.ecl_record = std::move(program);
    session.current_program_record = program_id;
    session.area = area;
    session.wall_banks = banks;
    session.state = std::move(state);
    session.party = std::move(party);
    session.loaded_character = loaded_character;
    session.monster_setup = std::move(monsters);
    session.row = static_cast<std::uint8_t>(row);
    session.column = static_cast<std::uint8_t>(column);
    session.facing = static_cast<Direction>(facing);
}
} // namespace goldbox
