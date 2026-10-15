# Universal Gold Box import architecture (M2 first slice)

**Status:** Shared DAX/GEO/ECL scanning, declarative profiles, bounded event
routing/VM and a terminal exploration pilot are implemented. One local Pool of
Radiance installation has passed archive/layout checks and a standalone original
event has executed with saved state. This does not establish full campaign or
cross-version compatibility. See [validation scope](research/local-validation-2026-10-15.md).

## Design boundary

```text
user-supplied installation
  -> read-only installation inventory / SHA-256 fingerprints
  -> game profile (data-driven; two starter profiles)
  -> format adapter (DAX implemented; GLB future)
  -> resource-specific decoder (GEO/ECL pilots; characters/graphics future)
  -> normalized, provenance-aware campaign definitions (future)
  -> existing ordered mod resolver + future category rulesets
  -> engine (bounded exploration/events pilot; rules/combat future)
```

*One format decoder per genuinely different format or dialect; one data-driven profile per title.* This avoids a bespoke engine for each game. Profiles select adapters, rules and event dialects. They **do not** contain game binaries, text or assets.

The testable DAX reader lives in `formats/`, a standalone library with no dependency on the game engine or Boost. `game_profile.cpp` lives in the engine and validates the profile schema. Both components are independent of the existing M1 mod loader. The CLI `--scan-dax` checks the index and extracts every record, printing only IDs/offsets/sizes. `DaxArchive::extract` provides strict-bounds RLE decoding. The Python installation scanner streams file hashes and invokes the bounded CLI decoders without publishing payloads.

## Current profiles (unverified)

- `games/pool-of-radiance/profile.json` — candidate DOS DAX profile.
- `games/curse-of-the-azure-bonds/profile.json` — candidate DOS DAX profile.

Both explicitly report `validationStatus: unverified`, `eventDialect: unverified`, and `rulesProfile: adnd-first-edition-unimplemented`. They describe **planned integrations**, not playable support. The profile loader accepts `glb-v1` as a future format name but the CLI explicitly refuses to decode it until an adapter is implemented.

Optional `eventDispatch` configuration selects a reusable lifecycle dialect,
numeric code base, square-info address, and X/Y/direction addresses. Pool uses
`0x9900`; Curse uses `0x8000`, as recorded in the upstream research inventory.
The core contains no title-name checks. Curse's new configuration remains
synthetically tested only; no original Curse validation was performed.

## Compatibility acceptance per game/version

1. Fingerprint user-supplied install files without ever uploading or committing them.
2. Validate DAX index/record RLE against those files (including GEO quirks or exceptions).
3. Decode and cross-reference GEO maps, ECL event records, items, monsters and visuals independently.
4. Compare imported map/event counts, record references and decoded output against observed gameplay.
5. Validate combat and campaign progression, then mark the version/profile verified.
6. Only after validation, enable game-specific scripting and rules compatibility modes.

Later titles may use GLB containers and variations of ECL and rules. We should add adapters by **format capability**, not tie the logic to game titles.

## Source references / licensing

- DAX TOC and RLE layout discussion: https://forums.goldbox.games/index.php?topic=3148.0
- DAX TOC discussion / variations: https://forums.goldbox.games/index.php?topic=1073.0
- Later GLB format observations: https://forums.goldbox.games/index.php?topic=3079.0
- Gold Box Explorer (MIT) as a research reference: https://github.com/bsimser/Gold-Box-Explorer

This implementation uses independently written parsing logic based on public format observations. No GPL source code, SSI resources, or third-party parser code was copied or vendored.
