// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Goldbox Engine contributors
// Independently implemented bounded runtime; upstream SSI Engine is a
// behavior reference only. See attribution/sources.json.
#include "goldbox/ecl_vm.hpp"

#include <algorithm>

namespace goldbox {
using namespace formats;
namespace {
void require(bool condition, const char* diagnostic) {
    if (!condition) throw EclDecodeError(diagnostic);
}

std::uint32_t number(const EclOperand& operand, const EclState& state, EclHost& host,
    std::span<const std::uint8_t> code = {}, std::uint16_t code_base = 0) {
    if (operand.type == 0 || operand.type == 2 || operand.type == 4) return operand.value;
    require(operand.type == 1 || operand.type == 3 || operand.type == 5, "Expected numeric ECL operand");
    require(operand.value < state.memory.size(), "ECL memory read out of bounds");
    const auto byte = [&](std::uint32_t address) {
        if (address >= code_base && address - code_base < code.size()) return code[address - code_base];
        if (const auto value = host.memory_byte(static_cast<std::uint16_t>(address))) return *value;
        return state.memory[address];
    };
    const auto low = byte(operand.value);
    if (operand.type != 3) return low;
    require(operand.value + 1 < state.memory.size(), "ECL word read out of bounds");
    return low | (static_cast<std::uint32_t>(byte(operand.value + 1)) << 8);
}

void destination(const EclOperand& operand, const EclState& state) {
    require(operand.type == 1 || operand.type == 3 || operand.type == 5, "Invalid numeric ECL destination");
    require(operand.value < state.memory.size() && (operand.type != 3 || operand.value + 1 < state.memory.size()),
        "ECL memory write out of bounds");
}

void write(const EclOperand& operand, std::uint32_t value, EclState& state) {
    destination(operand, state);
    state.memory[operand.value] = static_cast<std::uint8_t>(value);
    if (operand.type == 3) state.memory[operand.value + 1] = static_cast<std::uint8_t>(value >> 8);
}

std::string string(const EclOperand& operand, const EclState& state, EclHost& host) {
    if (operand.type == 0x80) return operand.text;
    require(operand.type == 0x81, "Expected text ECL operand");
    if (const auto value = host.string_at(static_cast<std::uint16_t>(operand.value))) return *value;
    const auto found = state.strings.find(static_cast<std::uint16_t>(operand.value));
    require(found != state.strings.end(), "Unresolved ECL string address");
    return found->second;
}
} // namespace

std::size_t EclHost::menu(std::span<const std::string>, std::string_view, bool) {
    throw EclDecodeError("Menu host unavailable");
}
void EclHost::combat(EclState&) { throw EclDecodeError("Combat host unavailable"); }
void EclHost::picture(std::uint8_t) { throw EclDecodeError("Picture host unavailable"); }
void EclHost::sprite_start(EclSpriteRequest) { throw EclDecodeError("Sprite host unavailable"); }
void EclHost::sprite_advance() { throw EclDecodeError("Sprite host unavailable"); }
void EclHost::clear_images() { throw EclDecodeError("Image host unavailable"); }
void EclHost::wall_banks(std::array<std::uint8_t,3>) { throw EclDecodeError("Wall graphics host unavailable"); }
void EclHost::load_area(std::array<std::uint8_t,3>) { throw EclDecodeError("Map host unavailable"); }
void EclHost::load_character(std::uint8_t) { throw EclDecodeError("Party host unavailable"); }
void EclHost::select_character(std::string_view) { throw EclDecodeError("Party selection host unavailable"); }
std::optional<std::uint8_t> EclHost::memory_byte(std::uint16_t) { return std::nullopt; }
bool EclHost::write_memory(std::uint16_t, std::uint32_t, std::size_t) { return false; }
std::optional<std::string> EclHost::string_at(std::uint16_t) { return std::nullopt; }
bool EclHost::write_string(std::uint16_t, std::string_view) { return false; }

std::vector<EclOpcodeSpec> minimal_vm_opcodes(EclImageDialect images, EclAreaDialect area, EclPartyDialect party) {
    require(images == EclImageDialect::disabled || images == EclImageDialect::sprite3,"Unknown image dialect");
    require(area == EclAreaDialect::disabled || area == EclAreaDialect::map3,"Unknown area dialect");
    require(party == EclPartyDialect::disabled || party == EclPartyDialect::load_character,"Unknown party dialect");
    auto result = routing_ecl_opcodes();
    result.push_back({0x15, "MENU_VERTICAL", 3, 2});
    result.push_back({0x2b, "MENU_HORIZONTAL", 2, 1});
    result.push_back({0x24, "COMBAT", 0});
    result.push_back({0x2a, "COPY_MEM", 3});
    if (images == EclImageDialect::sprite3) {
        result.push_back({0x0c,"SPRITE_START3",3});
        result.push_back({0x31,"SPRITE_OFF",0});
        result.push_back({0x37,"LOAD_AREA_DECO",3});
    }
    if (area == EclAreaDialect::map3) result.push_back({0x21,"LOAD_AREA_MAP",3});
    if (party == EclPartyDialect::disabled)
        std::erase_if(result,[](const auto& op) { return op.opcode == 0x0a; });
    else result.push_back({0x39,"WHO",1});
    return result;
}

EclExecution execute_ecl(std::span<const std::uint8_t> record, EclEventKind kind,
    const EclDispatchDialect& dialect, std::span<const EclOpcodeSpec> opcodes,
    EclState& state, EclHost& host, EclLimits limits) {
    EclExecution result;
    try {
        const auto image_spec = std::find_if(opcodes.begin(),opcodes.end(),[](const auto& op) { return op.opcode == 0x0c; });
        const bool images_enabled = image_spec != opcodes.end();
        const auto party_spec = std::find_if(opcodes.begin(),opcodes.end(),[](const auto& op) { return op.opcode == 0x0a; });
        if (party_spec != opcodes.end()) require(party_spec->operand_count == 1 && party_spec->variable_count_operand == -1 &&
            std::count_if(opcodes.begin(),opcodes.end(),[](const auto& op) { return op.opcode == 0x0a; }) == 1,
            "Conflicting VM party opcode definition");
        const auto who_spec = std::find_if(opcodes.begin(),opcodes.end(),[](const auto& op) { return op.opcode == 0x39; });
        if (who_spec != opcodes.end()) require(who_spec->operand_count == 1 && who_spec->variable_count_operand == -1 &&
            std::count_if(opcodes.begin(),opcodes.end(),[](const auto& op) { return op.opcode == 0x39; }) == 1,
            "Conflicting VM WHO opcode definition");
        const auto area_spec = std::find_if(opcodes.begin(),opcodes.end(),[](const auto& op) { return op.opcode == 0x21; });
        if (area_spec != opcodes.end()) require(area_spec->operand_count == 3 && area_spec->variable_count_operand == -1 &&
            std::count_if(opcodes.begin(),opcodes.end(),[](const auto& op) { return op.opcode == 0x21; }) == 1,
            "Conflicting VM area opcode definition");
        if (images_enabled) require(image_spec->operand_count == 3 && image_spec->variable_count_operand == -1 &&
            std::count_if(opcodes.begin(),opcodes.end(),[](const auto& op) { return op.opcode == 0x0c; }) == 1,
            "Conflicting VM opcode definition");
        require(limits.steps > 0 && limits.stack_depth > 0, "ECL execution limits must be positive");
        const auto entries = decode_ecl_program_entries(record, dialect, opcodes);
        const auto code = record.subspan(entries.record_prefix_size);
        const auto numeric = [&](const EclOperand& operand) { return number(operand, state, host, code, dialect.code_base); };
        const auto store = [&](const EclOperand& operand, std::uint32_t value) {
            destination(operand, state);
            const auto end = operand.value + (operand.type == 3 ? 2u : 1u);
            require(end <= dialect.code_base || operand.value >= dialect.code_base + code.size(),
                "Self-modifying ECL program memory is unsupported");
            if (!host.write_memory(static_cast<std::uint16_t>(operand.value),value,operand.type == 3 ? 2 : 1))
                write(operand, value, state);
        };
        const auto index = static_cast<std::size_t>(kind);
        require(index < 5, "Invalid ECL lifecycle kind");
        std::size_t cursor = entries.instruction_offsets[index];
        std::vector<std::size_t> stack;
        std::map<std::size_t, std::size_t> boundaries;
        // Track decoded intervals. Branches into known operands/data fail.
        const auto decode = [&](std::size_t offset) {
            require(result.steps < limits.steps, "ECL step limit exceeded");
            ++result.steps;
            result.bytecode_offset = offset;
            const auto instruction = decode_ecl_instruction(code, offset, opcodes);
            const auto end = offset + instruction.size;
            const auto next = boundaries.lower_bound(offset);
            if (next != boundaries.end()) require(next->first == offset || end <= next->first, "Overlapping ECL instructions");
            if (next != boundaries.begin()) require(std::prev(next)->second <= offset, "ECL branch inside operand data");
            boundaries[offset] = end;
            // Decode arity from profile, but reject semantic conflicts explicitly.
            const auto expected = minimal_vm_opcodes(EclImageDialect::sprite3,EclAreaDialect::map3);
            const auto spec = std::find_if(expected.begin(), expected.end(), [&](const auto& s) { return s.opcode == instruction.opcode; });
            if (spec != expected.end()) {
                const auto actual = std::find_if(opcodes.begin(), opcodes.end(), [&](const auto& s) { return s.opcode == instruction.opcode; });
                require(actual->operand_count == spec->operand_count && actual->variable_count_operand == spec->variable_count_operand,
                    "Conflicting VM opcode definition");
            }
            return instruction;
        };
        const auto branch = [&](const EclOperand& operand) {
            require(operand.type == 1 || operand.type == 3 || operand.type == 5, "Invalid ECL branch operand");
            require(operand.value >= dialect.code_base, "ECL address below code base");
            const auto offset = static_cast<std::size_t>(operand.value - dialect.code_base);
            require(offset >= entries.header_size && offset < code.size(), "ECL branch outside code body");
            return offset;
        };
        // Comparison belongs to the invocation; flags and memory persist.
        state.comparison = 0;
        while (true) {
            const auto instruction = decode(cursor);
            cursor += instruction.size;
            const auto& args = instruction.operands;
            switch (instruction.opcode) {
            case 0: result.exited = true; return result;
            case 1: cursor = branch(args[0]); break;
            case 2:
                require(stack.size() < limits.stack_depth, "ECL stack depth limit exceeded");
                stack.push_back(cursor); cursor = branch(args[0]); break;
            case 0x13:
                require(!stack.empty(), "ECL RETURN with empty stack");
                cursor = stack.back(); stack.pop_back(); break;
            case 3: {
                if (args[0].type >= 0x80 || args[1].type >= 0x80) {
                    const auto left = string(args[0], state, host), right = string(args[1], state, host);
                    state.comparison = left < right ? -1 : left > right ? 1 : 0;
                } else {
                    const auto left = numeric(args[0]), right = numeric(args[1]);
                    state.comparison = left < right ? -1 : left > right ? 1 : 0;
                }
                break;
            }
            case 0x14:
                state.comparison = numeric(args[0]) == numeric(args[1]) && numeric(args[2]) == numeric(args[3]) ? 0 : 1;
                break;
            case 9:
                if (args[0].type >= 0x80) {
                    require(args[1].type == 0x81, "Invalid ECL string destination");
                    const auto value = string(args[0], state, host);
                    const auto address = static_cast<std::uint16_t>(args[1].value);
                    if (!host.write_string(address,value)) state.strings[address] = value;
                } else store(args[1], numeric(args[0]));
                break;
            case 0x2f: store(args[2], numeric(args[0]) & numeric(args[1])); break;
            case 0x2a: {
                auto source = args[0];
                require(source.type == 1 || source.type == 3 || source.type == 5, "Invalid COPY_MEM source");
                const auto offset = numeric(args[1]);
                require(offset <= 65535 && source.value <= 65535 - offset, "COPY_MEM source out of bounds");
                source.value += offset;
                store(args[2], numeric(source));
                break;
            }
            case 0x11: case 0x12: host.text(string(args[0], state, host), instruction.opcode == 0x12); break;
            case 0x16: case 0x17: case 0x18: case 0x19: case 0x1a: case 0x1b: {
                const int cmp = state.comparison;
                const bool conditions[]{cmp == 0, cmp != 0, cmp < 0, cmp > 0, cmp <= 0, cmp >= 0};
                if (!conditions[instruction.opcode - 0x16]) cursor += decode(cursor).size;
                break;
            }
            case 0x25: case 0x26: {
                const auto table = decode_ecl_dispatch_table(code, instruction.offset, dialect, opcodes, entries.header_size);
                const auto selector = numeric(table.selector);
                if (selector >= table.target_offsets.size()) break; // SSI-style fallthrough
                result.dispatches.push_back({instruction.offset, selector, table.target_offsets[selector], table.subroutine});
                if (table.subroutine) {
                    require(stack.size() < limits.stack_depth, "ECL stack depth limit exceeded");
                    stack.push_back(cursor);
                }
                cursor = table.target_offsets[selector];
                break;
            }
            case 0x15: case 0x2b: {
                destination(args[0], state);
                const bool vertical = instruction.opcode == 0x15;
                const std::size_t first = vertical ? 3 : 2;
                std::vector<std::string> choices;
                for (std::size_t i = first; i < args.size(); ++i) choices.push_back(string(args[i], state, host));
                require(!choices.empty(), "Empty ECL menu");
                const auto choice = host.menu(choices, vertical ? string(args[1], state, host) : "", vertical);
                require(choice < choices.size(), "Menu host returned invalid choice");
                store(args[0], static_cast<std::uint32_t>(choice));
                break;
            }
            case 0x0a: {
                require(party_spec != opcodes.end(),"Unsupported VM party opcode");
                const auto index = numeric(args[0]);
                require(index <= 255,"Character index outside byte range");
                host.load_character(static_cast<std::uint8_t>(index));
                break;
            }
            case 0x39:
                host.select_character(string(args[0],state,host));
                break;
            case 0x24: host.combat(state); break;
            case 0x0e: {
                if (!images_enabled) throw EclDecodeError("Unsupported VM opcode 14");
                const auto id = numeric(args[0]);
                require(id <= 255,"Picture ID outside byte range");
                if (id == 255) host.clear_images();
                else host.picture(static_cast<std::uint8_t>(id));
                break;
            }
            case 0x0c: {
                const auto id = numeric(args[0]), distance = numeric(args[1]), picture = numeric(args[2]);
                require(id <= 255 && distance <= 2 && picture <= 255,"Invalid sprite request");
                host.sprite_start({static_cast<std::uint8_t>(id),static_cast<std::uint8_t>(distance),
                    static_cast<std::uint8_t>(picture)});
                break;
            }
            case 0x0d:
                if (!images_enabled) throw EclDecodeError("Unsupported VM opcode 13");
                host.sprite_advance(); break;
            case 0x31:
                if (!images_enabled) throw EclDecodeError("Unsupported VM opcode 49");
                host.clear_images(); break;
            case 0x37: {
                if (!images_enabled) throw EclDecodeError("Unsupported VM opcode 55");
                std::array<std::uint8_t,3> banks{};
                for (std::size_t i = 0; i < banks.size(); ++i) {
                    const auto id = numeric(args[i]);
                    require(id <= 255,"Wall bank ID outside byte range");
                    banks[i] = static_cast<std::uint8_t>(id);
                }
                host.wall_banks(banks);
                break;
            }
            case 0x21: {
                std::array<std::uint8_t,3> values{};
                for (std::size_t i = 0; i < values.size(); ++i) {
                    const auto value = numeric(args[i]);
                    require(value <= 255,"Map selector outside byte range");
                    values[i] = static_cast<std::uint8_t>(value);
                }
                host.load_area(values);
                break;
            }
            default: throw EclDecodeError("Unsupported VM opcode " + std::to_string(instruction.opcode));
            }
        }
    } catch (const std::exception& error) {
        result.diagnostic = error.what();
        return result;
    }
}
} // namespace goldbox
