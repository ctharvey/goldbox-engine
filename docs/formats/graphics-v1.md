# SSI EGA wall graphics pilot

Status: strict decoder and native wall compositor, checked against one user-owned
Steam Pool of Radiance installation. This is not complete graphics compatibility.

An unpacked EGA record contains a 17-byte header: little-endian height at 0,
width in eight-pixel units at 2, X/Y offsets in eight-pixel units at 4/6,
image count at 8, and eight CGA mapping bytes at 9. Images follow consecutively,
row-major, with the high nibble as the left pixel and low nibble as the right.
Each pixel selects the standard 16-color EGA palette; color 6 is brown.
The CGA mapping is retained as metadata, not interpreted as an EGA palette.

Dimensions must be nonzero and at most 320x200; count must be nonzero and the
payload length must match exactly. The decoder does not guess VGA, monochrome,
palette swaps or count-plus-one variants. Explicit mono/sequence/palette adapters
are described below; VGA and count-plus-one variants remain unsupported.

`WALLDEF` contains one or more 156-byte definitions. Each byte selects an 8x8
tile. Views are row-major tile grids in this order:

| View | Columns x rows | Start byte |
| --- | --- | ---: |
| Far front | 1x2 | 0 |
| Far left/right | 1x4 each | 2 / 6 |
| Medium front | 3x4 | 10 |
| Medium left/right | 2x8 each | 22 / 38 |
| Close front | 7x8 | 54 |
| Close left/right | 2x11 each | 110 / 132 |
| Far filler | 1x2 | 154 |

The shared-tile adapter starts with a synthetic transparent tile at index 0,
then images from shared record 203, then the selected bank's tiles. Five-wall
records use a matching tile record. Combined 10/15-wall records additionally
use records `10*id+1..3` when present; record 0 uses 101..103. Missing single-bank
tiles, invalid references, non-8x8 images, offsets, more than 256 symbols and
malformed definitions fail explicitly. Self-contained 255/256-tile layouts are
not supported by this adapter. The generic EGA decoder remains separate.

Three explicit bank selectors populate GEO wall IDs 1..5, 6..10 and 11..15.
255 leaves a bank absent. 127 continues with the next five definitions of the
preceding selected record; as the first selector it means record 0. Other
missing record IDs fail; no silent fallback to unrelated artwork occurs.

The native view is 88x88 pixels. Front views occupy 56x64, 24x32 and 8x16
perspective planes. Side images bridge them; far fillers close adjoining
distant fronts. The compositor draws far-to-near, clips to the viewport, and
uses palette index 13 as transparent. It samples GEO relative to the current
position/facing and checks opposite-side walls when the near side is absent.
It does not mutate exploration or VM state. SDL uploads the indexed result as
RGBA and scales it with nearest-neighbor sampling, preserving its square aspect.

The caller supplies the map/ECL and initial wall-bank relationship. The selected
SSI dialect now supports ECL-driven bank switching through LOAD_AREA_DECO, using
those same explicit archives. It validates all referenced map walls before
publishing replacement graphics and stores selected IDs as optional `wallBanks`
save metadata. Window restore rebinds before publishing restored state. Graphics
archive fingerprints remain outside prototype save identity. Automatic archive
group/map loading, image backdrops, interactive doors and original-game scene
parity remain unimplemented or unverified; explicit banks are not a parity baseline.

Reuse: the EGA header/nibble decoder is adapted from MIT Gold Box Explorer
`EgaBlock.cs` and `EgaBlockSpecification.cs`, revision
`eac30abaa6ee66aea6f5d65ebe6d676b10015a8f`, Bil Simser, 2018. Its notice is
preserved, with exact paths/modifications in `attribution/sources.json`.
System.Drawing, filename heuristics and permissive size checks were removed.

SSI Engine at `51d4b6d409fa798754f5ff2ccb249f3c0b6d029b` was a research-only
cross-check for EGA records, shared/additional symbols, continuation selectors,
transparency, view geometry and map sampling. No GPL-3.0-only Java implementation
was ported. Gold Box Explorer's wall assembler cites Simeon Pilgrim for part of
its layout; that source was not copied or ported. The C++ assembler/compositor
uses independent bounded loops over the format facts above.

Public CTest uses synthetic bytes only. For a read-only local resource check:

```sh
timeout 30 ./build/tests/goldbox_graphics_tests \
  /private/8X8D1.DAX /private/WALLDEF1.DAX /private/8X8D1.DAX
```

It prints counts and per-record diagnostics, not pixels, strings or extracted
content. Non-EGA records are counted as unsupported; wall failures return an
error. Keep installation tests and rendered original artwork outside the repo.

## Monochrome tiles and picture/sprite sequences

`decode_mono8_tiles` explicitly decodes headerless 8x8 tiles: eight bytes per
tile, one byte per row, most-significant bit on the left, black index 0 and
white index 15. Empty, partial and over-256-tile strips are rejected. It is
never used as an automatic fallback for a failed EGA decode. In the inspected
installation, `8X8D1.DAX` record 201 contains 177 such tiles. The optional renderer
font adapter now maps the first 64 tiles in explicit SSI character order:
ASCII `@.._` uses 0..31, while space through `?` uses 32..63. UI lowercase folds
to uppercase and unsupported bytes use `?`. Extended glyphs remain unidentified.
This mapping is a separate rendering adapter, not part of monochrome decoding.

The font atlas is 128x32 pixels, sixteen columns of eight-pixel glyphs in four
rows. Input tiles must be 8x8 with exactly 64 binary palette pixels (0 or 15),
and the strip must have 64..256 tiles. Zero becomes transparent; white pixels
receive the current UI tint. Original and enhanced presentations share this
atlas, with separate wrapping regions. See `--font` in the runtime README; no
font artwork is redistributed.
Four imported letter masks (256 pixels) matched a saved local DOS screenshot
exactly, including detached strokes in the source font. This narrow check does
not verify every glyph or all scaling behavior.

SSI Engine's pinned Pool properties, `MonocromeSymbols.java`,
`GoldboxString.java`, `CustomGoldboxString.java`, `AbstractRenderer.java` and
`UIResourceManager.java` were consulted for font selection, character order and
foreground recoloring. This adapter is independently implemented; no Java was
copied or translated. Synthetic tests cover character examples, atlas placement,
invalid fonts and SDL menu/text rendering; local assets are checked separately.

`decode_ega_sequence` takes an explicit independent or XOR-first mode. A leading
byte gives 1..8 frames, each with this 21-byte header followed by packed nibbles:

| Relative byte | Value |
| --- | --- |
| 0..3 | Little-endian raw delay; timing unit unverified |
| 4..5 | Height in pixels |
| 6..7 | Width in eight-pixel units |
| 8..9 / 10..11 | X/Y offsets in eight-pixel units |
| 12 | Preserved raw byte; meaning not assumed |
| 13..20 | Preserved CGA mapping |

Dimensions are bounded to 320x200 and offsets must fit 16-bit pixel coordinates.
Independent frames may differ in size. XOR-first frames must share dimensions;
every later packed frame is XORed with the **original first frame**, not the
preceding decoded frame. Decoding does not mutate source bytes and rejects
truncated headers/pixels or trailing data rather than returning partial output.
SPRIT frames are not automatically treated as animation. The optional image host
now supports explicit distance selection and ECL picture/sprite callbacks; see
the runtime README for profile selection and supported/unsupported operations.

Palette policy is explicit and separate from indexed decoding. Standard EGA is
opaque. Sprite policy makes index 0 transparent and maps index 13 to opaque
black. Combat policy makes index 0 transparent and maps index 8 to opaque black.
The standard policy is the default, preserving wall-renderer behavior.

These additions adapt MIT Gold Box Explorer `MonoBlock.cs`, `EgaSpriteBlock.cs`
and their specifications at the same pinned revision, preserving the existing
license and attribution. SSI Engine `EGADependentImages.java` independently
confirms first-frame XOR and per-frame metadata as a research reference only;
no Java implementation was ported. Filename-based mode/palette selection and
source-buffer mutation were removed; bounds and exact-consumption tests added.

Opt-in read-only local validation examples (public CTest remains synthetic):

```sh
timeout 30 ./build/tests/goldbox_image_sequence_tests mono8 /private/8X8D1.DAX 201
timeout 30 ./build/tests/goldbox_image_sequence_tests xor-first /private/PIC1.DAX
timeout 30 ./build/tests/goldbox_image_sequence_tests independent /private/SPRIT1.DAX
timeout 30 ./build/tests/goldbox_image_sequence_tests ega /private/CPIC1.DAX
```

The validator prints record/frame counts and diagnostics only. This does not
export images, interpret ECL selection or establish original-game scene parity.

## ECL image host integration

The optional `imageDialect` profile field enables `ssi-images-3-v1`; absent or
`disabled` preserves unsupported VM image behavior. Pool's profile selects it;
Curse remains disabled. Opcode 0x0c must have three operands, with conflicting
or duplicate definitions rejected. Host callbacks receive typed numeric IDs,
distance and replacement-picture values; the VM never opens archives or calls
SDL. Missing hosts stop with diagnostics and do not emit new JSONL event types.

The independent `ImagePresentation` component owns transient decoded frames.
PICTURE starts at the first XOR-decoded frame, 255 clears; SPRITE_START selects an
independent frame at distance 0..2, clamped to blocking GEO edges. ADVANCE steps
toward zero, OFF clears both picture and sprite. Standard/sprite palettes are
applied at upload, honoring native offsets. The SDL loop animates pictures while idle and in menus using bounded event waits.
Optional profile `picturePeriodMs` selects a fixed interval (0 disables, maximum
60000 ms). Missing fields preserve static behavior. Pool selects a provisional
400 ms policy based on SSI Engine ClassicMode, not verified original timing.
The raw frame delay remains uninterpreted. Sprite distance frames and composed
portraits never animate. Presentation accepts explicit elapsed time, preserves
fractional intervals, skips missed frames without catch-up loops, and resets
on successful replacement/clear. Failed resource loads retain the prior timer.
There are no invented inter-instruction delays.
Timed replacement-picture requests are explicitly unsupported. Images are not
part of the prototype save format and are cleared on restore.

SSI Engine's `VirtualMachine.java`, `Engine.java`, `ClassicMode.java`,
`RendererState.java` and Pool properties at the pinned revision were consulted
for callback arity/argument order, clear semantics, distance indexing and timed
replacement behavior. They are research references only; no Java code was
ported. The C++ host/state/SDL integration is independently implemented.

## Color dungeon background

The optional profile `colorBackdrop` object contains exactly `skyColorAddresses`,
two unsigned 16-bit ECL memory addresses ordered outdoors/indoors. It opts the
original-wall compositor into SSI COLOR mode. Pool configures 0x49FD/0x49FE;
other profiles remain unchanged. No title name is checked in the renderer.

The current GEO cell's high event bit selects indoors when set. The selected
memory byte must be 0..15 and indexes EGA colors
`0,15,4,11,13,2,9,14,0,15,4,11,13,2,9,14`.
The native 88x88 background has sky at rows 0..42, white at row 43, light gray
at rows 44..45, and brown at rows 46..87. Walls are composited over it; sprites
retain their separate transparency policy. The renderer reads these values
without changing game state. ECL changes appear on the next draw, and the
existing memory save/restore preserves them. Script overrides of the square-info
memory byte are not used to select indoors: the current imported GEO cell is
the renderer's context, including before any event has run.

This is a researched color mode, not a new image decoder or proof of original
scene parity. Image backdrops, lighting/time-of-day behavior and automatic wall
bank selection remain unfinished. The default procedural ray-cast preview is
unchanged; this mode applies when original walls are selected.

SSI Engine's pinned Pool properties, VirtualMemory, RendererState and
DungeonRenderer were consulted for addresses, indoor selection, palette indices
and band dimensions. The C++ rendering loops are independent; no Java source was
copied or translated. Tests use synthetic maps, memory and ECL only.

## Separate body/head portraits

BODY and HEAD resources in the inspected installation use the existing strict
17-byte EGA decoder. `compose_ega_portrait` is a separate SSI composition adapter
that returns one normalized `IndexedImage`. Callers supply canvas dimensions
and fallback head/body coordinates through `EgaPortraitLayout`; no game name,
filename, record pairing or 88x88 geometry is inferred by the adapter.

Each part must contain exactly one image. If either part has nonzero embedded
offsets, both parts use their respective embedded offsets. Otherwise both use
the explicit fallback coordinates. The head is painted first, then the body,
opaquely: black and magenta remain visible pixels. Uncovered canvas pixels are
black. Dimensions are bounded to 320x200; malformed pixels, missing/multiple
frames and any part extending outside the canvas fail instead of being clipped.
Inputs are not modified. Other formats or placement policies need a different
adapter rather than a title-specific branch in this one.

An opt-in local validation preset checks candidate pairs in an 88x88 canvas
with head at (0,0), body at (0,40), matching the researched SSI placement:

```sh
timeout 30 ./build/tests/goldbox_image_sequence_tests portrait /private/HEAD1.DAX /private/BODY1.DAX
```

The output contains only counts. This tests structural fit, not which heads and
bodies an original event actually pairs. Opt-in ECL PICTURE integration now uses
the profile's head-ID address and caller-selected archives; campaign archive
binding and original-event pairing remain unverified. TITLE, combat and other static graphics
use the existing explicit `ega` validation mode; FINAL5 contains both static
images and picture sequences, so it must not be assigned one guessed decoder.

SSI Engine's pinned `ClassicMode.java`, `Palette.java`, `UIResourceManager.java`
and `UIScaler.java` were consulted for layering, opaque colors and placement.
The C++ compositor was independently implemented; no Java was copied or ported.

## Startup title adapter

`load_startup_screens` reads explicitly ordered EGA17 DAX records supplied by
`startupTitleRecords`; each must contain one image fitting a 320x200 canvas. The title adapter centers
the image and pads uncovered pixels with opaque black, independently of EGA
header offsets. Both local original titles are 320x200 with a nonzero X field;
SSI Engine TitleRenderer likewise centers titles rather than using that field. Rendering
uses the standard EGA palette and nearest-neighbor scaling independently of the
88x88 event-image host. Title ordering and selection belong to the adapter;
the shared renderer contains no title-specific filename or game-ID branches.
Pool's records 1 and 2 were selected from pinned SSI Engine title configuration.
This does not establish original title timing, menu geometry or campaign parity.
