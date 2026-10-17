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
combat palette swaps or count-plus-one variants. Those need separate adapters.

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

The caller supplies the map/ECL and wall-bank relationship. ECL-driven bank
selection/switching, imported backdrops, sprites, doors and original-game scene
parity are **not implemented or verified**. Save identity remains about engine
content/state; an explicit visual bank selection is not a parity baseline.

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
