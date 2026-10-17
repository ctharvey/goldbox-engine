# ECL lexer/decode pilot (M2)

**Status:** Lexer, bounded dispatch resolver and minimal headless VM implemented
and synthetically tested. Lifecycle headers and a narrow original event path
have also been checked against local Pool of Radiance resources.
No complete campaign execution or playable compatibility claim.

This is a deliberately narrow adoption from [Gold Box Explorer](https://github.com/bsimser/Gold-Box-Explorer) (MIT). Its ECL text decoder and typed-operand behavior have been adapted to standalone C++20 with strict bounds checks, exception-based error reporting, and a caller-selected instruction dialect. Opcode identifiers/operand counts were cross-checked against [SSI Engine](https://github.com/TiCoKH/ssi-engine) (GPLv3), but no Java VM code was copied.

## Interfaces

`goldbox/formats/ecl.hpp` declares:

- `decode_ecl_sixbit_text(span<uint8_t>)`: decode historically packed groups of four six-bit symbols from three bytes. Symbols 1..31 map to `A`..`_`, symbols 32..63 map to ASCII values; zero is padding and not emitted. Text may contain formatting/control codes; it is not localized Unicode.
- `decode_ecl_operand(bytes, offset&)`: decodes one typed operand, only advancing the passed offset on success. Handles 8-bit literal, 16-bit word/locations, 32-bit extended form, inline packed text, and string-address operands. Unsupported types fail closed.
- `decode_ecl_instruction(bytes, offset, dialect)`: reads one opcode and operands at a **known entry point** using a caller-provided `EclOpcodeSpec` table. Supports dynamic operand counts, rejects duplicate opcode definitions, unknown instructions, truncation, and oversize counts. Does not follow ECL branches or execute actions.
- `provisional_ecl_opcodes()`: a **non-authoritative shared starter set**, excluding known title-dependent opcode collisions such as 0x1F, 0x21 and 0x27. Title/version dialects will replace or extend it once validated.

The decoder is independent of the DAX container layer. Callers must first extract a record from `DaxArchive`, determine the entry point from the game profile/map event table, then pass a verified address and dialect. **Never sweep arbitrary ECL bytes as a linear instruction stream**: data and event jump tables may be interleaved, leading to false disassembly.

## Code provenance

- Adapted six-bit text/operand representation based on `src/Common/Plugins/DaxEcl/DaxEclFile.cs` and `Memory.cs` at Gold Box Explorer revision `eac30abaa6ee66aea6f5d65ebe6d676b10015a8f` (original copyright 2018 Bil Simser, MIT); original MIT license preserved in `licenses/gold-box-explorer-MIT.txt`.
- SSI Engine `src/main/java/engine/script/EclArgument.java`, `EclOpCode.java`, `EclInstruction.java` at revision `51d4b6d409fa798754f5ff2ccb249f3c0b6d029b` consulted as a behavior reference. No Java code or tables copied verbatim.
- This code is not verified as accurately parsing real game variants. `type 0x04` is decoded as four payload bytes plus one type byte (5 consumed bytes), rather than inheriting a reported length discrepancy in one upstream implementation.

## Synthetic tests

`tests/ecl_test.cpp` covers padding and 6-bit packing, word/dword/text operands, failure offsets, empty/truncated strings, unknown opcodes, a custom dialect, duplicate specs, variable counts, size caps, and instruction-boundary handling. No SSI game text or binary assets are included.

## Next compatibility gates

1. Obtain a local inventory of a user's licensed Pool of Radiance and Curse of the Azure Bonds installations; keep records and assets local.
2. Map GEO event selectors to specific ECL bytecode entry points. Validate ECL address bases per game/profile.
3. Compare parsed instructions against a known reference trace; differentiate generation-1 opcode conflicts and extended dialects.
4. Add trace/provenance context `(game, archive filename, record ID, offset, dialect)` and test unsupported opcode diagnostics.
5. Build a bounded interpreter with isolated event state and save compatibility only after semantic parity tests.

## Bounded runtime and state

`engine/include/goldbox/ecl_vm.hpp` separates `EclState`, `EclHost`, and execution
limits from the format decoder. The default budgets are 10,000 decoded steps
and 64 subroutine frames. EXIT, GOTO/GOSUB/RETURN, numeric/text memory writes,
indexed COPY_MEM, AND, comparison/conditional execution, inline text and menus
are supported. IF instructions conditionally execute the next decoded
instruction. ON_GOTO/ON_GOSUB select checked addresses and retain a typed
`DispatchSelection` trace. Code-address numeric reads use imported bytes;
numeric writes into the program region are rejected. Other opcode semantics,
unresolved string pointers and absent menu/combat hosts fail explicitly.

State changes made before an error remain visible; execution is not
transactional. The host owns input and combat behavior. Determinism assumes
deterministic host responses, and callers must bound blocking host operations
with a process timeout. No complete combat implementation is supplied.

`ExplorationSession` supplies map coordinates, facing and raw square info via
profile-selected addresses. Version-1 local saves preserve position/facing,
numeric memory and string state. Invalid loads are rejected before publishing
state. The terminal identifies its source pair using game/archive/record IDs,
configuration and non-cryptographic content checksums; the separate installation
inventory uses SHA-256. These are prototype saves, not original SSI save files
or a complete campaign/mod compatibility format.

The opt-in SSI image dialect also decodes LOAD_AREA_DECO (0x37, three byte-valued
selectors). The VM calls a typed host method and does not open archives or invent
original memory addresses for this presentation selection. The SDL host validates
and publishes banks, then records optional typed `wallBanks` session/save state.
Absent hosts and out-of-range values fail explicitly. Legacy saves without this
metadata remain readable by the engine; the wall-enabled window rejects an
ambiguous visual restore. Selecting archives or loading maps remains separate.

See [original-resource evidence and remaining gates](../research/local-validation-2026-10-15.md).
