# Research: OpenXcom mod architecture and Pool of Radiance resource structure

Date: 2026-10-12  
Status: research / design input only  
Scope: GPL-3.0-or-later Goldbox Engine (MIT when initial research was written). **No OpenXcom source or SSI assets were incorporated during this investigation.**

## Executive findings

1. OpenXcom has an installed master-game mod, optional user mods, separate original resource files, and declarative ruleset overlays. A user enables mods and the engine resolves effective definitions from the chosen master plus compatible enabled mods.
2. The original Gold Box *Pool of Radiance* DOS data is split into indexed DAX resource files (geography, event bytecode, monsters/items, visuals, etc.). ECL controls campaign progression and also refers to numeric resources and persistent state.
3. The *data flow and boundaries* remain useful; now that project-owned code is GPL-3.0-or-later, compatible upstream GPL code can also be assessed for reuse with full provenance and licensing review. The Goldbox design must account for ECL/event semantics and must not make modders edit proprietary DAX binaries.

## A. OpenXcom: directory-level observation

Verified from:
- https://github.com/OpenXcom/OpenXcom/tree/master/bin/standard
- https://github.com/OpenXcom/OpenXcom/tree/master/bin/standard/xcom1
- https://github.com/OpenXcom/OpenXcom/tree/master/bin/standard/XcomUtil_No_Psionics
- https://github.com/OpenXcom/OpenXcom/blob/master/README.md
- https://www.ufopaedia.org/Ruleset_Reference_Nightly_%28OpenXcom%29

The actual base mod `bin/standard/xcom1` contains:
- `metadata.yml` with `id: xcom1`, `isMaster: true`, and an original-resource directory name;
- multiple category-based `.rul` YAML files, including `items.rul`, `terrains.rul`, `units.rul`, and others;
- additional `Language/`, `MAPS/`, `ROUTES/`, and `Resources/` directories.

The actual optional `XcomUtil_No_Psionics` mod contains:
- `metadata.yml` with `master: xcom1`;
- a separate `.rul` with edits/removals of typed definitions.

The user install layout distinguishes **user** (mods, saves), **config**, and **data** (original game resource folders, standard mods). Mod folders can be unpacked into user mods and enabled in game.

The OpenXcom community reference identifies mod metadata fields such as identity, version, master dependency and resource requirements. Rulesets contain categories of records identified by stable type names. The older Rulesets reference documents order-sensitive overlays; later definitions update prior records sharing an identifier. Specific advanced directives documented as *OpenXcom Extended* (OXCE) should not be presented as baseline OpenXcom capabilities.

### Conceptual loading pipeline (independent summary)

```text
Enumerate candidate mod packages
   -> Parse metadata and identify master / compatible extensions
   -> Read user-enabled configuration
   -> Validate master relationship and determine deterministic load order
   -> Resolve original resources
   -> Load master content definitions
   -> Apply enabled category-based ruleset overlays
   -> Resolve references to the effective definitions
   -> Initialize game session and mod-aware saves
```

This sequence is an **architecture proposal informed by observed behavior**, not a line-by-line description or translated algorithm from OpenXcom source.

### Best patterns to borrow

- One active campaign master; optional compatible extension mods.
- Mod is a self-contained folder with metadata and logically grouped rule files.
- Original game files are user-supplied and separate from mod packages.
- Stable, symbolic identifiers for the normalized game data.
- Order-controlled overrides that only describe what changes.
- Enable/disable, dependency validation, version display, conflict diagnostics, asset paths, and runtime provenance.
- Separate user saves, settings, installed game data, and mods.

### Improvements for our project

- Use explicit `add`, `replace`, `patch`, and `remove` semantics, not ambiguous merging.
- Never silently change array semantics: support path-targeted edits and intentional append/remove operations.
- Fail or warn on multiple mods overriding the same field; show provenance ("which mod last wrote this?").
- Lock the resolved mod graph and original-game fingerprint into save metadata.
- Require schema version and target game compatibility. Avoid relying on directory names for identity.
- Keep strong editor/schema support for TypeScript-first mod developers.
- Support custom TypeScript hooks only through a restricted JS host in a later milestone.

## B. SSI Pool of Radiance (DOS): observed resource structure

Primary sources:
- https://www.dosdays.co.uk/topics/Games/game_poolrad.php (installed DOS files and configuration)
- https://forums.goldbox.games/index.php?topic=1912.0 (GEO and archive grouping)
- https://forums.goldbox.games/index.php?topic=2519.0 (detailed ECL event walk-through)
- https://forums.goldbox.games/index.php?topic=3148.0 (DAX container and compression)
- https://github.com/bsimser/Gold-Box-Explorer (MIT-licensed resource browser; no code imported)
- https://gbc.zorbus.net/ (Gold Box Companion demonstrates event inspection, automapping, journals)

The DOS installation includes `POOL.CFG` and multiple game-specific `.DAX` archives. Notable families:

| Family | Apparent role | Notes |
| --- | --- | --- |
| `GEO*.DAX` | Map geometry/location data | Locations packed in numbered resource archives |
| `ECL*.DAX` | Campaign event bytecode | Branching, text, menus, encounters and quest flags |
| `MON*CHA.DAX` | Monster/character resources | Referenced from events |
| `MON*ITM.DAX`, `MON*SPC.DAX` | Monster inventory/effects | Presence may vary by archive |
| `ITEM*.DAX` | Items | Distinct from party save inventory |
| `PIC*.DAX`, `CPIC*.DAX`, `SPRIT*.DAX` | Visual resources | Names/variants need version verification |
| `WALLDEF*.DAX`, `WALLSET*.DAX` | Exploration wall resources | Used for first-person view |
| `*.CHA`, `SAVGAM*.DAT` | Characters and saves | User state, not baseline content |

The numbered bank groupings historically correlate with original floppy organization. They should become provenance keys, **not public mod API IDs**.

### DAX preliminary container contract (requires validation)

Published community format descriptions indicate:
- first 2 bytes identify data/header offset;
- a variable number of 9-byte entries, each encoding record ID, relative offset, uncompressed size, stored size;
- stored data may use a run-length encoding scheme.

This is sufficient to define an **independently written first importer spike**, but behavior, endianness, compression corner cases, naming variations and title/version differences must be proven against a *legally obtained user installation*. Do not treat all DAX families and Gold Box versions as interchangeable.

### ECL behavior and why it changes the mod design

Community analysis of Phlan's slums identifies a map-square event selector dispatching ECL routines. Those routines can:
- query/set persistent quest flags;
- display messages and pictures;
- offer menus and parlay choices;
- load monsters and initiate combat;
- award items/treasure;
- change map position and branch to different events.

Consequently, a mod-friendly IR should preserve:

```text
Campaign -> Region -> Map -> Trigger -> EventGraph
                               |
                               +-- conditions/state reads and writes
                               +-- text references and menu choices
                               +-- encounter definitions and outcomes
                               +-- durable quest progression
```

The initial importer must preserve low-level **origin provenance** (source filename, DAX record ID, bytecode offset) to debug parity, while exposing a stable normalized identifier for mods. Example conceptual key: `pool-of-radiance:slums:event/3` (placeholder until mapping is verified). Do not expose transient memory addresses as stable public IDs.

### Original story text isn't the same thing as journal-book text

The game executable/events may store screen messages while the physical Adventurer's Journal contains additional numbered passages, tavern tales, proclamations and illustrations. In-game references must be connected to **user-provided external journal materials**, not shipped copyrighted text. Different books/categories can reuse numeric IDs. Prefer identities such as `game + category + entry ID` instead of a global integer.

A baseline journal reference can exist without having an imported text body. A user-side asset importer can attach content from a legally obtained book/PDF. Mods can override or enrich the resolved text without modifying original resources.

## C. Recommended normalized flow

```text
User-selected DOS install + user-supplied manuals
         |
   per-game import adapter (read-only; fingerprints & validation)
         |
   normalized immutable master campaign snapshot
         |
   ordered declarative ruleset overlays + localized resources
         |
   optional capability-limited TypeScript-authored JS hooks
         |
   session: exploration + ECL-compatible event execution + AD&D rules
         |
   UI + save state (game/version/master/mods fingerprinted)
```

The **importer** should understand SSI binaries; the **engine** should understand normalized definitions; the **mod author** should generally not need to know DAX or ECL bytecode.

## D. Proposed mod package layout

```text
user-data/
  mods/
    example.journal-enhancer/
      mod.json               # V1 supported metadata; extend deliberately
      rulesets/              # Proposed V2, not yet implemented
        journal.json
        events.json
        monsters.json
      locales/
        en-US.json
      assets/
        portraits/
      scripts/
        main.js              # JS compiled from TypeScript; runtime not yet implemented
  saves/
  config/
  cache/                     # imported game data, never committed
game-installations/          # user-managed original SSI files, not distributed
```

The existing M1 engine uses `mod.json` with inline journal patches. **Do not claim this proposed file layout already loads.** For M2, we can add optional modular `rulesets/` discovery with explicit order to M1, then deprecate inline patches only with a migration path and tests. Avoid supporting two simultaneous manifest authorities without reason.

Choose JSON/JSONC initially to preserve the working Boost.JSON parser and TypeScript ergonomics. We borrow OpenXcom's **package conventions and typed ruleset concepts**, not necessarily its YAML syntax or file suffix. A move to YAML can be evaluated later if meaningful to community adoption.

## E. Legal/provenance boundaries

- OpenXcom uses GPL licensing: it was **architecture reference only** under our original MIT policy. Under the current GPL-3.0-or-later policy, specific compatible source files may be reused following audit and attribution; none have yet been copied.
- Gold Box Explorer is MIT: review exact file licenses, copyright notices, dependencies, and provenance before reusing parsers.
- Publicly documented binary field layouts and observable compatibility behavior may guide independent implementations. Maintain sources and tests that demonstrate our own work.
- Do not commit SSI game binaries, scans, art, game text, campaign exports, or proprietary manuals.
- A user's possession of original files does not license redistributing them. Imported content and derived caches remain local by default.
- Keep fictional/synthetic fixtures in CI; separately run local integration checks against a user's own installation.

## F. Immediate follow-ups / research gaps

1. Collect a read-only inventory (paths, sizes, hashes) of the user's DOS `POOLRAD` directory, excluding source assets from Git.
2. Implement or reuse a bounded, read-only DAX header/index scanner against synthetic fixtures; evaluate open-source parser files for **GPLv3 compatibility**, provenance and code quality.
3. Verify header parsing/RLE behavior against user-provided local binary samples and maintain per-version fingerprints.
4. Build an event/record reference table (`GEO -> ECL -> monsters/visuals/journal`) and a namespaced identifier policy.
5. Specify a strict modular ruleset schema extending M1's `mod.json` API without breaking current mods.
6. Test determinism: different mod order, collisions, disabled mod, unknown ECL instruction, and import round-trip metadata.
7. Only after that, implement event execution and first-person exploration.

## Sources / provenance

- OpenXcom README and mod directories: https://github.com/OpenXcom/OpenXcom
- OpenXcom rule metadata and update semantics: https://www.ufopaedia.org/Ruleset_Reference_Nightly_%28OpenXcom%29
- OpenXcom ruleset examples (some older-version documentation): https://www.ufopaedia.org/Rulesets_%28OpenXcom%29
- Original DOS install inventory: https://www.dosdays.co.uk/topics/Games/game_poolrad.php
- Original GEO/ECL relation: https://forums.goldbox.games/index.php?topic=1912.0
- ECL semantics: https://forums.goldbox.games/index.php?topic=2519.0
- DAX structure/compression: https://forums.goldbox.games/index.php?topic=3148.0
- Gold Box Companion: https://gbc.zorbus.net/
- MIT-format tooling: https://github.com/bsimser/Gold-Box-Explorer
