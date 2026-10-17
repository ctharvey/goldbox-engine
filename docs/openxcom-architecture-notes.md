# OpenXcom shape — architectural study, not source reuse

Review performed 2026-10-12. Reference repository:
https://github.com/OpenXcom/OpenXcom (GPL-3.0-or-later)

## Observed separation

From public repository docs / paths:

- `src/Engine`: low-level application systems.
- `src/Mod`: rules, data definitions, mod handling.
- `src/Savegame`: persisted game state.
- `src/Battlescape`: tactical battlefield systems.
- `src/Geoscape` and `src/Basescape`: other major game modes.
- `src/Interface`: reusable UI.
- Installed original resources are separate from user mods and save files.
- Mod packages can be enabled independently and have a defined loading order.

Sources:
- https://github.com/OpenXcom/OpenXcom
- https://github.com/OpenXcom/OpenXcom/blob/master/README.md
- https://github.com/OpenXcom/OpenXcom/blob/master/src/Battlescape/BattlescapeState.cpp
- https://github.com/OpenXcom/OpenXcom/blob/master/src/Savegame/SavedGame.cpp

These are high-level observations; **no OpenXcom code, resources, class
implementation, method bodies, or translated code is included here.**

## Application to our project

- `engine/`: game-independent session, event model, content resolution, and state.
- `formats/`: isolated per-format binary decoders (DAX, ECL, GEO, graphics).
- Gameplay (exploration, ECL interpreter) currently lives in `engine/`; combat and
  AD&D rules are not started. See [architecture](architecture.md#target-layout).
- `runtime/`: rendering, input, UI, and embedded scripts.
- `packages/mod-sdk/`: strongly typed public authoring interface.
- `examples/`: synthetic fixtures without copyrighted SSI assets.

## Differences from OpenXcom

- Different underlying mechanics (AD&D party-based RPG).
- Original binary records converted to normalized internal definitions.
- TypeScript-authored mods via embedded JS runtime, not OpenXcom ruleset design.
- Text/journal content provided by the user or separately licensed sources.
- Core designed to run headlessly to support deterministic compatibility tests.

## Licensing status after 2026-10-12 relicensing

These original architecture notes were authored before the move from MIT to **GPL-3.0-or-later**. OpenXcom implementations may now be considered for **attributed source reuse** once individual files, authorship, licenses, dependencies and notices have been checked. The architectural research itself remains useful and no OpenXcom implementation code has been included yet. See [../LICENSING.md](../LICENSING.md).
