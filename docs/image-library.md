# Replacement image library

The optional `images` array in the existing schema-version-1 `mod.json` binds
independently supplied artwork to original resource identities. Existing journal
mods and their dependency/priority ordering are unchanged. This is an additive
manifest field, not the proposed master-campaign schema version 2.

```json
{
  "schemaVersion": 1,
  "id": "example.artwork",
  "version": "1.0.0",
  "images": [
    {
      "game": "example.game",
      "archive": "PIC3.DAX",
      "kind": "picture",
      "record": 7,
      "frame": 0,
      "file": "art/new-picture.bmp"
    }
  ]
}
```

This example is synthetic. It does not identify or approve any actual game art.
Keep original and derived copyrighted artwork outside the public repository.

Put each pack in a direct child directory of a local mods directory. Launch the
window with `--mods <directory>` before the profile/resource arguments, alongside
the existing `--images` options. The window resolves image-only packs using the
existing mod resolver; journal patches require imported journal content and will
be rejected there if they target absent journals. `goldbox_cli` retains its journal
workflow. Packs need no new master/campaign manifest or scripting runtime.

The game key must match the selected profile's `id`. Archive keys are basenames
with ASCII case folding; record IDs are 0..254 (255 clears the image), and frames
are zero-based, 0..255. `kind` is `picture`, `sprite` or `portrait`. The full source key
must match: there are no wildcard or numeric-ID-only fallbacks. Different banks,
games, kinds and animation frames cannot alias. This version does not fingerprint
individual editions: pack authors must validate their chosen profile/resources.

Adapters attach the game and archive identity to `ImageArchives`. `ImagePresentation`
tracks record/frame identities independently of decoded pixels and VM state.
Picture animation selects a binding per frame. Sprite frames correspond to the
original distance indices; timed replacement switches to the replacement picture's
identity. A composed portrait uses a separate head/body key, described below.

## Composed character portraits

A `portrait` binding replaces one complete composition, rather than replacing
heads or bodies independently. The primary `archive`/`record` identifies the head;
`bodyArchive`/`bodyRecord` identifies the body. Both archive basenames are normalized
with ASCII case folding. `frame` must be zero. For example:

```json
{
  "game": "example.game",
  "archive": "HEAD3.DAX",
  "kind": "portrait",
  "record": 7,
  "frame": 0,
  "bodyArchive": "BODY3.DAX",
  "bodyRecord": 9,
  "file": "art/head7-body9.bmp"
}
```

All IDs here are synthetic examples. Body fields are required for portraits and
rejected for pictures/sprites; missing parts, nonzero frames and record 255 are
invalid. Use raw archive record IDs, not the picker's one-based display numbers.
Each head/body pair needs an explicit binding. A different head, body or
archive does not borrow a nearby portrait. Unmapped pairs display their original
composition, so partially populated packs remain usable.

The existing game adapter supplies head/body archive identities for startup and
campaign resources. Creation previews, saved-character previews and ECL portraits
share the same lookup. Timed sprite-to-portrait transitions preserve the composed
identity. Selections and saves still contain original head/body IDs; installing or
removing a pack changes presentation only. F11 compares enhanced artwork with the
original composition without changing the chosen portrait. No original assets are
overwritten. Whole-portrait replacement does not add new head/body options, change
selection rules or automatically generate matching combinations.

Ordered mods build a shared, immutable `ImageLibrary`. Later mods win; dependency
order takes precedence over priority, matching journal overlays. Conflicts report
the two mod IDs and source key. Duplicate keys inside one manifest are rejected.
Bindings for other games remain isolated and cannot affect the selected game.

Enhanced mode renders a matched full-color BMP into the original frame's rectangle,
preserving its offsets, aspect placement, distance geometry and timing. BMP dimensions
may be 1..2048 per axis and the file must be at most 32 MiB. Linear filtering supports
higher-resolution artwork. Missing bindings use original art. Corrupt or missing
replacement files at runtime produce an SDL diagnostic and fall back to original
art. Original mode always renders the original indexed image. F11 switches the
already prepared textures without re-running ECL, changing saves or resetting animation.

Only relative `.bmp` paths inside the pack are accepted; traversal, absolute paths,
symlinked components and missing/oversized files fail discovery. Files are data only.
SDL's existing BMP decoder is reused; no image decoder implementation was copied.

Current limits: no PNG/JPEG loader, automatic art matching, edition fingerprints,
title-screen replacements, wall-piece replacements, separate head/body layer replacements,
new animation timing, new logical dimensions or pack approval UI. Existing PNG
modernization studies have not been bound automatically or marked production-ready.
The synthetic library tests cover identities, ordering, conflicts, paths, image
loading and pixel colors; renderer tests cover live original/enhanced cutover.
