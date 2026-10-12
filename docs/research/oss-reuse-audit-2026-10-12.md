# Gold Box open-source reuse audit — 2026-10-12

**Status:** Research completed; no third-party implementation copied or vendored in this change.  
**Target repository:** [ctharvey/goldbox-engine](https://github.com/ctharvey/goldbox-engine) (GPL-3.0-or-later)  
**Decision:** Reuse focused modules, not whole engines. Keep original SSI assets out of the public repository.

## Executive decision

Existing efforts are notably complementary. Gold Box Explorer has C# readers for DAX, GEO, ECL and some GLB; SSI Engine has a Java ECL VM and distinct rules flavors; Dungeon Craft has extensive FRUA and rules features; OpenXcom has mature GPLv3 mod packaging. None of these is a drop-in universal playable and moddable Gold Box interpreter. High-value adoption should therefore start with **one bounded reader or opcode table at a time**, backed by synthetic tests and user-owned binaries for private validation.

**Prefer first:** MIT Gold Box Explorer ECL opcode/string and GEO map parsing, then GPLv3 SSI Engine opcode metadata/VM behavior. **Do not replace our already implemented DAX scanner purely for code reuse**; compare/test it against upstream and improve defects only.

## Evaluated implementations

| Priority | Upstream and exact files | License evidence | Reuse value and blockers | Adoption decision |
|---|---|---|---|---|
| P0 | [Gold Box Explorer — `DaxEclFile.cs`](https://github.com/bsimser/Gold-Box-Explorer/blob/master/src/Common/Plugins/DaxEcl/DaxEclFile.cs); [`Commands.cs`](https://github.com/bsimser/Gold-Box-Explorer/blob/master/src/Common/Plugins/DaxEcl/Commands.cs); [`Program.cs`](https://github.com/bsimser/Gold-Box-Explorer/blob/master/src/Common/Plugins/DaxEcl/Program.cs) | [MIT repository license](https://github.com/bsimser/Gold-Box-Explorer/blob/master/LICENSE), latest reviewed master `eac30abaa6ee66aea6f5d65ebe6d676b10015a8f` | Existing ECL text decoding and opcode/disassembly knowledge. C# source, some legacy/diagnostic code and heuristics, not a complete production VM. Need per-file provenance/asset audit and platform-independent C++ integration. | **Pilot adoption**: ECL lexical decoding/opcode table with attribution, provenance and synthetic tests. Never copy extracted SSI text into fixtures. |
| P0 | [Gold Box Explorer — `GeoDaxFile.cs`](https://github.com/bsimser/Gold-Box-Explorer/blob/master/src/Common/Plugins/GeoDax/GeoDaxFile.cs), [`GeoMapRecord.cs`](https://github.com/bsimser/Gold-Box-Explorer/blob/master/src/Common/Plugins/GeoDax/GeoMapRecord.cs), [`GeoWallRecord.cs`](https://github.com/bsimser/Gold-Box-Explorer/blob/master/src/Common/Plugins/GeoDax/GeoWallRecord.cs) | MIT at repo root; review exact files | Existing GEO map + walls structures, map-name/game tables and references to ECL. `GeoDaxFile.cs` also depends on Windows System.Drawing and project viewer APIs, so avoid a wholesale port. Game-name tables may be unreliable as canonical identifiers. | **Second pilot**: extract just neutral map/wall/event structures and parsing behavior behind shared `formats/geo`. |
| P1 | [SSI Engine — `EclOpCode.java`](https://github.com/TiCoKH/ssi-engine/blob/master/src/main/java/engine/script/EclOpCode.java), [`VirtualMachine.java`](https://github.com/TiCoKH/ssi-engine/blob/master/src/main/java/engine/VirtualMachine.java), [`EclProgram.java`](https://github.com/TiCoKH/ssi-engine/blob/master/src/main/java/data/script/EclProgram.java) | [GNU GPLv3 at root](https://github.com/TiCoKH/ssi-engine/blob/master/LICENSE), latest master `51d4b6d409fa798754f5ff2ccb249f3c0b6d029b`; no per-file 'or later' grant observed in reviewed Java source | Strongest VM and opcode reference: variable instruction widths, expression modes, branching and persistent state. Tied to Java/Vavr (`io.vavr`) and its engine callbacks. May constrain imported component's licensing to GPLv3 specifically unless an 'or later' grant is found. | **Reuse selectively** after file-level license review. Adapt opcode tables and behavioral tests before considering VM port. |
| P1 | [SSI Engine — `Flavors.java`](https://github.com/TiCoKH/ssi-engine/blob/master/src/main/java/engine/rulesystem/Flavors.java), [Buck Rogers flavor](https://github.com/TiCoKH/ssi-engine/blob/master/src/main/java/engine/rulesystem/buckrogers/BuckRogersFlavor.java) | GPLv3 root, check file terms | Separates STANDARD/KRYNN/BUCK_ROGERS. Valuable architecture/data for one-engine/many-rules profiles. Depends on Java character model. | Reference for modular rules profiles; port narrow calculations only after parity checks. |
| P2 | [Dungeon Craft / UAF — `ImportFRUAData.cpp`](https://github.com/grannypron/uaf/blob/master/src/UAFWinEd/ImportFRUAData.cpp), [`GameRules.cpp`](https://github.com/GaidamakUA/DungeonCraft/blob/master/src/Shared/GameRules.cpp) | Root [GPLv2](https://github.com/grannypron/uaf/blob/master/LICENSE) but **these reviewed files explicitly say GPL version 2 or any later version**; latest UAF master `4a4f3a29ba1e80f8548e4268d35592d4e23e5bda` | Huge C++ game/editor codebase, FRUA import and rules; requires care with legacy MFC/Windows, shared dependencies and mixed copyright provenance. **GPLv2-or-later files can be used under GPLv3 when other conditions are met**; do not generalize to unreviewed files. | Keep for FRUA milestone; file-by-file license audit and isolate portable rules/data first. |
| P2 | [OpenXcom — `ModInfo.cpp`](https://github.com/OpenXcom/OpenXcom/blob/master/src/Engine/ModInfo.cpp), [`Mod.cpp`](https://github.com/OpenXcom/OpenXcom/blob/master/src/Mod/Mod.cpp), [`Options.cpp`](https://github.com/OpenXcom/OpenXcom/blob/master/src/Engine/Options.cpp) | Headers explicitly **GPLv3 or later**, latest master `4a4bae5fa2e5a09282928e84943fc84b093e34ff` | Mature packaging, metadata, resource/loading rules; C++ but tightly X-COM-specific and uses yaml-cpp/other dependencies. Goldbox Engine already has working JSON manifests/loader; wholesale port would introduce complexity. | Borrow/refactor isolated metadata/compatibility patterns only when it outperforms our existing resolver. No duplication for its own sake. |
| Research only | [OpenUA](https://github.com/slaapliedje/OpenUA), especially [web-engine spec](https://github.com/slaapliedje/OpenUA/blob/main/docs/web-engine/README.md) | Repo root LICENSE **GPLv2** while [engine ADR](https://github.com/slaapliedje/OpenUA/blob/main/docs/engine/adr/0001-engine-license-and-clean-room.md) proposes MIT; ambiguous, needs author clarification. Latest master `48224774c52e77dbcc477323c67d5c91618a2317` | Major near-parallel plan: multi-dialect ECL, format library, multi-ruleset TS engine. The indexed tree did **not** contain the planned `web-engine/` implementation, only `docs/web-engine/` and other tools; avoid treating documentation as shipped code. Retro hardware FRUA port exists but is a different deliverable. | Reach out for compatibility research/collaboration; **no code copy** until terms and ownership are resolved. |
| Research only | [Amber Folio](https://github.com/amberfolio/amberfolio), e.g. [journal runtime](https://github.com/amberfolio/amberfolio/blob/main/core/src/machine/journal.cpp) | Root **AGPLv3**; current tree contains separately licensed material too | Strong integrated-journal and overlay UI behavior, but emulator-centric and AGPL network obligations require analysis if combined. | Reference only until architectural and legal review. |
| Blocked | [Pool of Radiance JS](https://github.com/stlhood/pool-of-radiance-js), [Goldbox RPG](https://github.com/opd-ai/goldbox-rpg) | At reviewed revisions, **no root LICENSE file** in repository tree. Goldbox RPG README advertises MIT, but badge is not a substitute for a license grant | Pool of Radiance JS contains asset-derived game-data JSON and an Apple II pipeline; the Go project is inspired-by, not original SSI compatibility. | Do **not** copy code or asset-derived fixtures unless a clear valid license and rights are confirmed. |

## Why an all-games, fully moddable implementation remains unfinished

- Decoding assets is only one layer. Campaign ECL needs accurate VM, persistent flags, branching, encounter outcomes and save semantics, often with title/dialect variations.
- Combat and character progression depend on distinct AD&D/Krynn/Buck Rogers rule profiles, plus original-game quirks.
- Cross-game parity requires owned binaries/data from multiple versions and lengthy end-to-end scenarios. Public CI must still avoid redistributing SSI assets.
- A mod platform needs stable normalized IDs, migrations, conflict diagnostics, editor support and reproducible saves **on top of** the interpreter.
- Projects often focus on one platform, one campaign, one utility, or mod authoring rather than all these constraints together.

This is a hypothesis supported by the audited project scopes; it is not a claim that nobody has ever implemented a complete alternative elsewhere.

## Recommended next implementation order

1. **ECL lexical and opcode pilot**: port only the best-tested MIT parsing/format logic from Gold Box Explorer, compare against SSI Engine opcode reference, and expose a neutral `EclInstruction` + byte offsets + decode warnings. Do not execute script or alter engine game state in the first PR.
2. **GEO map pilot**: adapt MIT reader patterns from Gold Box Explorer, isolate map dimensions/walls/events behind normalized `GameMap`; use synthetic maps. Do not copy game-derived map-name lists or proprietary extracted resources without verifying rights.
3. **DAX cross-check**: retain existing M2 reader, test header/compression behavior against independent OSS decoders and locally owned game archives. Do not replace proven code blindly.
4. **ECL VM bounded prototype**: adapt narrow GPLv3-compatible SSI Engine behaviors only after confirming license/version/provenance and mapping them to C++ state/execution APIs.
5. **Rules and editor**: later evaluate UAF GPLv2-or-later fragments for FRUA import, spells, combat and authoring. Keep obsolete MFC interfaces out of the portable core.
6. **Upstream coordination**: contact OpenUA maintainers about overlapping modern multiplatform vision and license inconsistency before investing in parallel TypeScript parsers.

## Adoption checklist for each PR

- Record exact upstream repository, commit, source files and original authors.
- Read **all involved file headers** and transitive dependency licenses; ensure GPLv3 compatibility (GPLv2-or-later differs from GPLv2-only).
- Preserve notices and include upstream attribution + copyright and modified-file details in `THIRD_PARTY.md`.
- Keep component boundaries small so cross-language ports do not pull unrelated dependencies.
- Synthetic unit tests in the public repository, and private integration checks against the user's own installation.
- No original SSI game binaries, maps, journals, artwork, or derived corpus in commits.

## Current implementation status

Research only in this document. **No upstream code has been imported by this audit**. Existing M2 synthetic DAX tests and data-driven Pool/Curse profiles are not evidence of original-game compatibility.
