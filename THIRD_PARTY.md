# Third-party licensing and provenance

## First speaker audio slice (2026-10-17)

SDL3 3.2.28 (existing zlib dependency) supplies audio devices, conversion and
stream queues through `runtime/audio.hpp`. No SDL implementation was copied.
SSI Engine's two SOUND_EVENT handlers at the recorded revision are empty and
provide no playback code to reuse. Gold Box Explorer's inspected tree has no
sound/music adapter. Neither project's audio implementation was incorporated.

David Fifield's Rust EXEPACK tool, reviewed via `viiri/exepack` revision
`a42f2c8fb1018feeb2c32fc7f7b1f01240d824c4`, offers a complete compressor and
executable reconstructor. COPYING waives copyright under CC0-1.0. We need only
bounded resident data extraction, not a Rust runtime or reconstructed executable.
No Rust or decompression-stub source was copied/translated. The small B0/B2
extractor independently implements behavior already observed in the installed
EXEPACK stub. It neither applies relocations nor writes executable output.

The SSI constant-tone subset and shared speaker synthesis are independent
interoperability code. Executable layout facts live in the game profile; no
original sound programs, note data, memory images or waveforms are distributed.
An external Unicorn probe remains private and is not a playback dependency.

The subsequent ECL audio-call route, signed sweeps, counter loops and modulation
are independently implemented from bounded execution of the installed original.
SSI Engine's recorded `VirtualMachine.java` CALL cases were consulted, but no
Java call routing, sound interpreter or scheduler was copied. Later-game
SOUND_EVENT opcode names were not applied to Pool without native evidence.

## RNG debug laboratory (2026-10-17)

The optional external analysis tool is Unicorn Engine 2.1.4, revision
`8028ec436f2d9376525352dd38ed9ed6b9f6be10`. Its README identifies GPLv2 and
COPYING contains GPL version 2; the public API header separately states LGPL2.
We conservatively record the tool as GPL-2.0-only rather than assuming a later
version grant. Its COPYING is preserved in `licenses/unicorn-GPL-2.0.txt`.
Authorship includes Nguyen Anh Quynh, Chen Huitao and the upstream contributors;
see upstream CREDITS.TXT and individual notices.

No Unicorn implementation is copied or adapted. The independently written
`scripts/rng_original_probe.py` calls the separately installed tool in a bounded
analysis subprocess, exchanging numeric rows with the debug screen. Unicorn is
not linked into, bundled with, or required to build/test the C++ engine. This
does not authorize distributing a combined GPLv2/GPLv3 artifact. The registry
records this optional tool dependency separately from adopted source.

The four-line mathematical LCG update and high-word modulo mapping are an
independent implementation of observed runtime behavior. Reusing a CPU emulator
provides the independent original-code oracle; porting another game's RNG or
embedding an emulator in the engine would offer no benefit here. Native seed
capture uses documented Windows memory-query/read APIs against an owned child.
No original machine-code bytes, memory dumps or game assets are distributed.

## Character creation (2026-10-16)

SSI Engine `engine/character/CharacterCreator.java`,
`engine/rulesystem/standard/{StandardFlavor,RaceData,ClassData}.java` and
`engine/rulesystem/Dice.java` at
`51d4b6d409fa798754f5ff2ccb249f3c0b6d029b` were consulted for creation
stages, single-class availability, alignments and 4d6 drop-lowest rolls.
These are research-only references; no Java implementation or character-byte
layout was copied/translated. Existing JSON validation, save and SDL menu
components were reused. Starting HP/gold are independently verified against
bounded local original-code execution; `CharacterValues.java` at the revision
above corroborates the original gold field. No upstream HP implementation is
adopted. Upstream fixed-HP and zero-racial-adjustment placeholders
were not adopted. The normalized character model, data profile, bounded dice,
save validation and SDL creator are independent project code.

Gold Box Explorer `src/Common/Plugins/Character/FruaCharacter.cs` at
`eac30abaa6ee66aea6f5d65ebe6d676b10015a8f` was consulted for spell identifier
names and slot-field annotations during starting spellbook research. Native
Pool execution independently verifies its starting spell flags and slot counts;
FRUA layout similarity alone is not treated as Pool compatibility. No viewer,
spell interpreter or upstream array implementation is copied or translated.

Multi-class creation also consults the previously listed SSI Engine standard
race/class definitions. They include later-game options that are not assumed
valid for Pool. Available combinations and their numeric starting state are
independently checked against the locally owned original selection/creation
routines; no upstream combined-class implementation is adopted.

The locally installed Pool manual (PDF pages 4, 5 and 17; character/party
sections and race-limit table) was read to cross-check six races, four base
classes, six PCs, 15-character names and race/gender ability caps. Only mechanical
facts were independently represented in the rules profile. No manual text,
images, executable character data or proprietary character files are distributed.
The rules profile is provisional: exact original rolling, racial adjustments,
alignment availability and exceptional-strength distributions need parity tests.

## Startup title/menu shell (2026-10-16)

SSI Engine Pool properties, `ui/classic/ClassicMode.java`,
`ui/classic/TitleRenderer.java` and `engine/Engine.java` at `51d4b6d409fa798754f5ff2ccb249f3c0b6d029b`
were consulted for title record ordering, centering independently of EGA header
offsets, the final title as menu backdrop,
and the distinction between mode/program menus and starting a game. These
remain research-only references. Existing MIT-attributed EGA decoding and the
SDL renderer provide meaningful reusable components; no Java menu, scheduler,
party or startup implementation was copied/ported. The independent C++ shell
uses explicit input advances and an exploration-only menu. It does not adopt
upstream timed title switching or claim a complete original New Game flow.

The 2026-10-17 timing change also consulted `ClassicMode.showNextTitle` at that
same revision, whose five-second schedule corroborates the independent original
Pool trace. The bounded SDL deadline and profile setting are independently
implemented; no Java scheduler or menu implementation was copied. Original
credits and exact party-menu layout remain unfinished. Startup sound research
uses private original-code traces and a calibrated DOSBox recording; no original
audio or text is included in the repository.

## ECL picture/sprite host (2026-10-16)

SSI Engine `VirtualMachine.java`, `Engine.java`, `ClassicMode.java`,
`RendererState.java` and Pool properties at
`51d4b6d409fa798754f5ff2ccb249f3c0b6d029b` were consulted for image callback
arguments, clearing, distance indexing and timed replacement. These remain
research-only GPLv3 references; no Java implementation was copied or ported.
The C++ VM callbacks, `engine/src/image_presentation.cpp` and SDL integration
are independent project code. Existing attributed decoders supply the images.
ClassicMode fixed 400 ms picture scheduling and RendererState frame wrapping
also informed an explicit profile presentation policy and independently written
elapsed-time state. This is provisional original timing; raw frame delays remain
uninterpreted. Timed sprite replacement remains unsupported. Exact upstream paths are retained in the registry.

## EGA wall graphics (2026-10-16)

`formats/src/graphics.cpp` adapts the MIT EGA header and packed-nibble decoding
from Gold Box Explorer `src/Common/Plugins/Dax/EgaBlock.cs` and
`EgaBlockSpecification.cs`, revision `eac30abaa6ee66aea6f5d65ebe6d676b10015a8f`.
Copyright (c) 2018 Bil Simser; notice preserved in
`licenses/gold-box-explorer-MIT.txt`. The C++ version returns bounded indexed
images, rejects mismatched sizes/counts and removes System.Drawing, filename
palette guessing and count adjustments. Independent wall assembly and explicit
bank loading were added to that MIT file. No SSI pixel data is included.

The same MIT integration now includes headerless mono tiles and multi-frame
EGA images from `MonoBlock.cs`, `MonoBlockSpecification.cs`, `EgaSpriteBlock.cs`
and `EgaSpriteBlockSpecification.cs` at that revision. It retains per-frame
metadata, makes independent/XOR-first modes and palette policy explicit,
preserves source bytes and adds strict bounds/exact-consumption checks.
SSI Engine `EGADependentImages.java`, `ExtendedEGAImage.java` and
`ExtendedEGADependentImages.java` were additional research-only cross-checks;
no GPL-3.0-only Java implementation was ported. No upstream assets were copied.

SSI Engine `EGAImage.java`, `Palette.java`, `WallDef.java`,
`DungeonWallSetBuilder.java`, `DungeonRenderer.java` and `DungeonMap.java` at
`51d4b6d409fa798754f5ff2ccb249f3c0b6d029b` were research references for format,
bank and perspective facts; no Java source was ported. Gold Box Explorer's
`DaxWallDefFile.cs` (including its cited Simeon Pilgrim layout source) and
`EgaVgaPalette.cs` were cross-checks only. Exact paths and distinctions are in
the registry. The independent native compositor is in `runtime/view.cpp`.

**Canonical machine-readable attribution registry:** [attribution/sources.json](attribution/sources.json), with [maintenance rules](attribution/README.md). It distinguishes implemented adaptations, external dependencies and research-only references; the sections below provide human-readable detail. Keep both updated in the same change.

## Actually used (M2)

| Library | License | Use | Distribution |
|---|---|---|---|
| Boost.JSON (Boost 1.75+, tested with 1.83) | Boost Software License 1.0 | JSON parsing/serialization | Supplied by package manager; not vendored |
| SDL3 (>=3.2, tested with 3.2.28) | zlib | Optional exploration window, input, rendering and temporary debug font | External unmodified library; preserve its notices with binary distributions |

SDL3 was validated at `7f3ae3d57459e59943a4ecfefc8f6277ec6bf540`.
Copyright (C) 1997-2025 Sam Lantinga. The exact notice is preserved in
[licenses/SDL3-zlib.txt](licenses/SDL3-zlib.txt). Integration lives in
`runtime/window.cpp` and `runtime/CMakeLists.txt`; no upstream implementation or
font files are copied. SDL replaces platform backend work; GEO view geometry and
application UI are independent project code. Its embedded debug font has a
separate **Public Domain** notice naming Marcel Sondaar and International
Business Machines in `src/render/SDL_render_debug_font.h`; preserved in
[licenses/SDL3-debug-font-PD.txt](licenses/SDL3-debug-font-PD.txt) and separately
registered as a dependency. No glyph data is vendored. OpenXcom `src/Engine/SurfaceSet.h`
was additionally examined at `4a4bae5fa2e5a09282928e84943fc84b093e34ff` as
research only (GPL-3.0-or-later); its sprite surfaces were not adopted.

Upstream: https://github.com/boostorg/json
License: https://www.boost.org/LICENSE_1_0.txt

Boost.JSON headers are compiled as part of this application using `boost/json/src.hpp` in exactly one translation unit. The Boost copyright/license notices remain with the installed library; package redistributors must comply with the Boost license.

## Research / prospective reuse (no source copied into this project)

- OpenXcom: https://github.com/OpenXcom/OpenXcom — GPL-3.0-or-later. Source reuse is potentially permitted under our new license after verifying individual files and preserving notices; none has been included.
- Gold Box Explorer: https://github.com/bsimser/Gold-Box-Explorer — MIT. Evaluate individual parser files and copyright notices before adapting.
- SSI Engine: https://github.com/TiCoKH/ssi-engine — GPLv3 license text at repository root; verify individual file terms and whether 'or later' permission exists before adopting. Research reference only thus far.
- QuickJS-NG: https://github.com/quickjs-ng/quickjs — MIT. Future script host candidate; not integrated.

## Reuse evaluation requirement

Before writing a new parser, event runtime, UI framework, renderer, or script host, consult [the open-source-first policy](docs/open-source-first.md). Record candidates and file-level GPL compatibility/licensing review before adopting or porting code. The candidate inventory in that policy is **not** a claim that the libraries are installed.

## Future optional dependencies (not installed by this project)

- SDL3 for windowing/input/rendering.
- RmlUi for interface rendering.
- QuickJS-NG for executing JS emitted by the TypeScript compiler.

No original SSI game files or code, and no copied OpenXcom or SSI Engine implementation, are currently distributed by this repository. Existing Boost.JSON remains independently licensed under BSL-1.0. See [LICENSING.md](LICENSING.md) for project licensing and historical MIT releases.

## Adapted code in M2 ECL decoding pilot (2026-10-12)

| Origin | File(s) adopted / behavior reference | License, revision, attribution | Local changes |
| --- | --- | --- | --- |
| [Gold Box Explorer](https://github.com/bsimser/Gold-Box-Explorer) | [DaxEclFile.cs](https://github.com/bsimser/Gold-Box-Explorer/blob/eac30abaa6ee66aea6f5d65ebe6d676b10015a8f/src/Common/Plugins/DaxEcl/DaxEclFile.cs) and [Memory.cs](https://github.com/bsimser/Gold-Box-Explorer/blob/eac30abaa6ee66aea6f5d65ebe6d676b10015a8f/src/Common/Plugins/DaxEcl/Memory.cs): 6-bit text decoding and typed operands | **MIT**, copyright (c) 2018 Bil Simser; revision `eac30abaa6ee66aea6f5d65ebe6d676b10015a8f`. Full original license preserved in [licenses/gold-box-explorer-MIT.txt](licenses/gold-box-explorer-MIT.txt) | Adapted from C# concepts/algorithm to C++20 in `formats/src/ecl.cpp` and `formats/include/goldbox/formats/ecl.hpp`. Replaced state-machine loop with bounded bit accumulator; added overflow/truncation checks, selected-dialect opcode table, exceptions, synthetic tests. Original MIT license and author acknowledgement retained. |
| [SSI Engine](https://github.com/TiCoKH/ssi-engine) | [EclArgument.java](https://github.com/TiCoKH/ssi-engine/blob/51d4b6d409fa798754f5ff2ccb249f3c0b6d029b/src/main/java/engine/script/EclArgument.java), [EclOpCode.java](https://github.com/TiCoKH/ssi-engine/blob/51d4b6d409fa798754f5ff2ccb249f3c0b6d029b/src/main/java/engine/script/EclOpCode.java) | GPLv3, revision `51d4b6d409fa798754f5ff2ccb249f3c0b6d029b` | **Behavioral reference only** for type widths and provisional opcode semantics; no Java source ported/copied and no Java dependencies introduced. Source-level reuse remains subject to license review. |

**Implementation status:** ECL text and operand decoding only. This is **not** a playable ECL event VM, and original game compatibility remains unvalidated. Source and tests use synthetic bytes only. See [docs/formats/ecl-v1.md](docs/formats/ecl-v1.md).

## Adapted code in M2 GEO decoding pilot (2026-10-12)

| Origin | File(s) adopted | License and revision | Changes / redistribution |
| --- | --- | --- | --- |
| [Gold Box Explorer](https://github.com/bsimser/Gold-Box-Explorer) | [GeoDaxFile.cs](https://github.com/bsimser/Gold-Box-Explorer/blob/eac30abaa6ee66aea6f5d65ebe6d676b10015a8f/src/Common/Plugins/GeoDax/GeoDaxFile.cs) (`ProcessBlocks`), [GeoWallRecord.cs](https://github.com/bsimser/Gold-Box-Explorer/blob/eac30abaa6ee66aea6f5d65ebe6d676b10015a8f/src/Common/Plugins/GeoDax/GeoWallRecord.cs), and [GeoMapRecord.cs](https://github.com/bsimser/Gold-Box-Explorer/blob/eac30abaa6ee66aea6f5d65ebe6d676b10015a8f/src/Common/Plugins/GeoDax/GeoMapRecord.cs) | **MIT**, Bil Simser, 2018; upstream revision `eac30abaa6ee66aea6f5d65ebe6d676b10015a8f`. MIT notice preserved at [licenses/gold-box-explorer-MIT.txt](licenses/gold-box-explorer-MIT.txt) | Adapted map-cell plane indices and wall/event splitting to bounded standalone C++20 `formats/src/geo.cpp` and `formats/include/goldbox/formats/geo.hpp`. Removed Windows/System.Drawing coupling and game-derived map-name list; added checked coordinates, trailing-byte metadata, explicit ECL archive+record binding, read-only CLI and synthetic tests. No original game data, pictures or map names incorporated. |

**Status:** GEO/ECL binding associates a caller-selected existing ECL *record*, not a decoded ECL dispatch-table address. Per-title validation is still required. See [docs/formats/geo-v1.md](docs/formats/geo-v1.md).

## Dispatch, DAX comparison and bounded runtime (2026-10-12)

SSI Engine at `51d4b6d409fa798754f5ff2ccb249f3c0b6d029b` was consulted for
`VirtualMachine.java`, `VirtualMemory.java`, `EngineConfiguration.java`,
`Engine.java`, `data/script/EclProgram.java`, `data/dungeon/DungeonMap.java`,
`data/DAXFile.java`, and the Pool/Curse properties.
Its root license is GPLv3; these files do not establish an or-later grant.
These are **research references**, not adapted Java implementations. The C++
dispatch resolver and bounded VM were written independently without Java/Vavr
dependencies. The numeric code-base and square-info configuration facts are
recorded in the unverified game profiles. No title checks enter the runtime.

Gold Box Explorer at `eac30abaa6ee66aea6f5d65ebe6d676b10015a8f` (MIT,
Bil Simser) was consulted for `src/Common/Plugins/Dax/DaxFile.cs` and
`src/Common/Plugins/DaxEcl/Program.cs`. Both it and SSI Engine agree that the
DAX header word excludes the leading two bytes. Our bounded RLE implementation
was retained; only index interpretation was corrected, with compatibility for
the previously distributed synthetic fixtures. Gold Box Explorer's pattern
search for event tables was not adopted.

The exact immutable paths and research/adaptation distinction remain in
`attribution/sources.json`. Existing MIT adaptations and their notices remain
unchanged. Synthetic success does not establish original-game compatibility.

The independently implemented SSI color backdrop also consults that same SSI
Engine revision's Pool properties, `VirtualMemory.java`, `ViewDungeonPosition.java`,
`RendererState.java` and `DungeonRenderer.java` for memory addresses, indoor
selection, sky palette indices and native band sizes. These remain research-only
references; no Java source was copied or translated. Original scene parity and
script overrides of the indoor context remain unverified.

The original-font renderer similarly consults the pinned SSI Engine Pool
properties and `MonocromeSymbols`, `GoldboxString`, `CustomGoldboxString`,
`AbstractRenderer` and `UIResourceManager` for font-record selection, character
order and foreground recoloring. It uses the existing MIT-adapted monochrome
decoder and independently written C++ atlas/SDL rendering. No Java source was
copied or translated; original glyphs remain in the user's local installation.

The independent EGA portrait compositor consults SSI Engine's pinned
`ClassicMode.java`, `Palette.java`, `UIResourceManager.java` and `UIScaler.java`
for head/body placement, opaque palette behavior and offset precedence. It uses
the existing EGA decoder and caller-supplied geometry; no Java compositor was
copied or translated. These remain research references in the attribution registry.

The three-operand map-loading adapter also consults that pinned SSI Engine's
Pool configuration, EclOpCode, VirtualMachine and Engine for argument layout and
supported dungeon selectors. The C++ read-only resolver, atomic map transition
and map-aware save binding are independently implemented, without upstream
private virtual-memory addresses or filename-search policy. No Java source was
copied or translated.

The independent read-only party adapter consults the same pinned SSI Engine's
VirtualMemory, VirtualMachine, EclOpCode, Pool configuration, CharacterValues
and CharacterValueType for LOAD_CHAR, the selected-character memory window,
name lookup and ability offsets. Address facts are explicit game-profile data;
normalized party access, validation and saves are independent C++. No Java
source was copied or translated. This remains a research-only reference, with
original character-access parity unverified.

WHO selection additionally consults the pinned EngineInputAction and
engine/input/MenuHandler for the single SELECT action and resumption behavior,
plus VirtualMemory's distinction between loading and selecting a character.
The C++ implementation reuses this project's menu host and updates configured
state after validation; no Java handler or UI implementation was copied/ported.

The symbolic status binding also consults the pinned
`character/forgottenrealms/CharacterStatusForgottenRealms.java` and CharacterValues
for the distinction between native status ordinals and script-facing readiness.
The optional normalized status, JSON persistence and configurable byte mapping
are independently implemented; no Java enum or status implementation was ported.
