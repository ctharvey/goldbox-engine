// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "goldbox/formats/ecl_dispatch.hpp"
#include "goldbox/random.hpp"

#include <array>
#include <map>
#include <optional>

namespace goldbox {

struct EclState {
    std::array<std::uint8_t, 65536> memory{};
    std::map<std::uint16_t, std::string> strings;
    int comparison{};
    std::optional<HighWordLcg32> random{};
};
enum class EclRandomDialect { disabled, byte_upper };
enum class EclImageDialect { disabled, sprite3 };
enum class EclAreaDialect { disabled, map3 };
enum class EclPartyDialect { disabled, load_character, weighted_strength };
struct EclSpriteRequest {
    std::uint8_t sprite{}, distance{}, replacement_picture{};
};
struct EclSoundCall {
    struct Case { std::uint16_t selector{}, sound{}; };
    std::uint16_t function{}, selector_address{};
    std::vector<Case> cases;
};
struct EclPositionAdvanceCall {
    std::uint16_t function{}, wall_type_address{};
};

class EclHost {
public:
    virtual ~EclHost() = default;
    virtual void text(std::string_view value, bool clear) = 0;
    // The host supplies deterministic choices; no OS input inside the VM.
    virtual std::size_t menu(std::span<const std::string> choices, std::string_view prompt, bool vertical);
    virtual void combat(EclState& state);
    virtual void native_call(std::uint16_t function);
    virtual void sound(std::uint16_t id);
    // Presentation pacing belongs to the host; headless hosts use virtual time.
    virtual void delay();
    virtual void picture(std::uint8_t id);
    virtual void sprite_start(EclSpriteRequest request);
    virtual void sprite_advance();
    virtual void clear_images();
    virtual void wall_banks(std::array<std::uint8_t,3> banks);
    virtual void load_area(std::array<std::uint8_t,3> values);
    virtual void load_character(std::uint8_t index);
    virtual void select_character(std::string_view prompt);
    virtual std::uint8_t party_strength();
    // Prepare host resources before a validated program is published and initialized.
    virtual void program_changed(std::uint8_t record);
    // Optional virtual memory overlays. Unhandled accesses retain ordinary RAM/string semantics.
    virtual std::optional<std::uint8_t> memory_byte(std::uint16_t address);
    virtual bool write_memory(std::uint16_t address, std::uint32_t value, std::size_t width);
    virtual std::optional<std::string> string_at(std::uint16_t address);
    virtual bool write_string(std::uint16_t address, std::string_view value);
};

struct EclLimits {
    std::size_t steps{10000};
    std::size_t stack_depth{64};
};

struct EclExecution {
    struct DispatchSelection {
        std::size_t table_offset{};
        std::uint32_t selector{};
        std::size_t target_offset{};
        bool subroutine{};
    };
    std::size_t steps{};
    bool exited{};
    std::size_t bytecode_offset{};
    std::string diagnostic;
    std::vector<DispatchSelection> dispatches;
    // A program transfer ends this invocation; the session resolves it safely.
    std::optional<std::uint8_t> next_program;
    std::size_t program_transfers{}; // Session-resolved transfers replace a move continuation.
};

// Headless, deterministic, bounded execution. Mutates only supplied state and
// calls the host. Failure preserves changes already made (no rollback claim).
EclExecution execute_ecl(std::span<const std::uint8_t> record, formats::EclEventKind kind,
    const formats::EclDispatchDialect& dialect, std::span<const formats::EclOpcodeSpec> opcodes,
    EclState& state, EclHost& host, EclLimits limits = {},
    EclRandomDialect random = EclRandomDialect::disabled);

std::vector<formats::EclOpcodeSpec> minimal_vm_opcodes(EclImageDialect images = EclImageDialect::disabled,
    EclAreaDialect area = EclAreaDialect::disabled, EclPartyDialect party = EclPartyDialect::disabled);

} // namespace goldbox
