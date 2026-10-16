# Exploration renderer

Optional SDL3 window over the existing `ExplorationSession` and ECL host interface.
It draws a first-person view using independent grid-edge ray casting, a revealed
map overview, coordinates/facing, event text, and keyboard-driven menus. Geometry
checks walls on both sides of an edge and treats map bounds as solid, matching
movement. All nonzero wall IDs remain blocking; door flags are not interactive.
The renderer has no title checks and does not change decoder/VM state semantics.

Install SDL3 >=3.2 (for example, vcpkg `sdl3` on Windows), then configure:

```sh
cmake -S . -B build -DGOLDBOX_BUILD_RENDERER=ON
bash scripts/wsl-build.sh --no-tests=error
timeout 120 ./build/apps/goldbox_explore --window \
  games/pool-of-radiance/profile.json /private/GEO.DAX 1 /private/ECL.DAX 18 /private/save.json
```

Supply the explicit map/ECL pairing you have validated locally. On native Windows,
use the configured toolchain, build with two workers, and run the resulting `.exe`.
SDL's runtime library must be discoverable. The option defaults OFF, preserving
the headless build and JSONL harness without an SDL dependency.

Arrows or WASD move forward/backward and turn. Space searches, R rests, I runs
initialization explicitly, F5 saves to a new file, F9 restores, Escape closes.
Menus use Up/Down and Enter; map actions are suspended until a choice is made.
Turning alone does not execute an ECL movement event. Backward movement preserves
facing. Resizing letterboxes a 960x640 logical view; the window sleeps until an
input/window event rather than rendering continuously.

This is a **procedural preview**, not original graphics or a verified game UI.
It starts at (0,0), north, with empty state unless a matching engine save is
loaded. The map overview reveals all cells. Text currently uses SDL's limited
ASCII debug font, with bounded wrapping/history and a scrolling menu selection.
RmlUi, production fonts, original wall/sprite decoding, audio and QuickJS-NG
remain future work. Unsupported ECL/combat reports a diagnostic; changes made
before a VM failure retain the existing partial-progress semantics.

Reuse decision: use SDL3's zlib-licensed platform/input/rendering/font services
instead of implementing platform backends. Validated with SDL 3.2.28 at
`7f3ae3d57459e59943a4ecfefc8f6277ec6bf540`; no upstream source or artwork copied.
OpenXcom's GPL-3.0-or-later `SurfaceSet.h` at the previously researched revision
`4a4bae5fa2e5a09282928e84943fc84b093e34ff` describes indexed sprite surfaces;
its SDL1-era sprite architecture does not supply a GEO grid view. Retain it as
research only. See the attribution registry and SDL notice in `licenses/`.

With the renderer enabled, CTest covers cardinal/oblique/boundary rays, opposing
walls, invalid rays, and the real SDL event loop under its dummy/software driver:
movement, an ECL menu/state write, save/load and exit. Public fixtures are synthetic.
