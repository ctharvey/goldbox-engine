# Exploration renderer

Optional SDL3 window over the existing `ExplorationSession` and ECL host interface.
It draws a first-person view using independent grid-edge ray casting, a revealed
map overview, coordinates/facing, event text, and keyboard-driven menus. Geometry
checks walls on both sides of an edge and treats map bounds as solid, matching
movement. All nonzero wall IDs remain blocking; door flags are not interactive.
The renderer has no title checks and does not change decoder/VM state semantics.
In enhanced mode, choice menus use the right-hand overview panel, with two-line labels, leaving
portraits and cutscene artwork visible. Dialogue stays below the artwork.

`--display original` selects the profile's original presentation adapter; Pool
defaults to this mode. `--display enhanced` retains the larger scene and revealed
map overview. Put this switch after `--window`, before the resource arguments.
The two presentations use the same VM, rules, inputs and saves. Profiles without
an original adapter default to enhanced; an explicit unsupported original mode fails.

Pool's first original adapter renders a 320x200 logical canvas, an 88x88 scene at
(24,24), original symbol borders from `8X8D1.DAX` record 202, native font, party
panel, green dialogue below the scene and a separate command row. Profile fields
select resource names, records, regions and repeated border symbols; the renderer
contains no title checks. Long text pauses between pages rather than dropping lines.
Original mode loads its configured font/symbol archive beside the GEO archive.
Dialogue wraps at word boundaries, preserves explicit newlines and continues at
the current cursor across PRINT fragments. Leading spaces are suppressed; words
longer than the region are split safely. Engine notices replace the dialogue,
and enhanced-mode control hints are omitted. Party names use the configured
selected-member memory binding, independently of the loaded character: white
for selected, bright cyan otherwise, with green numeric fields.
Original dialogue interprets SSI `%0` through `%F` as inline EGA colors, with
`%0` selecting normal bright green and `%%` displaying a percent sign. Controls
take no character cells. Each displayed glyph retains its color through word
wrapping and paging; the current color survives PRINT and PRINT_CLEAR. A fresh
window starts in normal green. Unknown or incomplete controls remain literal.
This is a shared SSI presentation adapter, with no title checks or VM/save changes;
enhanced mode and the plain-text accumulator retain their existing behavior.

This is an initial display implementation, not full visual parity. Armor class
shows `?` because equipment is unfinished; the clock is omitted. Startup, creation
menus and statistics sheets still use the engine's interaction flow. Typewriter
timing, recorded-original color parity, exact command styling and monitor aspect correction
remain unverified. The configured border tiles matched a private DOS capture in
all 14,400 compared pixels. Original assets and reference screenshots stay private.
The word-boundary behavior was compared with private DOS opening captures and
manually exercised through creation paging and the installed campaign opening.
Synthetic tests cover PRINT continuation, page boundaries, blank lines, oversized
words and party selection. This does not establish full paragraph pixel parity,
original text reveal timing or character-status indicators. Inline colors have
synthetic coverage for all selectors, escaped/malformed controls, color changes
inside words, continuation and clears, plus an original-mode ECL/SDL smoke test.
They have not yet been compared against a color-changing DOS scene.

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
F10 restarts from the launch state in exploration, event menus, or startup. It
returns to the first title when `--startup` is active; otherwise it resets the
exploration preview without automatically initializing it. Current unsaved
progress, image/animation state and event menus are discarded; save files are
untouched. Original launch map, facing, memory and wall bindings are restored.
Turning alone does not execute an ECL movement event. Backward movement preserves
facing. Resizing letterboxes a 960x640 logical view; the window sleeps until an
input/window event rather than rendering continuously.

The default mode is a **procedural preview**; optional original wall artwork is
described below. Neither mode is a verified original-game UI.
The explicit preview starts at (0,0), north, with empty state unless a matching engine save is
loaded. With `--startup` and a campaign profile, Begin instead resolves the configured
initial map/program, retains the created party and runs its opening event. Pool's
program zero now runs through its city tour with original graphics. Engine saves
bind the active ECL program as well as the map; complete DOS gameplay parity remains
unverified. Without `--audio`, event sound requests are muted and startup continues.
The map overview reveals all cells. Text defaults to SDL's limited
ASCII debug font, with bounded wrapping/history and a scrolling menu selection.
RmlUi, production fonts, complete game audio and QuickJS-NG
remain future work. Unsupported ECL/combat reports a diagnostic; changes made
before a VM failure retain the existing partial-progress semantics.

### Speaker effects

With `--window`, add `--audio /private/START.EXE` before the six resource/save
arguments to opt into the profile's original speaker effects. F8 auditions
it on the title/menu screens or in exploration. Other title/creation keys keep
their existing behavior. Audio is off unless explicitly selected; an unavailable
device reports an error only when audio was requested. Repeated F8 replaces the
previous sound, F10 clears it, and closing the window releases the stream/device.

The Pool profile supplies resident segment/table/timer offsets and entry 8.
The adapter reads the user-owned EXEPACK executable in memory without executing
or modifying it. The effect subset accepts pitches, gate changes, durations,
signed sweeps, bounded counter loops and modulation. Note/music commands and
unsupported envelope fields fail explicitly. Pitch
and gate are decoded per original timer tick into a shared mono synthesizer,
then played through SDL3. This does not reproduce analog speaker filtering.

F8 remains an audition. The Pool profile additionally selects the verified
native ECL CALL sound route, its virtual selector word and configured effect IDs.
Supported requests now play automatically during ECL execution. New sessions
keep the normal initial state; ECL changes and saves preserve the selector word.
The adapter retains the native word-register write behavior even when WRITE
uses a byte destination operand. Unknown calls and missing resources report errors;
selector values with no sound route perform the original no-op. CALL operands
are raw word tokens, not memory reads. Byte-address and text variants are not
supported. No native machine code is executed by the engine.

Automatic controls for silence/enable/stop, overlapping effect scheduling,
Tandy sound and note/music decoding remain unfinished. The installed Steam
edition's observed startup is silent: a private sound-enabled DOSBox recording
and isolated title-routine trace found no startup audio. A synthetic speaker
tone confirmed that the recording path worked. This does not establish music
behavior in other editions or later game scenes.
There is no invented startup jingle. Synthetic tests cover decoding, duration,
priority, truncation/unsupported input, exact sample counts and dummy-device
playback. Private original-code traces check the effect decoder and call routing.

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

## Startup preview

With `--window`, insert `--startup /private/TITLE.DAX` alongside the other
resource options. The profile must explicitly list `startupTitleRecords` in
presentation order (1..64 byte-sized IDs). Pool selects `[1,2]` and a
`startupTitlePeriodMs` of 5000, corroborated against its original title routine.
Each title advances after five seconds or a key; both appear before a separate
menu. With an omitted/zero period, the legacy input-only presentation keeps the
last title as the menu backdrop. Positive periods require title records and are
bounded to 60000ms. No archive,
map, ECL or resource-group association is guessed. Existing launches without
`--startup` still open exploration directly.

Title records use the existing strict EGA17 decoder, one image per record,
centered on a 320x200 black canvas. The title adapter ignores EGA header
offsets for placement, matching the researched title renderer; the decoder
still preserves them. Unsupported composite
or multi-frame title records fail explicitly. The menu offers party management
when configured, Begin exploration, Load engine save, and Exit. Use Up/Down and
Enter or the displayed C/B/L/E shortcuts. A key that skips a title is consumed
there. F10 replays the titles; returning from party management or an error goes
straight to the menu. Escape or closing the window quits. The event loop sleeps
until input or the current title deadline; window events do not restart that
deadline. Original credit pages, animated title effects, borders and exact menu
layout remain unimplemented. Native character/save import menu items are not
offered as though they worked.

Start exploration executes the explicitly paired map/ECL initialization before
accepting movement. Successful initialization uses the existing image/wall/menu
host callbacks. Failure discards its partial state and presentation changes,
returns to the startup menu, and displays a diagnostic. Loading uses the same
bounded save reader and resource-identity/graphics checks as F9; it does not
rerun initialization. Saves are engine exploration saves, not DOS saves.

This is a startup shell for the exploration pilot. Full original character creation, character import, original save import,
campaign startup state and NEW_ECL transitions remain unfinished. The current
party creator is described below. The start action does not fabricate a party or claim that
a standalone map initialization reproduces the original New Game sequence.

## Party creation

Profiles can opt into `characterCreationRules` with an explicit relative JSON
path. Pool selects the separate `rules/adnd-first-edition-creation.json` profile.
With startup enabled, choose Create / manage party, then Create character.
Choose race and gender, a supported class or combination and alignment, review
or reroll scores, enter a name, choose a configured portrait, and confirm Add to party. Escape cancels the current
selection/draft; F10 resets the entire launch session. View and Remove character
operate on the current roster, with confirmation before removal.

When a race configures starting-age dice (including Pool's supported races),
class and alignment selection precede age and ability rolling. Age uses the shared session
RNG, remains fixed during score rerolls, appears in the summary, and survives
save/load. Complete rolls apply configured age modifiers before racial clamps
and class floors, and draw exceptional strength before later abilities. Numeric
generation matches the tested isolated original routines; live interaction,
cancellation and idle-loop draw timing remain unverified.

Pool uses its profile-selected compatibility RNG for creation and ECL RANDOM.
Each ability keeps the best of six 3d6 trials. The default startup seed is 1;
`--seed <decimal-or-0x-uint32>` selects another reproducible stream. Engine saves
preserve its current state. F10 returns to the launch seed, while loading a legacy
save with no RNG state requires a new seeded session before further rolls.
Rules-configured racial adjustments apply before class eligibility checks and
can produce legal scores of 19. Racial/gender bounds now clamp scores without
rerolling; exceptional strength rolls 1..100 and caps the result. Full original
creation-sequence parity and age
caps remain unverified; see [creation rules](../docs/character-creation.md).
Class choices account for optional creation floors. Complete rolls include those
floors in the score review; legacy creators apply them at completion. The final
confirmation preserves prepared percentiles. Loading a party preserves its abilities.

Starting gold, HP and spellbook capacities are rules-defined and preserved in
saves. Multi-class choices use their verified fixed ages and component rolls;
divine and arcane spell capacities remain separate. Portrait selection uses H
(next head), B (next body), K (keep), or Up/Down and Enter. It consumes no random
numbers. For Pool's complete player choices, supply the original `HEAD3.DAX` and
`BODY3.DAX` using the existing `--portraits` option together with `--images`.
Other HEAD/BODY banks may omit offered parts and fail explicitly. Character
View displays saved portraits when their resources are loaded. No original art
is stored inside the engine save or repository. Combat-icon selection is unfinished.

Pool allows up to six members and names up to 15 printable ASCII characters.
Names must be distinct ignoring case. At least one member is required before
starting through this startup menu. Failed map initialization preserves your
created roster. The roster appears beside the exploration map. F5 stores it in
the engine save; startup Load or F9 restores it with matching creation rules.
Older engine saves without a party remain readable and restore an empty roster.

This is a creation pilot: multi-classing, portraits, standalone character files,
DOS save import, HP/AC, equipment, spells, advancement and ECL character-memory
bindings are not implemented. Creating a party does not yet enable original
character-dependent scripts or combat. See [creation rules](../docs/character-creation.md).
