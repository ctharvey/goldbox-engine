// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "goldbox/formats/ecl_dispatch.hpp"

#include <array>
#include <map>

namespace goldbox {

struct EclState {
    std::array<std::uint8_t, 65536> memory{};
    std::map<std::uint16_t, std::string> strings;
    int comparison{};
};
enum class EclImageDialect { disabled, sprite3 };
struct EclSpriteRequest {
    std::uint8_t sprite{}, distance{}, replacement_picture{};
};

class EclHost {
public:
    virtual ~EclHost() = default;
    virtual void text(std::string_view value, bool clear) = 0;
    // The host supplies deterministic choices; no OS input inside the VM.
    virtual std::size_t menu(std::span<const std::string> choices, std::string_view prompt, bool vertical);
    virtual void combat(EclState& state);
    virtual void picture(std::uint8_t id);
    virtual void sprite_start(EclSpriteRequest request);
    virtual void sprite_advance();
    virtual void clear_images();
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
};

// Headless, deterministic, bounded execution. Mutates only supplied state and
// calls the host. Failure preserves changes already made (no rollback claim).
EclExecution execute_ecl(std::span<const std::uint8_t> record, formats::EclEventKind kind,
    const formats::EclDispatchDialect& dialect, std::span<const formats::EclOpcodeSpec> opcodes,
    EclState& state, EclHost& host, EclLimits limits = {});

std::vector<formats::EclOpcodeSpec> minimal_vm_opcodes(EclImageDialect images = EclImageDialect::disabled);

} // namespace goldbox
