# Architecture — M1

```text
[User-owned SSI game files] ---> [Future SSI adapters] ---\
                                                     [Normalized Content Model]
[Synthetic fixture journals.json] -> [JSON Importer] --/             |
                                                                    v
[examples/mods/*/mod.json] -> [Validation/Dependency Resolver] -> [Journal Resolver]
                                      |                             |
                                      v                             v
                              [Warnings/errors]              [Classic/Enhanced]
                                                                    |
                                                 [CLI now; SDL3/RmlUi later]
                                                                    |
                                                        [Future game state]

TypeScript -> compiled JavaScript -> future embedded QuickJS host (not implemented)
```

## Core contracts

- `ContentImporter` is an abstraction for independently implemented importers, including future SSI versions.
- `SyntheticJsonImporter` only accepts `sourceFormat: synthetic-v1` and does not claim original-game compatibility.
- `ImportedContent` describes the campaign/source identity and normalized journal entries.
- `discover_mods` reads and validates manifests; `resolve_mods` checks dependencies, missing journal targets, and field collisions.
- `build_journal` merges entries in resolved order, keeping originals intact.
- `make_session_metadata` produces descriptive startup metadata. A full save/load implementation remains future work.

## Invariants

- Binary readers will never mutate the original user-supplied installation.
- Deterministic mod load order; dependency constraints precede priority.
- A missing target is an error rather than creating implicit content.
- Classic mode shows unmodified imported entries.
- Game simulation stays independent of rendering or TypeScript runtime.
- Repository tests never include copyrighted SSI game content.

## Later phases

- First Pool of Radiance binary importer and ECL event interpreter.
- AD&D rules engine / exploration / combat.
- SDL3 presentation and optional RmlUi.
- QuickJS-NG TypeScript mod bindings (compiled JS), with sandboxing.
- Versioned saves and compatibility fingerprints.
