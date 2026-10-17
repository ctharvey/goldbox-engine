# Third-party licensing and provenance

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
