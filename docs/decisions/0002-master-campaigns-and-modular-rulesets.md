# ADR-0002: Master campaigns and modular content rulesets

Date: 2026-10-12  
Status: **Proposed** — research-backed, not implemented  
Supersedes: nothing. M1 `mod.json` behavior remains supported.

## Context

Our M1 importer and mod loader already consume a JSON manifest with inline journal patch definitions, require explicit dependencies, and apply ordered overlays. Research into the real OpenXcom package system suggests three useful improvements: (1) a single active master game definition, (2) category-specific rule files, (3) an explicit separation between installed original resources and user mods.

Pool of Radiance's original DOS resources also contain ECL event bytecode and banked resource references, so game data must first become a stable normalized representation before mods are applied.

Research: [OpenXcom and Pool of Radiance investigation](../research/openxcom-mod-system-and-pool-of-radiance.md).

## Proposed decision

1. A game import creates a **virtual master campaign**, e.g. `ssi.pool-of-radiance.dos`, which refers to files in a user's own installation. It is not packaged with the engine or committed to Git.
2. Enable at most one campaign master per running session. A mod declares a compatible master identifier; compatible mods may be enabled individually.
3. Keep M1's `mod.json` as the single manifest filename. Introduce `schemaVersion: 2` only when a modular ruleset implementation and migration tests exist.
4. Add `rulesets` — explicit, ordered, relative paths to JSON files scoped by content categories. Directory names are organizational, not authoritative load order.
5. Content definitions use stable IDs with game namespaces, typed categories, and explicit add/update/remove operations. Preserve original archive/record/offset only as importer provenance.
6. Resolve `master -> required dependencies -> optional mods`, with deterministic priority/id ordering for otherwise unconstrained mods. Show conflicts with provenance before game start.
7. Persist imported game fingerprint, active master, engine/schema version, enabled mod IDs/versions and resolved order in save metadata.
8. When enabled in future, TypeScript mods compile to JS and can call a constrained, versioned API. JSON rulesets are the default path for most mods; scripts do not have direct file/network access.

## Proposed directory structure (not supported by M1 yet)

```text
<user-data>/
  mods/
    example.journal-enhancer/
      mod.json
      rulesets/
        journals.json
        events.json
      locales/
        en-US.json
      assets/
        portraits/
      scripts/
        main.js
  saves/
  config/
  cache/<game-fingerprint>/
<user-supplied-game-dir>/POOLRAD/...
```

## Illustrative manifest V2 — NOT executable in M1

```json
{
  "schemaVersion": 2,
  "id": "example.journal-enhancer",
  "name": "Enhanced Journals",
  "version": "0.2.0",
  "master": "ssi.pool-of-radiance.dos",
  "requires": [],
  "priority": 100,
  "rulesets": ["rulesets/journals.json"]
}
```

## Illustrative typed ruleset — NOT executable in M1

```json
{
  "schemaVersion": 1,
  "journals": [
    {
      "op": "patch",
      "id": "ssi.pool-of-radiance.dos:journal:17",
      "set": {
        "title": "An Unfamiliar Inscription",
        "body": "Invented example prose, not original game text."
      }
    }
  ]
}
```

The `journal:17` identifier is illustrative. Imported book and event identifiers must also distinguish journal entries, proclamations, and tavern tales. Original journal text comes from user-supplied materials. The importer may emit an empty text placeholder linked to an event reference.

## Mod operations — proposal

| Operation | Semantics | Failure behavior |
| --- | --- | --- |
| add | Create an explicit, unique definition | Error if ID exists |
| patch | Change specified fields of existing definition | Error if ID missing |
| replace | Replace entire definition | Error if ID missing |
| remove | Remove named definition | Error if referenced or policy disallows removal |

Array fields must use explicitly documented semantics (replace, append, remove values), not implicit recursive merging. Mod conflict diagnostics must include target ID, field path, mod ID, operation, and prior writer.

## How code consumes mods (independent API boundary)

```text
GameInstallationScanner -> GameImporter -> NormalizedContentSnapshot
                                                |
ModsDirectoryScanner -> ManifestValidator -> ModGraphResolver
                                                |
CategoryRulesetLoader -------------------------+
                                                |
                                  Immutable EffectiveContent
                                     /          |          \
                             Exploration    EventVM      Combat
                                     \          |          /
                                         GameSession
                                              |
                                        Presentation
```

Runtime game systems look up effective content by stable identity; they do not traverse mod directories directly or parse SSI DAX files. The importer retains source provenance for diagnostics. This isolation keeps the game simulation headless-testable.

## Required acceptance tests before declaring ADR accepted

- M1 mod manifests and inline patches work unchanged; V2 support is opt-in.
- Enable a mod with a valid `master` and reject another game's mod before applying any patch.
- Ruleset paths cannot escape the package or follow unsafe symlinks.
- Conflicting field writes are deterministic and emit actionable diagnostics.
- An imported event reference remains stable across supported minor source-version differences or migrates explicitly.
- Disabling a mod restores base content and does not alter original files.
- Engine loads imported game data and user-local journal content, while public CI uses synthetic data only.
- Saves detect missing mods, incompatible versions, changed game data and mismatched mod order.
- Scripts (when implemented) have permission/CPU/memory limits and versioned API declarations.

## Consequences

Advantages: established community modding conventions, additive pack installation, upgrade-safe overlays, reproducibility, and good fit for TypeScript tooling. Costs: normalized model and resource provenance require upfront design, and strict operations/schema may be more verbose than permissive YAML overlays.

**Historical implementation note:** No OpenXcom source code was incorporated when this ADR was written. The project has since moved to GPL-3.0-or-later, so compatible upstream source can be reused after full per-file license and provenance review. The proposed architecture itself remains independent of any particular upstream implementation.
