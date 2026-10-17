# Architecture

## Current layering (M1 content + M2/M3 pilot)

```text
formats/    pure decoders: DAX, ECL (lexer, dispatch), GEO, EGA graphics.
            No Boost, no engine dependency.
   ^
engine/     goldbox_core: content + mods + journal (M1), game profiles,
            ECL VM, exploration session, image presentation. Links formats.
   ^
runtime/    optional SDL3 view/window (GOLDBOX_BUILD_RENDERER, default OFF).
   ^
apps/       goldbox_cli (content/mod/scan CLI), goldbox_explore (terminal pilot,
            JSONL parity harness, optional --window).

scripts/    Python tooling, not linked into the engine: installation scanner,
            hygiene/attribution checks, WSL build wrapper, and the side-by-side
            comparison controller (compare_game.py and its compare_* modules).
tests/      C++ unit tests, Python CLI/controller tests (ctest-registered).
```

Includes point down the stack only. Engine simulation stays independent of
rendering and of the TypeScript runtime.

## Content pipeline (M1)

```text
[Synthetic fixture journals.json] -> [SyntheticJsonImporter] --\
                                                         [Normalized Content Model]
[examples/mods/*/mod.json] -> [Validation/Dependency Resolver] -> [Journal Resolver]
                                                                  |
                                                       [Classic / Enhanced mode]
```

- `ContentImporter` is the abstraction for independently implemented importers.
- `SyntheticJsonImporter` only accepts `sourceFormat: synthetic-v1` and rejects
  original-game identifiers.
- `discover_mods` reads and validates manifests; `resolve_mods` checks dependencies,
  missing journal targets and field collisions; `build_journal` merges in resolved order.
- `make_session_metadata` produces descriptive startup metadata, not a savegame.

## Original-resource pipeline (M2/M3 pilot)

```text
user-supplied installation -> read-only inventory (SHA-256) -> game profile
  -> DAX/GEO/ECL/graphics decoders (formats/) -> ECL VM + exploration (engine/)
  -> terminal host | JSONL harness | SDL3 window (apps/, runtime/)
```

Details: [universal importer](universal-importer.md), the format notes in
`docs/formats/`, and the [validation scope](research/local-validation-2026-10-15.md).

## Invariants

- Binary readers never mutate the user-supplied installation.
- Deterministic mod load order; dependency constraints precede priority.
- A missing target is an error rather than implicit content.
- Classic mode shows unmodified imported entries.
- Repository tests never include copyrighted SSI game content.

## Target layout

Not yet created; today's code sits where noted.

| Planned | Today |
| --- | --- |
| `importers/` per-game importers producing `ImportedContent` | `formats/` decoders; only `SyntheticJsonImporter` implements `ContentImporter` |
| `gameplay/` exploration, events, combat, rules | exploration and ECL VM in `engine/`; combat and rules not started |
| Content/simulation split of `goldbox_core` | one library holds both (decide before M4) |
| Embedded QuickJS-NG TypeScript host | design only: [security](security.md); `packages/mod-sdk` types |
| Versioned saves with compatibility fingerprints | prototype position/event state only |

Create `importers/` and `gameplay/` when real code needs them, not as placeholders.
The `assets/` README is a standing licensing rule, not a stub.
