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

The default mode is a **procedural preview**; optional original wall artwork is
described below. Neither mode is a verified original-game UI.
It starts at (0,0), north, with empty state unless a matching engine save is
loaded. The map overview reveals all cells. Text defaults to SDL's limited
ASCII debug font, with bounded wrapping/history and a scrolling menu selection.
RmlUi, production fonts, audio and QuickJS-NG
remain future work. Unsupported ECL/combat reports a diagnostic; changes made
before a VM failure retain the existing partial-progress semantics.

To use original wall artwork instead of procedural walls, insert this before
the six profile/map/ECL/save arguments, after `--window`:

```text
--walls /private/WALLDEF1.DAX /private/8X8D1.DAX /private/8X8D1.DAX 1 3 5
```

The three paths are definition, bank tiles and shared tiles archives. The three
selectors explicitly bind five walls each to GEO IDs 1..5,6..10,11..15. This
example selects artwork, **not a verified map association**. Use 255 for an
absent bank; 127 continues the previous combined record (first 127 selects
record 0). All nonzero wall IDs used by the map must have a selected bank.
Missing records/tiles and unsupported layouts fail before opening the window.

This mode composes the original preprojected pieces in a square 88x88 native
view rather than distorting them onto ray-cast surfaces. Bright magenta is
transparent. Profiles with `colorBackdrop` use the researched SSI color background
in this mode: 43 sky rows, one white horizon row, two light-gray rows and 42 brown
floor rows. Pool selects outdoors/indoors sky values from ECL memory at 0x49FD/0x49FE;
the current GEO cell's high event bit selects indoors. Changes appear on the next
draw and existing saves preserve those memory values. Invalid sky selectors fail
explicitly. Profiles without this configuration keep the placeholder background.
This is an independently implemented color mode, not an imported backdrop image
or verified original scene. Supported ECL LOAD_AREA_DECO calls can now select
different banks from these explicitly supplied archives.
See [graphics format and limits](../docs/formats/graphics-v1.md).

For ECL-selected pictures/sprites, also insert `--images /private/PIC1.DAX
/private/SPRIT1.DAX` before the six core arguments. `--walls` and `--images`
may appear in either order. The caller explicitly selects the current archives;
the engine does not guess a disk/group from a picture ID. Pool's profile opts
into `imageDialect: ssi-images-3-v1`; absent/disabled profiles retain unsupported
image behavior, including the existing headless/harness diagnostics.

Supported callbacks are PICTURE (0x0e), three-operand SPRITE_START (0x0c),
SPRITE_ADVANCE (0x0d) and SPRITE_OFF (0x31). Picture 255 or SPRITE_OFF clears
both displays. Pictures start at frame zero, while sprites use the requested distance
frame and move one frame nearer on advance. Walls and map bounds clamp distance.
Missing records, invalid distances and images outside the 88x88 view stop the
event explicitly, retaining any earlier VM progress. Failed image-resource loads
preserve the preceding image state. Optional profile `picturePeriodMs` animates
normal pictures while idle and in menus; absent/zero keeps them static. Pool uses
a provisional 400 ms interval from SSI Engine research. Sprite distance frames
and portraits remain static, and raw frame delays are not interpreted. Original
timing parity, inter-command delays and timed sprite-to-picture replacement
are not implemented; non-255 replacement
requests are rejected. Four-operand sprite dialects remain unsupported.

Image presentation lives outside ECL memory. Saves preserve existing engine
state; images are transient and cleared after loading a save. This does not
establish original scene/timing parity or full original-event compatibility.

For the original monochrome font, insert `--font /private/8X8D1.DAX 201` before
the six resource arguments. It can be combined with `--walls` and `--images`
in any order. The record must contain 64..256 complete monochrome 8x8 tiles.
The SSI character-order adapter uses tiles 0..63 for ASCII space through
underscore: `@.._` maps to tiles 0..31, and space through `?` to tiles 32..63.
Lowercase UI text is displayed in uppercase; other bytes display `?`. This
does not interpret extended glyphs, runes or other font orders.

All labels, menu choices and event text use the selected font at eight logical
pixels per character. A single transparent white atlas is tinted to the current
UI text color and scaled with nearest-neighbor sampling. Glyphs are decoded
read-only from the supplied archive; no artwork is bundled. Font selection is
explicit, does not affect game state, and is not a recreation of the original
screen layout or a visual-parity claim.

Add `--portraits /private/HEAD1.DAX /private/BODY1.DAX` with `--images` to enable
profile-driven PICTURE portrait selection. The profile must declare `portrait`
with `headIdAddress`, canvas `width`/`height`, `headOffset` and `bodyOffset` pairs.
Pool declares head ID at 0x6DE1 and an 88x88 canvas with body fallback (0,40).
PICTURE reads that byte at the call: 255 selects a normal PIC record; otherwise
it selects HEAD by that byte and BODY by the PICTURE operand. PICTURE 255 still
clears before any archive lookup. Missing parts stop the event without replacing
the preceding image. This is opt-in; without `--portraits` the picture-only pilot
retains its previous behavior and does not claim original portrait routing.
No initial head ID is invented: empty VM memory remains empty. Original startup
must establish correct state, or an authored test must set the head byte explicitly.
Archive groups remain caller-selected. Timed sprite/portrait replacement and
campaign-driven archive switching are not implemented.

LOAD_AREA_DECO (0x37) accepts three byte-valued bank selectors through the
selected SSI image dialect. With `--walls`, the host reloads them from the same
definition/tile/shared archives and validates every wall used by the current
map before publishing the replacement. Missing banks, bad operands and missing
hosts stop explicitly. Failed resource selection preserves the preceding walls.
The command does not select another archive group or load another GEO map.

Engine saves optionally store `wallBanks`; restoring in the window rebuilds and
validates those banks before publishing game state. A failed rebind changes
neither game state nor walls. The engine can still read older saves without the
field; a window with an active bank selection rejects such a visual restore
instead of guessing. Image/portrait overlays remain transient and are cleared.
Graphics archive fingerprints are not yet part of save identity: this is record
selection persistence, not proof that the same artwork or original scene is loaded.

The selected `areaDialect` can execute LOAD_AREA_MAP through the shared session
adapter in terminal, harness and window modes. It loads supported dungeon records
from the explicitly supplied GEO archive, preserving the current ECL program.
Missing records, unsupported scene modes and invalid script positions stop with
diagnostics. The CLI does not guess archive groups or change ECL records.
Map-aware saves bind the selected map to that archive snapshot's change detector;
restore rebuilds geometry before the window rebinds saved walls. Older single-map
saves remain readable. See the ECL format document for supported selector modes
and the non-cryptographic prototype identity limitation.

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
