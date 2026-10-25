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
AND stores the bitwise result and sets comparison to zero for a zero result,
one otherwise; subsequent IF instructions use that flag rather than an earlier
COMPARE. Comparison is updated only after a successful store.

State changes made before an error remain visible; execution is not
transactional. The host owns input and combat behavior. Determinism assumes
deterministic host responses, and callers must bound blocking host operations
with a process timeout. No complete combat implementation is supplied.

RANDOM is enabled only by the profile's `random.eclDialect: ssi-byte-upper-v1`
and requires explicit RNG state. Independent analysis and bounded execution
of the fingerprinted original DOS Pool of Radiance handler establish a
build-specific rule: narrow the resolved upper operand to one byte, draw using
runtime bound `min(upperByte + 1, 255)`, and consume one PRNG draw even when the
result must be zero. Upper bounds 0..254 are inclusive; 255 yields 0..254.
This is a profile-selected rule, not a shared rule for all ECL dialects. Invalid
operands/destinations fail without consuming a draw. Saves preserve algorithm,
parameters, dialect and current state; restore rejects mismatched configuration.
Legacy saves without RNG state remain readable, but RANDOM fails explicitly
until a new seeded session is started. See the
[original RNG and handler evidence](../research/local-validation-2026-10-15.md#original-ecl-random-caller-verification-2026-10-17)
for the generator formula, tested operand forms and remaining parity limits.

`ExplorationSession` supplies map coordinates, facing and raw square info via
profile-selected addresses. Without a movement binding, the original prototype
still invokes MOVE only after successful geometric movement, including cells
whose GEO selector is zero. An optional `eventDispatch.movementAddresses` array
selects the source-cell lifecycle: block, exit-attempt, previous-X and previous-Y
registers, in that order. Position bindings are required; overlapping context,
party, program or native-call registers are rejected at their adapter boundary.
Each attempt prepares block/exit state, invokes MOVE at the source, applies the
permitted step, then invokes SEARCH even when blocked. Native GEO16 traversal
uses the source edge and wraps at 16 cells. Solid walls and closed doors remain
blocked; opening/bashing prompts are not implemented. A program transfer replaces
the pending physical step/search with its own initialize/move/search continuation,
including self-transfers. Compound execution shares one step budget. Scripts may
reposition or load maps; their successful resulting coordinates are validated
before the continuation. Save restoration does not trigger an event.
Map and ECL selection remain explicit adapter responsibilities.
The static nonzero-selector inventory can use known X/Y for its supported
GOTO/AND/WRITE_MEM routes and numeric COMPARE/IF conditions. IF skips exactly
one decoded instruction when false; AND replaces the comparison flag, as in the
VM. Unknown reads report the required address rather than assuming zero.
Arrival facing, live flags and arbitrary code remain unresolved. This inventory
is not an exhaustive list of runtime events or proof of movement parity.
Version-1 local saves preserve position/facing,
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

## Three-operand dungeon map loading

`areaDialect: ssi-area-map3-v1` explicitly selects opcode 0x21 with three numeric
byte operands; absent/disabled preserves the old unsupported behavior. Pool
opts in, while other profiles remain unchanged. Conflicting arity, duplicate
definitions, invalid operand types and values above 255 are rejected. The VM
calls `EclHost::load_area`; it does not open archives or select a game by name.

The shared exploration adapter supports the researched GEO dungeon modes:
all-zero selectors load record 0; a first selector other than 127/255 with
second selector 2/127/255 loads the first selector's record; (255,2,*) retains
the current dungeon. The third byte is preserved without assigning invented
meaning. Runic/overland/clear-map variants are explicitly unsupported here.

`MapResources` supplies a read-only resolver to normalized GEO16 content and a
caller-owned source identity. The CLI supplies the explicit GEO archive already
selected at launch, with an immutable snapshot and a non-cryptographic FNV-1a
change detector. It never searches similarly named archives. Another title or
campaign can provide a different resolver rather than adding title branches.

Lookup, returned record identity and position/direction are validated before
publishing a transition. Configured position bytes can supply script-written
coordinates at the load. The ECL program remains unchanged; NEW_ECL selects a
program separately through the configured resolver. Earlier VM writes retain partial-progress semantics on
failure. Successful decoration loading refreshes current square-info context.

The observed no-resource selector `(255,255,127)` deactivates the dungeon and
clears presentation images without discarding its cached normalized map. The
active/inactive state derives from the existing `area` tuple and survives saves;
no additional save field is required. Inactive geometry is hidden and movement
is rejected. Event context supplies zero square info until a dungeon is loaded.
Other unsupported area modes still fail explicitly; this is not an overland map
implementation.

After a map load, prototype saves also contain `area`, `mapRecord`, and
`mapSource`. All three are required together. Restore resolves/validates the map
and checks the source identity before publishing state; changed sources, missing
maps or inconsistent bindings fail atomically. Older saves without these fields
remain readable and restore the launch map when necessary. This is not an
original SSI save format or a complete campaign loader. The change detector is
not a cryptographic integrity guarantee; installation inventory still uses SHA-256.

SSI Engine's pinned Pool configuration, EclOpCode, VirtualMachine and Engine
were consulted for operand arity, dungeon mode selectors and separation from
program loading. The resolver, session transition and save implementation are
independent C++; no Java source was copied or ported.

See [original-resource evidence and remaining gates](../research/local-validation-2026-10-15.md).

## Optional party host

The VM can opt into one-operand LOAD_CHAR (0x0A) and WHO (0x39), with unique opcode metadata
validated before execution. Numeric and string operands pass through optional
host memory callbacks; unhandled addresses retain RAM/string semantics. The
exploration host attaches the profile-selected read-only party adapter described
in [character creation](../character-creation.md#read-only-script-adapter).
Character mutation, numeric race/class encodings and native character import
remain unsupported. Unsupported party access stops execution with a diagnostic;
earlier script progress is retained under the existing VM failure policy.

## Campaign startup and program binding (2026-10-18)

An optional profile `campaignStartup` selects initial normalized map/program IDs,
explicit map/program archive lists and a native position-refresh call token.
Its optional seven-entry `graphicsArchives` array binds pictures, sprites, heads,
bodies, wall definitions, wall tiles and shared tiles, in that order. Begin/load
select these campaign resources; creation/party previews retain their selected
portrait sources. Pool's opening uses group 3 and shared tiles from group 1.
Optional `campaignStartup.programGraphics` entries bind a `programRecord` to
another seven-entry `graphicsArchives` array in the same order. IDs are unique,
exclude the initial program, and require safe archive filenames. Pool explicitly
binds program 20 to group 2, retaining shared tiles from group 1. No filename
suffix inference is performed. Other unbound programs fail explicitly.
The host resolves graphics before publishing a program transfer or running its
initialize event. The SDL adapter retains immutable bundles and resets image and
wall presentation when switching. Save restore resolves its saved program and
decodes/validates saved wall banks before publishing the restored session.
The startup window uses this adapter for Begin; explicit map/ECL preview and
JSONL prototype modes retain their explicit launch bindings. Begin resolves both inputs
before publishing, resets script RAM/strings, preserves the created party and
its RNG continuation, and runs the initialize entry. Window errors restore the
pre-Begin session. Pool starts program/map zero; its opening script selects
(15,1), west, and the chosen city-tour route ends at (0,4), west.

Optional `campaignStartup.positionAdvanceCall` binds a native function token and
`wallTypeAddress`. Pool v1.3 selects 0xC01E and 0xC04E. The independent shared host
advances one cell in the current facing, wraps coordinates, and refreshes raw
square/wall context without collision checks. No events are triggered inside
this call; the surrounding script/lifecycle owns continuation. Function conflicts
and overlapping wall registers are rejected. Call tokens and addresses come from
the profile, with no title checks in the engine.

The boundary route next reaches opcode 0x1D, which Pool's pinned profile names
PARTY_STRENGTH (one destination). A profile `partyStrength` selects dialect
`ssi-weighted-party-strength-v1` and explicit `clericClass`/`mageClass` IDs. It
opts into the corresponding party opcode set; other profiles retain their
existing dialect. The VM uses a read-only host callback and the ordinary typed
destination writer, including protected-memory checks. Duplicate or conflicting
opcode definitions fail before execution. Another title's zero-operand
INPUT_RETURN meaning is unsupported and cannot be substituted.

The host computes each member's contribution from current HP, AC, THAC0 and
configured class levels, divides each contribution by ten, then adds with byte
wrapping. Empty parties yield zero. Unknown state and values outside the native
byte representation fail explicitly. Class IDs and creation defaults belong to
profiles/rules, with no title checks in the VM. The corresponding SSI Engine
handler is empty; the calculation was independently verified against original
code. See the local-validation research for its formula and limits.

The original-resource headless `boundaryProbe` now reports `executed`: map/program
20, (15,4), west, EXIT at offset 615. Its opening remains unchanged. This is
supported original-bytecode execution with an engine-created party; equivalent
native party state, UI pacing and full boundary parity remain unverified.
Program 20 graphics rebinding is implemented and its original wall banks decode;
matched original-scene pixel parity and additional program bindings remain open.

ADD stores through the existing typed memory interface with byte/word wrapping.
DELAY invokes the host; the VM never sleeps. `scriptDelayMs` and
`spriteReplacementMs` are explicit presentation policies, currently 400/1000 ms
for Pool based on SSI Engine research. A replacement portrait/picture appears
after the approaching sprite reaches its nearest distance. Native timing parity
has not been established.

NEW_ECL ends an invocation with a typed program ID. The session resolves and
validates that program, then invokes initialize/move/search with a shared step
budget and a 32-transfer limit. No byte-pattern instruction search is involved.
Saves with a program resolver contain `programRecord` and `programSource` as a
required pair. Restore validates both source and resolved code before publishing
any saved state. Optional `eventDispatch.transferClearRanges` contains numeric RAM
`[start,length]` pairs cleared after successful program resolution and before
initialize/move/search. Ranges must be nonempty, bounded and nonoverlapping;
host/context and program-memory overlaps fail before clearing. An absent list
preserves the prototype behavior. Pool selects 0x4A00..0x4A1F and
0x6E79..0x6E82, verified against its complete original interpreter reset routine.
The original's nonzero native preserve guard skips these clears once and resets
itself. Engine save restoration already preserves saved RAM without invoking
program initialization; native save-file loading and its guard are not emulated.
This policy changes numeric RAM only; string-storage parity is not established.

`goldbox_campaign_startup_tests` is an opt-in original-resource check, enabled by
`GOLDBOX_POOL_INSTALLATION` pointing to the authorized GAME/POOLRAD directory.
It validates decoded graphics, deterministic opening execution and exact engine
save round-trip. `scripts/pool_startup_reference.py` compares the resulting JSON
checkpoint with a bounded, privately copied DOS session. Its coverage is position,
facing, three opening flags and the original interpreter return address. It does
not establish complete party, pixel, timing, sound or original-save parity.
Observations also record the installed build's movement-block (0x6DC9) and
exit-attempt (0x6DD5) words from its separate 0x6B00 memory buffer. These are
diagnostic reference values; the opening comparison does not yet validate
engine movement against them.
