# Local Pool of Radiance validation - 2026-10-15

## Scope

Authorized Steam installation, Forgotten Realms The Archives - Collection Two,
`games/POOLRAD/GAME/POOLRAD`. Original assets were read in place. Reports,
captured event text and prototype saves remain outside this repository in the
developer's ignored workspace cache. This document contains only metadata and
diagnostics. The Pool and Curse profiles retain `validationStatus: unverified`.

## Installation inventory (2026-10-12)

The read-only installation scanner inventoried 168 files. All 113 DAX archives,
containing 1,245 records, passed index and bounded RLE extraction. Eight GEO
archives contained 29 records accepted by the fixed 16x16 layout decoder;
eight ECL archives contained 29 records with valid five-instruction lifecycle
headers. No decoder failures were reported. These checks do not validate every
ECL instruction, wall/door meaning, rendering or original-game behavior.

The comparison with Gold Box Explorer and SSI Engine identified an incorrect
DAX header interpretation: the first word is the index byte count, excluding
the leading word. The corrected reader also retains the old, unambiguous
synthetic absolute-offset convention. The bounded RLE implementation was kept.

## Original event replay (2026-10-15)

| Resource | Size (bytes) | SHA-256 |
| --- | ---: | --- |
| GEO1.DAX | 2619 | `e6bcc8c5ea4b101497353ec75386b96ad6368447a6669f4f82ca7dac3c1e204a` |
| ECL1.DAX | 14564 | `b1d08075869ef58ae4f08b53d2059b4ea60f0259c35fb6fda44e1c0737c3a4b9` |

The caller explicitly selected GEO record 18 and ECL record 18. This association
was cross-checked by decoding the ECL initialization entry's LOAD_AREA_MAP
instruction referencing map 18; matching record IDs were not used as a general
pairing rule. The runtime remains configured through the profile's code base
`0x9900` and position/square-info addresses.

The terminal started at `(0,0)` with zero-initialized prototype state. Moving
east reached `(0,1)` and displayed an original menu. Fresh runs selecting indices
0 and 1 each exited the original event after 15 decoded steps without an
unsupported-opcode diagnostic. Both runs saved state and restored `(0,1)` in a
fresh process. Two persisted memory bytes differed between the menu outcomes,
including a flag changing from zero to one. No original text, menu labels, map
geometry or extracted bytecode is included here.

The replay ran each process with a 60-second timeout, a 10,000-step VM budget
and a maximum of 64 subroutine frames. The build used `scripts/wsl-build.sh`,
which locks, builds with `-j2`, and bounds build/test duration. A Windows CRLF
input issue found during replay was fixed and covered by the terminal test.

## Public verification and limits

The Debug C++ build and 13 synthetic CTest suites passed on WSL Ubuntu with
GCC 11 and Boost 1.83. Runtime/terminal tests cover loops, stack depth, branch
conditions, typed memory, indexed reads, output, host menus, dispatch selection,
collision, source mismatch, state round trips and CRLF input. Attribution and
asset hygiene are checked separately. Native Windows C++ compilation and the
unchanged TypeScript SDK were not run during this milestone.

This demonstrates a narrow imported-map/original-event/state slice. It does
not establish correct campaign initialization, party state, original save
compatibility, reference-game trace parity, all event selectors, doors,
transitions, combat, journal dispatch, modded exploration or another game/version.
Some original routes still stop on unsupported opcodes or depend on state that
the static resolver cannot know. Those routes remain explicit diagnostics.

## Wall graphics validation (2026-10-16)

Read-only checks used all eight installed `8X8D`/`WALLDEF` archive pairs, with
shared tiles from `8X8D1.DAX` record 203. All 38 definition records assembled:
210 walls / 2,100 distance-and-angle views. The strict EGA decoder accepted
3,016 images across these archives, including repeated artwork stored in
multiple archives. `8X8D1.DAX` record 201 is not supported by this EGA adapter
and reports `Unsupported SSI EGA dimensions/count`; it is not needed for the
validated wall assembly. No wall records failed after split-bank support.

| Group | EGA images | Definition records | Wall views |
| --- | ---: | ---: | ---: |
| 1 | 286 | 3 | 150 |
| 2 | 420 | 6 | 300 |
| 3 | 420 | 4 | 300 |
| 4 | 490 | 6 | 350 |
| 5 | 210 | 2 | 150 |
| 6 | 350 | 5 | 250 |
| 7 | 350 | 5 | 250 |
| 8 | 490 | 7 | 350 |

Public-safe input fingerprints (no decoded pixels or game content):

| Archive | Bytes | SHA-256 |
| --- | ---: | --- |
| 8X8D1.DAX | 9711 | `cce9313f23a600200fec60b5d46cd08aa9535a6143eee5d5d768a1d4e0415021` |
| 8X8D2.DAX | 12190 | `418fc6cfa448646d3f0b590b53d1c926a0b308a521260469c0d06d05a7869cf4` |
| 8X8D3.DAX | 12967 | `379aea0e70fe2f74512a2845176b3fb65d2d1f883abf661e7c8a9813e319c223` |
| 8X8D4.DAX | 14343 | `02dbb15f007e8609d365dcbf47b1ae850bff577569e5b844931f9312569fa032` |
| 8X8D5.DAX | 5847 | `90698c59d65d38b5cd4fb80c7ec8e9ad90f7ecce087d66dc47f5cf5b56ff3a2f` |
| 8X8D6.DAX | 10285 | `f24577eec17af3bbfe0ee8517391157b4b655a35082429d2618de223c2021083` |
| 8X8D7.DAX | 9820 | `d8086eb6b52f8a0b04156d67d2cfc16d4aefc571aad440285a55a601236bf0a2` |
| 8X8D8.DAX | 14510 | `05590e593534715f26790a82f9dbaf1ee73e29e7b71a038ee039a3d64651c4d5` |
| WALLDEF1.DAX | 2147 | `7a6102b9a38b2400f38d3c3d66460ab889ab7b35bc684c9ac282b80de028ec5a` |
| WALLDEF2.DAX | 4044 | `bfbd4106d6207ea22d0e5862074ddef582ae24772656b9e47251f1b00933995d` |
| WALLDEF3.DAX | 4289 | `86a8b136b0254398c03c31fdf13ebe2d23cf571d473a4f38718578f1479f661e` |
| WALLDEF4.DAX | 4956 | `e1729459c867048b25553beea4681808c958a5501291b9bf931def3463fe70ed` |
| WALLDEF5.DAX | 2121 | `26d2dbab7054fc22fb4ce44019b6e5cb0d981a05d082a4a22b085dbafd0b4802` |
| WALLDEF6.DAX | 3492 | `d1b1ebc727dca3e65d86d4ccbf7f461dca59b64cce173d51bd46f024e5121010` |
| WALLDEF7.DAX | 3176 | `cbb055cf16e516cdfc58cae66fdff21ceaa2f33adbe17321fb57c47f589238aa` |
| WALLDEF8.DAX | 4998 | `1fc950ad158ef1542f13efc1d7ace02e61ba8e49b8042850098386648285bca2` |

The native compositor has synthetic pixel checks for rotations, transparency,
near-over-far ordering and unresolved banks, plus SDL software upload/event tests.
The visible SDL window was also exercised using GEO/ECL record 18 from group 1,
explicit artwork banks 1/3/5 and position (0,0): turning north to east changed
the composed wall view. The private screenshot is excluded from the repository.
The window can explicitly bind supplied banks to GEO wall IDs; this is not an
automatically verified map/ECL/artwork relationship. ECL-selected bank changes,
original backdrops and an original-game scene at matching position/facing remain
unverified. No proprietary images, map bytes or extracted content are included
in this report or the public tests.

## Monochrome, picture and sprite validation (2026-10-16)

Explicit-format read-only checks accepted all selected records: 177 mono tiles
from `8X8D1.DAX` record 201; 325 frames from 92 PIC records; 258 frames from
88 SPRIT records; 218 images from 218 CPIC records. The earlier EGA-only check
correctly rejected record 201; the new explicit mono adapter now handles it.
No fallback-by-length was introduced. These counts include repeated resources
stored in multiple archives and do not imply unique artworks or scene parity.

PIC was selected as XOR-first, SPRIT as independent frames, and CPIC as the
existing 17-byte EGA format. All 25 selected archive/format checks returned zero.
Palette remapping is tested synthetically; these checks establish decoding and
metadata validity, not original display/color/timing equivalence. Glyph identity,
ECL picture/sprite selection, distance/animation semantics and UI rendering of
these additional image families remain unverified or unimplemented.

| Archive | Format | Records | Frames/tiles | Bytes | SHA-256 |
| --- | --- | ---: | ---: | ---: | --- |
| 8X8D1.DAX (201) | mono8 | 1 | 177 | 9711 | `cce9313f23a600200fec60b5d46cd08aa9535a6143eee5d5d768a1d4e0415021` |
| PIC1.DAX | xor-first | 7 | 26 | 36273 | `ff636705387218dcca9b8f559e12223d2f1ab1ca45c56fcc5e11b6fd67f0891d` |
| PIC2.DAX | xor-first | 13 | 48 | 71533 | `aa0f7e67e65b362e19056a6d869b9238e2d2815404c508c7e93f72f0abef0f21` |
| PIC3.DAX | xor-first | 3 | 10 | 15316 | `44fc6edc72bad4a62ac4b5c45ca90ff99d59370044342c0bb2e3e8b4809d028d` |
| PIC4.DAX | xor-first | 17 | 68 | 91885 | `346d2f48680a7693137c2a2366903fbe59ce1de94f7b02b2654a81b7e2d6cdc8` |
| PIC5.DAX | xor-first | 10 | 34 | 46750 | `522f71a6de97a59e69e11985c91b5cae279961c30648b26286036a14f1710227` |
| PIC6.DAX | xor-first | 14 | 45 | 66888 | `2c93668dc322c4823b20d7be7ae51653f0c03f31fd1f94dc8c109541f71d0234` |
| PIC7.DAX | xor-first | 14 | 52 | 67804 | `4171a7d575b562ccb9bbe28fa1e4d0fa3250625744a418931d0721a07a3b12cd` |
| PIC8.DAX | xor-first | 14 | 42 | 67227 | `760cf9042110bbf41e60bb4b0cf76635398751ceaf1ce693dc5e39ef92d8200a` |
| SPRIT1.DAX | independent | 5 | 15 | 5613 | `a7a822a1fb883aa619881e0cfca0b8534b29ee07786b5eab722c3e7163bde3a7` |
| SPRIT2.DAX | independent | 15 | 45 | 16768 | `413c1dd3a1a787122396aab3b9451b4c4058ad1a0612309945cd6064fce8585e` |
| SPRIT3.DAX | independent | 7 | 21 | 6124 | `890f70ff6fb131f03abca845f5468b601fda3c1b5daf9dc163b72dad6bf1f9d0` |
| SPRIT4.DAX | independent | 12 | 36 | 11176 | `3a32d1a894ab35c5a6541d9cd686ee61ef6a18bb7a1a5581132e181c3c10aef1` |
| SPRIT5.DAX | independent | 11 | 33 | 13174 | `edcba16aab8187feb733f2edd50e4b7edec9b18af6a7a48973af231d1e47dd59` |
| SPRIT6.DAX | independent | 14 | 38 | 13727 | `53b777d72abba897d20452fe68ab4242bbc0fcb58101991fc7000860bef61079` |
| SPRIT7.DAX | independent | 12 | 36 | 12985 | `11090b90c51a655ade1aaa0b76e2ab30ce961d1e2d4f17483a91b11d51f30f5d` |
| SPRIT8.DAX | independent | 12 | 34 | 14853 | `046fb1431b1401e7267ad79038c74160cb9d91c77dbc10cb6a7acfd3cccd2c47` |
| CPIC1.DAX | ega | 22 | 22 | 6182 | `61a13e8daf91b511d316ecdc4e230e776c7d2de49894ccef3c30e2e250c09ff3` |
| CPIC2.DAX | ega | 38 | 38 | 11505 | `d9c24617095cb341a843fdb4921fe54d3d4ee0f909de2b3179301afb7ae567ba` |
| CPIC3.DAX | ega | 14 | 14 | 3150 | `0e063d1b659832639c859f4248e538a2ea69917acfcad394aaecec43228336df` |
| CPIC4.DAX | ega | 44 | 44 | 12154 | `181fe21499fce0504d84bcc3bb01d7911db74584400cdaa857a9fe7a20fdfac9` |
| CPIC5.DAX | ega | 20 | 20 | 7779 | `ace0d371b4e5a7fcb894a3c6b6078751ac52445b9e435897432e0a6ce576f705` |
| CPIC6.DAX | ega | 30 | 30 | 8101 | `f8285ccd2642a3c8c90b74810952796f240ee189412a5e24d7ed167c0ac424c6` |
| CPIC7.DAX | ega | 22 | 22 | 6178 | `6918aa709d53ef608574d734c879216f09c362ac1c8805f680232aad4744544a` |
| CPIC8.DAX | ega | 28 | 28 | 10325 | `dda82c9521fad757cf8dd35756393d76937a471224895ac99da92218c2a92f30` |

No pixels, glyph bitmaps or proprietary extracted content are included.

## ECL image host validation (2026-10-16)

The presentation layer selected all 92 picture records and all 88 sprite
records across the same eight archive groups fingerprinted above. Available
sprite distance frames were selected and compared with the decoder's indexed
pixels; all supported images fit the native 88x88 viewport. All eight group
checks returned zero errors. Picture selection is static frame zero; this does
not validate animation or timing.

A private independently authored GEO/ECL fixture then drove the actual SDL
window using the installed PIC1 record 1 and SPRIT1 record 2. Search executed
PICTURE and displayed the selected original image; rest executed SPRITE_START
with no replacement picture and displayed an original sprite over the view;
initialize executed SPRITE_OFF and cleared the artwork. Screenshots remain
outside the repository. The event fixture, not an original SSI event, selected
these assets. No original campaign resource binding or matched-scene parity
is claimed.

Synthetic CTest covers VM argument order, typed IDs, disabled/missing hosts,
conflicting and duplicate sprite dialect definitions, clear semantics,
distance advancement, both sides of blocking walls, missing/oversized resources,
failed-request state retention and the real SDL callback/upload loop. Existing
headless/harness unsupported-opcode diagnostics remain covered unchanged.
Images are transient presentation state and are cleared on restoring engine
state. Timed replacement pictures, four-operand sprite commands, original
inter-command delays and picture animation remain unsupported or unverified.


## Additional graphics and portrait structure (2026-10-16)

Read-only checks show the existing strict EGA decoder also handles TITLE
(2 records/2 images), CBODY (128/128), CHEAD (56/56), COMSPR (26/26),
BACPAC (1/40), ICON (6/6), and SQRPACI (2/256) in this installation.
FINAL5 records 1, 3, 4, 5 and 6 decode as static EGA (one image each);
records 2, 7 and 8 decode as explicit XOR-first sequences (4, 6 and 6 frames).
This is an observed inventory, not a filename-based decoder heuristic.

The separate EGA portrait adapter decoded and structurally composed all candidate
head/body pairs within each archive group, using an explicit 88x88 canvas and
fallback positions head (0,0), body (0,40):

| Group | Head records | Body records | Candidate pairs | Failures |
| --- | ---: | ---: | ---: | ---: |
| 1 | 3 | 3 | 9 | 0 |
| 2 | 5 | 6 | 30 | 0 |
| 3 | 21 | 16 | 336 | 0 |
| 4 | 3 | 3 | 9 | 0 |
| 5 | 11 | 6 | 66 | 0 |
| 6 | 7 | 5 | 35 | 0 |
| 7 | 10 | 4 | 40 | 0 |
| 8 | 3 | 3 | 9 | 0 |

Totals: 63 head records, 46 body records, 534 candidate pairs. These checks
establish decoding and bounded geometric fit, not original-event pairing,
correct character identities or visual parity. No portrait selection has been
wired into ECL or the SDL window in this increment. The private report retains
file sizes and SHA-256 fingerprints outside the repository. No pixels, extracted
artwork or proprietary text are included here.


## Graphics event integration and current blocker (2026-10-16)

The PICTURE host now supports explicit HEAD/BODY archives and profile-configured
head-ID memory/layout. A synthetic SDL event writes an arbitrary configured head
address, displays a portrait, writes the 255 sentinel, and displays a normal PIC.
Resource failures preserve the preceding image. This is host-flow validation,
not execution of an original portrait encounter.

LOAD_AREA_DECO now requests typed bank IDs from the host. Synthetic tests cover
argument bounds, missing hosts/banks, replacing banks, saving selected IDs,
restoring/rebinding them, and rejecting a visual restore when its banks cannot
be loaded without changing the current state. Original archive groups remain
explicit; archive fingerprints are not yet bound to prototype save identity.

A bounded read-only harness probe of the installed GEO1/ECL1 record 18 invoked
its initialization entry from the documented empty prototype state. It stopped
after 2 steps with `Unknown ECL opcode 33` at reported bytecode offset 113.
Opcode 0x21 is the researched LOAD_AREA_MAP command. The process exited normally
after the unsupported result and quit request. No original text or artwork was
emitted by the validation wrapper; only status, step count and diagnostics were
retained privately outside the repository.

At that checkpoint, map/campaign resource loading was not implemented, so this
original initialization could not establish its map and decoration state. This
was not evidence of matched-scene parity or complete campaign startup. The
map-loading resolution described below supersedes that specific blocker.


## Map-loading blocker resolved (2026-10-16)

The profile-selected three-operand LOAD_AREA_MAP adapter now resolves normalized
maps through a supplied read-only resolver. The CLI binds that resolver to the
explicit GEO archive snapshot; there is no automatic archive or ECL-program
selection. Synthetic tests cover changing map records, retained/all-zero modes,
invalid requests, missing/wrong records, position bounds, dialect conflicts,
map-aware save rebind, changed sources and legacy single-map saves. The SDL
path also loads a synthetic map, binds walls, saves, changes map and restores
both geometry and wall selection.

The original GEO1/ECL1 record-18 initialization was repeated from the same empty
prototype baseline. In the ordinary headless harness it passed the formerly
unknown opcode and selected area [18,2,255], map record 18, then stopped at the
absent wall-graphics host. That is a host limitation, not an unimplemented opcode.

An opt-in read-only probe with the explicitly supplied WALLDEF1/8X8D1/shared
8X8D1 archives then executed the complete original initialization entry:
`exited=1`, 15 steps, reported offset 184, map 18, one wall-bank binding [1,3,5],
zero text calls, and no diagnostic. It decoded the actual selected wall resources
and checked every wall ID in the loaded map. Only counts/IDs/diagnostics were
emitted; original text and pixels were suppressed. The private result is retained
outside the repository.

This resolves the specific map-18 initialization blocker. It does not establish
full campaign startup, NEW_ECL/program switching, automatic archive-group changes,
party initialization, combat or matched-scene graphical parity. Prototype map
save identities use a non-cryptographic archive change detector; this is not an
integrity guarantee or the installation scanner's SHA-256 inventory.

To repeat the same class of local probe with explicit authorized resources:

```sh
timeout 30 ./build/tests/goldbox_image_host_tests initialize /private/profile.json \
  /private/GEO.DAX 18 /private/ECL.DAX 18 \
  /private/WALLDEF.DAX /private/8X8D.DAX /private/shared-8X8D.DAX
```

Public CTest remains synthetic and does not access an installation.

## Character-dependent checkpoint investigation (2026-10-17)

The approved next acceptance target is a matched original character-dependent
event, followed by save/reload. It is not yet achieved.

A bounded reference run used the existing private DOSBox/game copy. Observed
startup checkpoints were wheel prompt -> Enter -> party menu -> Load -> slot A.
The original loaded six characters at displayed coordinates (15,4), facing west.
Opening View exposed the first character's six ability scores. The six ordinary
ability bytes at offsets 0x10..0x15 in its 285-byte native character record matched
the visible sheet. This supports those offsets for this record, not all character
layouts or creation rules. No installed executable/configuration was modified.
The owned reference process tree was stopped and cleanup verified.

Private metadata-only evidence binds the baseline to:

| Resource | Bytes | SHA-256 |
|---|---:|---|
| SAVGAMA.DAT | 13137 | abca248371287c43c99e1b34e03394819fca3cf406bed184e7e4fbfe4f0c08f1 |
| CHRDATA1.SAV | 285 | d7ba1321e1fca8464d1220e77c26ce92173d60cb67b0f155c71868926cfa6c8e |

A private investigative control-flow inventory decoded the five declared lifecycle
instructions, followed typed branch/table targets and fallthroughs, skipped inline
strings by their declared lengths, and rejected overlapping instruction intervals.
It used opcode facts from the already-pinned SSI Engine reference and its explicit
Pool configuration; it did not search for byte patterns. It found party references
in 20 of 29 ECL records. This is a conservative structural inventory, not an
execution trace: conditional feasibility, subroutine state and original-game
reachability are not established by following both possible successors.

The current map-18 program references additional unsupported character fields;
the read-only name/ability adapter alone is insufficient. Candidate name output
in ECL5 record 7 includes PRINT at code offset 893 and a structural predecessor
WHO (0x39) at offset 805. Other candidates include name output in ECL3 record 14
and ECL8 record 13, with additional script/state dependencies. No candidate has
yet been reproduced as the same event in both engines.

The follow-up implementation adds configurable WHO party selection through the
existing menu host, updating loaded-character and selected-PC state together.
LOAD_CHAR continues to update only the loaded character. Synthetic SDL, JSONL
and state tests cover these mechanics; original interaction parity is unverified.
Next establish the actual reference route and matching world/quest baseline for
one event; implement only fields/effects that route requires. The supplied native
save is not an engine save: copying its coordinates and visible party scores does
not reproduce its world state. Do not report MATCH or event parity until both
sides execute the same checkpoint from equivalent state.

Original saves, character bytes, names, extracted scripts and screenshots remain
outside the repository. The private investigation is not a public importer or
a replacement for the tested C++ decoders. The research itself needed no build.
The subsequent WHO implementation passed all 23 CTest suites in 17.45 seconds,
including SDL selection, JSONL automation, invalid choices and saved state.
Hygiene and attribution checks also passed. This is synthetic behavior coverage,
not original WHO event parity.

### Executed map-7 character event and next blocker

The candidate is now confirmed as an original search route in GEO5/ECL5 record 7
at (8,5), whose raw selector 0x84 resolves through table offset 249, selector 4,
to code offset 730. A bounded headless probe used authored two-character state,
not the supplied original save. It chose option 0 in the initial two-choice menu,
then character index 1 through WHO. The actual C++ VM reached offset 821 after
13 steps and reported `Unsupported character field at offset 256`; both loaded
and selected-PC indices were 1. Original text was suppressed from the report.

Adding explicit symbolic `okay` status to the authored characters and the
profile-defined readiness binding allowed the same original route to pass that
read and advance to RANDOM (opcode 0x08) at offset 846 after 19 steps. RANDOM
remains unsupported. This is execution of original bytecode with controlled
authored inputs, not an equivalent original-game checkpoint or a parity result.

The event also exposed a VM comparison issue: AND stored its result but did not
update comparison state. The pinned SSI Engine sets the flag to zero for a zero
result and one otherwise. A synthetic harness reproduction first compared equal
values, computed AND=1, then incorrectly skipped an IF_NOT_EQUALS write (0 rather
than the expected 42). The fix updates comparison after a successful store, with
regressions for both zero/nonzero results replacing stale comparison state.

The next execution requirement is explicit deterministic random-state handling
with save/restore and bounded draws; do not use ambient time/randomness or claim
SSI RNG parity. The actual original reference route and equivalent world/quest
baseline still need to be established. Character status mutations, native status
flags/ordinals and class-level reads remain outside this increment.

### Completed controlled character-name branch

A second probe explicitly authored bit 0x10 at memory 0x4A6C, following the
decoded flag-controlled alternative around RANDOM. This is a declared test
precondition, not a discovered native-save value or a modification to an installed
game. With both authored characters `okay`, responses were initial menu index 0,
WHO character index 1, and subsequent two-choice menu index 1.

The original event displayed the chosen authored character's name and exited
successfully after 30 steps at offset 1061 with no diagnostic. Loaded-character
and selected-PC indices were both 1. The harness saved the resulting state, a
fresh engine process loaded it, and the full serialized state matched exactly.
Text content remained private; the public evidence records only byte counts,
selection IDs, offsets and pass/fail results. Choosing index 0 in that later menu
instead reached unsupported DAMAGE (0x2E) at offset 1103 after 36 steps.

The final bounded build passed all 23 CTest suites in 19.16 seconds, including
status persistence/unknown-state behavior and both AND flag regressions. The
independent synthetic AND reproduction now performed the expected write of 42.
Hygiene and attribution checks passed. Metadata reports and engine saves remain
under the workspace's private `.cache/goldbox-validation/` directory.

This establishes a completed original character-dependent branch with authored
inputs and fresh-process persistence. It does not establish a naturally reached
reference-game route, equivalent quest flags, original RNG/damage behavior, or
side-by-side event parity. Those remain explicit acceptance gates.

## Original DOS random-generator investigation (2026-10-17)

**Confirmed for the fingerprinted installed build: it uses a seeded PRNG, and
startup seeds it from the DOS time-of-day.** This is analysis of the original
executable, not an inference from SSI Engine's Java Random implementation.

| Original resource | Bytes | SHA-256 |
|---|---:|---|
| START.EXE | 65231 | d825df2b174675c9088ba1489488bdeebe66ad2a22943f17d3a198e60b6a07bd |
| GAME.OVR | 206105 | 872964a32cef0fc32bbf83bab68b15e04ad36d59076f37b00a179dfee7928614 |

START.EXE uses EXEPACK. Private analysis reconstructed its 68,592-byte load image
using the installed decompression stub's backwards run semantics: 889 commands
and a 337-byte unchanged prefix. The unpacked image and all disassembly remain
outside the repository. No installed executable was changed or replaced.

### Seed and generator

The runtime holds a 32-bit state at DS:4428/442A. Its update is:

```text
state = (state * 0x08088405 + 1) modulo 2^32
```

The integer runtime Random(n) advances that state and returns
`(state >> 16) modulo n` for nonzero 16-bit n. For n=0 it returns zero but still
advances state. This describes the runtime function, not yet a complete audit of
how every game/ECL caller transforms its bounds or consumes draws.

The seed routine calls DOS INT 21h/AH=2Ch, stores CX as the low word and DX as the
high word. Its seed therefore is:

```text
(second << 24) | (hundredth << 16) | (hour << 8) | minute
```

These are binary clock fields, not BCD or the game's adventure-time clock.
The units returned by DOS do not imply a guaranteed hundredth-second clock
resolution. The underlying API is documented in Microsoft's
[MS-DOS reference](https://www.pcjs.org/documents/books/mspl13/msdos/dosref33/).

The startup call chain was traced from load-image offset 00FD through overlay
stub 003E:0025, whose descriptor resolves to GAME.OVR offset E600. That routine
calls the seed function at overlay-file offset EBD5, targeting runtime
0AF8:1247 (load-image offset C1C7). The seed-function far pointer occurs once
in GAME.OVR and zero times in the resident image. This establishes a startup
call site; it is not proof against all possible indirect reseeding paths.

Private isolated emulation used Unicorn 2.1.4, with 16-bit x86 instruction budgets
and mocked DOS clock responses. Executing the original integer RNG matched the
formula for 1,120 draws: five initial states, seven bounds (including 0 and 65535),
and 32 draws each. Three clock inputs produced the expected seed words. For
initial state 0x12345678, the first ten Random(20) values were
19,11,16,4,9,19,8,3,12,7. No game executable or proprietary routine bytes are
included in these public findings, and this did not execute a full game session.

### Native save and repeatability limits

The native main-save loader issues ten block reads with sizes
1,2048,2048,1024,7680,5,1,1,1,328: exactly 13,137 bytes, matching the supplied
SAVGAMA.DAT. The large destinations are allocated game-state buffers; the
remaining destinations are specific game fields and stack-local records. The
resident runtime seed at DS:4428 is outside the identified saved fields, and
the examined loader does not restore it. The static audit therefore indicates
that the ordinary save omits RNG state. **A full live save/reload memory trace
has not been performed**, so that conclusion remains a static-analysis finding.

The same seed and draw-call sequence reproduce the same runtime results. A
fresh program launch normally receives a clock-dependent seed; merely loading
the same native save is not sufficient evidence of identical future rolls.
Exact harness replay should capture/restore runtime RNG state or an emulator
snapshot and align draw order. Seeding our replacement generator alone would
not establish full-game parity.

### Research provenance

Primary evidence is the user-owned fingerprinted binaries. Supporting format
research consulted the EXEPACK author's
[format notes](https://www.bamsoftware.com/software/exepack/) and the documented
Turbo Pascal overlay descriptor in
[Dos Navigator's overlay source](https://github.com/romiras/turbo-pascal-archive/blob/master/Files/Dos%20Navigator/OVERLAYS.PAS).
The latter has restrictive licensing and was used only to interpret metadata;
no source was copied, translated or adopted. GNU objdump and a private Unicorn
installation were analysis tools, not added engine dependencies. The private
scripts, extracted load image and metadata/emulation reports stay under
`.cache/goldbox-validation/`. No engine code changed and no C++ build was needed.

### Original ECL RANDOM caller verification (2026-10-17)

The same fingerprinted build's opcode-08 dispatch was followed to its actual
handler: GAME.OVR file offset 3CBC tests opcode 08 and calls the handler at F7A
(overlay unit 001F, code offset 0234). Its operand services are exported by
overlay unit 002B. This identifies the instruction through executable control
flow, rather than assuming that every runtime Random call has ECL semantics.

The handler's verified numeric behavior is:

```text
upperByte = resolvedUpperOperand & 255
runtimeBound = min(upperByte + 1, 255)
state = (state * 0x08088405 + 1) modulo 2^32
destination = (state >> 16) modulo runtimeBound
```

| Resolved upper operand | Runtime bound | Possible result |
|---:|---:|---|
| 0 | 1 | 0; still consumes one draw |
| 1 | 2 | 0..1 |
| 20 | 21 | 0..20 |
| 254 | 255 | 0..254 |
| 255 | 255 | 0..254 |
| 256 | 1 | 0; word operand narrows to its low byte |
| 257 | 2 | 0..1 |
| 65535 | 255 | 0..254 |

Thus ordinary byte bounds below 255 are inclusive, but 255 cannot be produced
by this instruction. A replacement that always draws from 0 through the full
resolved upper operand would disagree with this original build. Constant-zero
results must still advance the generator to preserve subsequent draw order.

Private Unicorn probes executed the original handler, operand decoder, operand
reader, destination writer, numeric conversion routines and runtime RNG. Overlay
entry thunks were resolved to private emulator load addresses; instruction bodies
were unchanged. Only synthetic ECL operands and memory were supplied. No DOSBox
process, original overlay loader or whole-game event loop was executed.

Verification passed **2,048 handler executions**: all 256 byte bounds, four
initial states (0, 1, FFFFFFFF, 12345678), and two consecutive draws per case.
Each execution checked the destination word, resulting 32-bit seed, exactly one
runtime draw and the six-byte advance over the synthetic instruction. Another
**32 executions** used word-memory upper operands 0, 255, 256, 257, 511, 65280,
65534 and 65535 across the same four states; these checked narrowing, the
unchanged source word, destination, seed, draw count and seven-byte advance.
Each execution had a 5,000-instruction/one-second limit, with separate whole-probe
deadlines. Reports and reproduction probes remain private under
`.cache/goldbox-validation/`; no original routine bytes are committed.

This resolves numeric RANDOM bounds and draw consumption for the tested operand
forms in this installed build. It does not establish every operand type, another
title/version's behavior, engine RANDOM support, or complete event parity.
Live native save/reload and idle-loop RNG observations remain unperformed; the
save-state conclusion above is still based on static analysis. Future engine
support should select these semantics through a compatibility profile rather
than imposing this build's byte narrowing on every game.

### Seeded comparison laboratory and live capture (2026-10-17)

`scripts/rng_debug.py` adds an original-runtime versus C++ comparison window.
The reusable engine primitive accepts explicit seed/multiplier/increment values;
the separate Pool adapter owns the fingerprint and native memory locations.
The original oracle executes the fingerprinted routine in a bounded external
Unicorn subprocess, not another implementation of its formula. No emulator or
original bytes are bundled with the engine.

The Windows capture path successfully launched an owned private DOSBox copy,
located the runtime body supplied by the private image, checked the DOS PSP and
relocated segment references, and read the same four-byte state twice. It waits
past the initial zero-filled state. A fresh captured state was `0x2E4D0932`;
32 isolated original-runtime draws with runtime bound 20 matched the C++ values
and before/after states exactly. A separate matrix passed 280 draws across five
seeds, seven bounds including 0 and 65535, and eight consecutive draws per case.
Native reference cleanup completed after capture. The numeric report stays at
the private cache's `rng-debug-live-comparison.json`.

The serialized, two-job build and full CTest run passed **24/24 suites**. Synthetic
RNG tests cover an independently observed vector, seed continuation, zero-bound
consumption, configurable arithmetic/wraparound, invalid inputs, deliberately
mismatching states, incomplete sequences, rejected images and validated seed
location. They require no original resources or Unicorn. Desktop window launch
completed, but visual inspection through computer-use was not completed because
the app-access approval timed out.

This verifies live seed extraction followed by isolated generator replay. It does
not observe live game roll calls, capture an atomic whole-game snapshot, validate
native save/reload, or align gameplay draw consumption. The gameplay ECL RANDOM
instruction remains unsupported; existing party creation still uses its previous
generator. Native gameplay parity is not established by the comparison window.

### Gameplay RNG and raw human character rolls (2026-10-17)

The engine now selects RNG parameters and ECL RANDOM semantics through the game
profile. Pool uses the verified 32-bit generator for character creation and ECL,
sharing one saved stream. The startup seed defaults to 1 and can be overridden
with `--seed`. Fresh-process scripted tests verify RANDOM results and restored
continuation; legacy saves without a random field retain unknown RNG state.
No original resources are required for those public tests.

This reuses the engine's existing parameterized RNG and party model. The dice
policy is an independent implementation of observed behavior, rather than a new
copy of upstream interpreter or character-creation code. Existing third-party
attribution remains unchanged.

A private bounded probe executes the installed original character ability loop
from GAME.OVR file offset 0x1AB90 through the stop before 0x1AC4F. It uses the
original dice helper and resident runtime, synthetic character memory and human
race code 7. Each ability retains the best of six 3d6 trials: 18 draws per ability,
108 per six-ability block. Five starting states produce these numeric facts:

| Seed | STR, INT, WIS, DEX, CON, CHA | Final state (decimal) |
| --- | --- | --- |
| 0 | 14, 11, 13, 14, 15, 15 | 1638965300 |
| 1 | 16, 13, 14, 16, 17, 14 | 2628645125 |
| 0xFFFFFFFF | 16, 13, 14, 13, 16, 13 | 649285475 |
| 0x12345678 | 15, 15, 14, 15, 15, 16 | 1040758828 |
| 0x2E4D0932 | 15, 16, 14, 15, 17, 14 | 2883502598 |

All 540 runtime draws have bound 6. Public regression tests check these scores
and final states against the engine. The probe limits each ability to 10,000
instructions/100 ms and the complete run to ten seconds, with a separate
15-second process deadline. The reproduction script and numeric report remain
private at `.cache/goldbox-validation/emulate_original_creation_rolls.py` and
`original-creation-rolls-report.json`. No original bytes or disassembly are added.

This verifies only the raw human subloop from the declared starting state.
The original draws Random(256) earlier in creation and subsequently performs
racial, age, class and gender processing, including exceptional strength.
The engine still has provisional eligibility rejection and percentile-cap
policies; racial adjustments, native percentile timing and complete creation
draw alignment remain unfinished. This milestone does not establish native
save/reload, live input timing or full character/event parity.

The serialized two-job build passed. CTest passed all 26 public suites in
19.55 seconds; the optional private original-RNG integration suite skipped
because its private inputs were not configured for this run. Hygiene,
attribution and whitespace checks passed. This run includes synthetic
fresh-process RANDOM/save/reload and numeric raw-character regression coverage.

### Racial ability adjustments (2026-10-17)

The same private original-code probe now exercises race codes 1, 2, 3, 4, 5 and
7 through the raw rolling loop. The installed GAME.OVR and unpacked resident
image SHA-256 fingerprints match the previously recorded inputs. Original
instructions apply fixed modifiers to each trial before selecting the best:
dwarf CON +1/CHA -1, elf DEX +1/CON -1, halfling STR -1/DEX +1; gnome, half-elf
and human remain unchanged in this loop. Applying a fixed modifier once after
the maximum yields the same scores without extra RNG draws.

The bounded probe ran seven seeds (the five earlier states plus 7 and 17) for
each of six races: 42 blocks, 4,536 original runtime draws. Public regression
tests check the original five seeds for all six races, plus seed 7 dwarf
scores [13,11,15,12,19,16] / final state 4271756779 and seed 17 elf scores
[14,15,13,19,15,18] / final state 1283653141. Those last two cases also validate
legal 19 values through party persistence. Every block consumes exactly 108
bound-6 draws. The other raw seed-7/17 blocks are not public engine parity
fixtures because existing provisional eligibility/cap checks can reroll them.

The independent implementation adds optional signed, bounded per-ability values
to each race's rules configuration, with no title IDs or native race codes in
the core. It reuses the current party roller; no upstream source is copied or
ported. Existing saved values are read as already adjusted. Private reproduction
and numeric results remain in `.cache/goldbox-validation/` as
`emulate_original_racial_rolls.py` and `original-racial-rolls-report.json`, with
the same instruction/time/process limits as the human probe.

This advances raw-score compatibility only. Native age/class/gender processing,
cap-versus-rejection behavior, exceptional-strength timing and whole-creation
draw alignment remain unverified. No original game assets are redistributed.

Verification: the bounded serialized two-job build and all 26 public CTest
suites passed (19.06 seconds); the optional private integration test skipped
because its private inputs were not configured. Hygiene, attribution and
whitespace checks passed. Synthetic cases also cover alternate ability layouts,
invalid adjustment counts/types/ranges and unchanged draw consumption.

### Racial bounds and exceptional-strength caps (2026-10-17)

Bounded execution of the original post-roll code verifies clamping to the
existing race/gender minimum and maximum tables, with no random draws.
The private probe enters each ability's bound block directly and stops at the
first read of the character class code. This deliberately excludes age and
class-floor processing. Entry file offsets are 0x1AD47, 0x1AF29, 0x1B0B5,
0x1B222, 0x1B385 and 0x1B46D for STR through CHA in the previously fingerprinted
GAME.OVR. Synthetic input scores 0, 2, 3, 8, 18, 19 and 255 across six races,
two genders and six abilities pass **504 cases**, all without RNG consumption.
The creation rules' current racial limits match these observed tables.

A second probe enters the original exceptional-strength block at 0x1AE59 and
stops before 0x1AECD. Synthetic fighter flags and legal capped strength values
exercise six races, two genders and six states: **72 cases**, 36 runtime draws.
The routine draws Random(100)+1 when strength is 18 and the fighter flag is set,
then caps the percentile to the race/gender maximum. It does not roll directly
within a smaller cap. Seed 0x12345678 produces 60, capped to 50 for a male gnome
or female human, with final state 0xCB5B9059. Seed 23 produces 100, capped to
99/75/50/90 for male dwarf/elf/gnome/half-elf, respectively, and stays 100 for
a male human. Its final state is 3099843700. Legal strength below 18 consumes
no exceptional-strength draw. Native draw ordering within creation is unchanged
by this finding and remains to be implemented.

Private probes/reports are `emulate_original_ability_caps.py`,
`original-ability-caps-report.json`, `emulate_original_percentile_caps.py` and
`original-percentile-caps-report.json` under `.cache/goldbox-validation/`.
They use 10,000-instruction/100 ms per-case limits, ten-second total deadlines
and a 15-second outer process timeout. Original code and data remain private.
Public fixtures contain numeric facts and synthetic rule configurations only.

The independent engine implementation extends the existing rules reader with
optional `dice.bounds` and `percentile.rollMaximum`; omitted values preserve
the previous reject/direct-within-cap policies. The AD&D creation rules opt in
to clamping and a 100-sided percentile roll. Existing party validation/save
reading still reject invalid saved values rather than silently clamping them.
No upstream source is copied or translated for these changes.

Native age modifiers, class-specific score floors, native class selection order,
the earlier Random(256) draw and percentile timing remain unfinished. The creator
currently selects a class after rolling and consumes exceptional strength at
completion; therefore matching these isolated primitives is not complete
character-creation or live-game parity.

Verification: the serialized two-job build and final full CTest run passed all
26 public suites in 19.75 seconds; the optional private integration test skipped
for unconfigured inputs. Hygiene, attribution and whitespace checks passed.
The first run found a CRLF-sensitive new legacy-rules fixture, corrected by
normalizing carriage returns, and one existing harness SIGTERM diagnostic
failure. That signal failure did not reproduce in its isolated test, 20 further
bounded SIGTERM/SIGHUP checks, or the final full run. Harness implementation
is unchanged; this does not establish that the intermittent failure is fixed.

### Class-specific creation floors (2026-10-17)

A private probe resumes each original ability handler at its first class-code
read after racial clamping, injects a synthetic score, and stops before the
common presentation callback at GAME.OVR offset 0x1B54B. The original blocks
raise scores to the class table's minimum, leaving already higher scores alone.
Four supported single-class codes, six abilities and nine input scores
(0, 2, 3, 6, 8, 9, 12, 18, 255) pass **216 cases**, with no RNG draws. The
fighter flag is clear for this primitive probe to exclude percentile processing.

| Class | Native code | STR, INT, WIS, DEX, CON, CHA floors |
| --- | --- | --- |
| Cleric | 0 | 6, 6, 9, 0, 0, 0 |
| Fighter | 2 | 9, 0, 6, 6, 7, 0 |
| Magic-user | 5 | 0, 9, 6, 6, 0, 0 |
| Thief | 6 | 6, 6, 0, 9, 0, 0 |

Zero means no class floor for that ability. These independently observed numeric
facts are selected in the AD&D rules via optional per-class `creationMinimum`
vectors; title IDs and native class codes do not enter the shared engine.
No upstream source is copied or translated. The private reproduction and report
are `.cache/goldbox-validation/emulate_original_class_floors.py` and
`original-class-floors-report.json`, using the existing per-case 10,000-instruction/
100 ms, ten-second total and 15-second outer process limits.

The engine's creation choice query considers floors without changing the draft.
Completion raises scores before exceptional strength. Impossible combinations
whose floors exceed racial/gender caps are unavailable and fail explicitly.
The existing strict eligibility query and saved-party validation retain their
ordinary requirements; loading an existing character never raises its scores.
Rules without creation floors keep their prior score-based choices.

This preserves the current UI's roll-then-class flow; the original chooses its
class earlier and may draw exceptional strength between ability rolls. Age
modifiers, native class-selection order, the earlier Random(256) draw and exact
draw alignment remain unfinished. This is a class-floor milestone rather than
full character-creation parity.

Verification: the bounded serialized two-job build and all 26 public CTest
suites passed (30.19 seconds); the optional private integration suite skipped
for unconfigured inputs. After making empty-vector initialization explicit to
remove a compiler warning, the build and all three affected party/RNG/SDL-host
suites passed again (0.48 seconds). Hygiene, attribution and whitespace checks
passed. The SDL regression creates a class requiring 12 from a raw ability of 3,
then verifies the raised value survives save/load. Synthetic tests also cover
all four floor vectors, unchanged RNG state, impossible caps, malformed rules,
legacy rules without floors and existing saved characters below creation floors.

### Starting-age generation (2026-10-17)

The original single-class creator generates age after class selection and before
ability rolls. A bounded private probe enters GAME.OVR offset 0x1A862 and stops
before 0x1AB60, using the original dice/runtime routines and synthetic character
records. Across all 17 supported race/class pairs and six initial states
(0, 1, FFFFFFFF, 12345678, 2E4D0932 and 23), **102 cases** complete with exactly
the configured number of runtime draws: **318 total**. The method excludes
earlier setup and subsequent ability/age-modifier processing.

The observed model is a class/race-specific base plus NdS. Examples from state
0x12345678 are dwarf fighter 40+5d4 = 52, elf fighter 130+5d6 = 148, human
fighter 15+1d4 = 19, and human magic-user 24+2d4 = 32. Corresponding final states
are 3859354085, 3859354085, 3411775577 and 2046801342. The private reproduction
and numeric report are `.cache/goldbox-validation/emulate_original_starting_age.py`
and `original-starting-age-report.json`; per-case limits remain 10,000
instructions/100 ms, with ten-second total and 15-second outer process bounds.

The independent implementation adds optional `startingAge` data to existing
race rules and optional normalized age to party records. It reuses the existing
dice mapping and shared RNG; no upstream code or original character bytes are
copied. Seventeen public numeric fixtures check age and final state, plus
synthetic persistence, invalid-input and legacy-unknown-age cases. Configured
UI creation selects a class before generating age, then rolls abilities;
unconfigured creators preserve their old flow and consume no age draws.

Age-based ability modifiers are not implemented in this milestone. The original
applies those before racial clamps; applying them at completion to already
clamped scores would be incorrect. Reroll/native UI behavior, the earlier
Random(256) draw, alignment sequencing and exceptional-strength draw timing
also remain unverified or unfinished. This verifies age generation and storage,
not complete original character-creation or gameplay parity.

Verification: the bounded serialized two-job build and all 26 public CTest
suites passed in 20.38 seconds. The optional private integration suite skipped
for unconfigured inputs. Hygiene, attribution and whitespace checks passed.
Synthetic tests cover all 17 seeded age fixtures, exact final RNG states,
continuation after restore, unknown legacy ages, rejected age values/dice/class
references, and SDL creation with age preserved through save/load.

### Combined seeded character-generation comparison (2026-10-17)

The engine now executes the tested numeric creation sequence as one prepared
roll: a setup Random(256) draw, starting age, then each ability's best-of-six
3d6 result, racial adjustment, ordered age effects, racial/gender clamp, class
floor and (when applicable) exceptional strength before the next ability.
Age effects use data-defined inclusive age ranges, signed per-ability changes,
positive-only guards and optional unsigned-byte wrapping. The latter preserves
the original byte operations rather than assuming unbounded integer arithmetic.

The private combined oracle executes the fingerprinted original blocks:
GAME.OVR 0x19F9D..0x19FAE for the initial random draw/store, 0x1A862..0x1AB60
for age, and 0x1AB90..0x1B54B for each ability. Synthetic records select known
race/class/gender values and the native single-class flag. Rendering callbacks,
menus and allocations are excluded; original dice, runtime and age/class
processing execute directly. No instruction bytes are pattern-scanned to infer
ECL entry points, and no original resources are committed.

Three bounded probes produce numeric snapshots:

- All 17 supported race/class pairs, both genders and six seeds (0, 1,
  FFFFFFFF, 12345678, 2E4D0932, 23): 204 snapshots and 22,872 runtime draws.
- Male human fighters from seeds 0..255: 256 snapshots and 28,165 draws,
  including five exceptional-strength cases. Seed 18 produces age 17, scores
  [18,14,13,12,16,13], percentile 29 and final state 1273674349 after 111 draws.
- Explicit ages immediately below, at and above all four modifier thresholds,
  across six races and both genders, using fighter class and seed 12345678:
  144 snapshots and 15,552 draws. These start immediately before abilities,
  with age supplied, so they exclude setup/age draws.

Public CI stores 353 numeric snapshots: the first matrix, all five exceptional
cases, and every age-boundary case. It checks age, all scores, percentiles and
exact final state, then validates party persistence. Expectations come from
original execution, not from recomputing the engine's formula. Further tests
exercise save/restore continuation and deterministic age-preserving rerolls,
invalid-input RNG atomicity, and the SDL path's lack of a confirmation-time
percentile redraw. The existing lower-level raw-roll/dice tests remain intact.

Private reproduction/report names under `.cache/goldbox-validation/` are
`emulate_original_creation_sequence.py` / `original-creation-sequence-report.json`,
`emulate_original_creation_strength.py` / `original-creation-strength-report.json`,
and `emulate_original_creation_age_boundaries.py` /
`original-creation-age-boundaries-report.json`. Each execution is bounded to
25,000 instructions/100 ms per original block, 45 seconds per complete probe
and a separate 50-second outer process timeout.

This independently extends the existing party roller and configurable rules;
no upstream implementation is copied or translated. The setup byte written to
the native record is currently consumed for RNG alignment but its unrelated
character-field meaning is not modeled. Numeric kernel matching does not prove
live input/idle/cancellation/reroll parity, complete native character records,
HP, spells, equipment, multiclass creation, portraits or exact UI presentation.
Alignment is selected before rolling in the configured engine flow; an
interactive original-game checkpoint comparison is still outstanding.

Two additional original post-processing checks verify byte-edge behavior:
elf CON 2 at age 1201 wraps through 255 and clamps to 18; human STR 0 at age 91
remains zero under the positive-only guards, then clamps to 3. Synthetic dice
configurations force these inputs through the same engine roll path. These
are arithmetic compatibility checks, not claims about ordinary starting ages.
The private reproducer is `emulate_original_age_byte_edges.py` in the same cache.

Verification: the bounded serialized two-job build and all 26 public CTest
suites passed in 20.05 seconds; the optional private integration suite skipped
for unconfigured inputs. After adding the save/replay and byte-edge checks,
the affected generation suite passed again (0.24 seconds). Hygiene, attribution
and whitespace checks passed. The SDL synthetic creator verifies four draws
(setup, age, ability, percentile), final state 0xF25CF394, percentile 45 and
save/load continuation, detecting any extra confirmation-time draw.

### Starting HP and gold — 2026-10-17

Using the same private installation and unpacked image fingerprints recorded
above, execute the original post-stat initialization through its HP stores.
The bounded probe loads overlay AC for cleric spell initialization as well as
78 and B0; no installed file is changed. All 204 supported race/class/gender/seed
cases agree with isolated money/HP runs: cleric initialization adds no random
draws. Gold is stored at record +8E, corroborated by the existing research-only
SSI Engine `data/character/CharacterValues.java` at commit
`51d4b6d409fa798754f5ff2ccb249f3c0b6d029b`. Its placeholder creator/HP rules
offer no advantage for this verified behavior; no upstream code is adopted.

Native sequence: class-specific money dice, times ten; class-specific hit dice,
floored at two thirds of die sides; Constitution adjustment; minimum one HP.
The complete original initializer stores identical initial current/maximum HP.
The base roll is retained independently. A separate 72-case probe covers CON
3..20 for all four supported classes, including negative modifiers, minimum-one
HP and fighter-only CON 17/18 bonuses. The original rejection branch returns
to ability rolling after age generation and rerolls money/HP too.

Public tests retain numeric observations only. They cover all 204 full results,
72 HP boundaries, exact final RNG, persisted wounds/zero balances, malformed
saves/rules, failure atomicity after money rolls, complete seeded reroll replay,
and scripted SDL creation/save/load. The six-draw synthetic SDL case now ends
at 0xE28F6D7A (adding money and HP after the prior four draws).

Private reproductions/reports under `.cache/goldbox-validation/`:
`emulate_original_creation_vitals.py`, `original-creation-vitals-report.json`,
`emulate_original_creation_initialization.py`,
`original-creation-initialization-report.json`,
`emulate_original_creation_hp_edges.py`, `original-creation-hp-edges-report.json`.
Each original block is bounded to 25,000 instructions/100 ms, with a 45-second
total deadline (10 seconds for HP boundaries). This verifies native numeric
creation behavior, not interactive screen parity, idle timing, multiclass
creation, complete spell semantics, equipment or native-save interoperability.

Verification: the serialized two-job build and all 26 public CTest suites pass
(20.54 seconds); optional private integration skips without configured inputs.
Hygiene, attribution and whitespace checks pass. The first HP test run exposed
missing modifier coverage for legal dwarf CON 19; the corrected table and new
original boundary checks cover scores through 20.

### Starting spellbooks — 2026-10-17

The same 204 complete original post-stat executions record spell availability
flags and first-level capacity. Clerics set IDs 1..8; magic-users set 11,18,19,21;
fighter/thief set none. Gold Box Explorer `FruaCharacter.cs` at research revision
`eac30abaa6ee66aea6f5d65ebe6d676b10015a8f` corroborates standard spell names and
slot-field meanings. These references remain research-only; no upstream viewer,
spell implementation or character layout is adopted. Independent native Pool
execution supplies the behavior rather than assuming FRUA compatibility.

Private `emulate_original_creation_spell_edges.py` and
`original-creation-spell-edges-report.json` extend the full initializer probe to
18 cleric Wisdom scores 3..20, under the same per-block/total limits. First-level
slots are 1 through Wisdom 12, 2 at 13, and 3 from 14; mage capacity is 1.
The full initializer consumes exactly the previously recorded gold/HP draws.
Public regression tests compare stable normalized known-spell IDs and capacity
for 204 cases and all 18 boundaries, verify unchanged RNG, reject malformed
books/rules, preserve custom saved IDs and legacy unknowns, and exercise SDL
creation/save/load. Casting, memorization and higher-level spell progression
are not implemented by this starting-state model.

Verification: all 26 public CTest suites pass (20.63 seconds); optional private
integration skips without configured inputs. After adding stricter in-memory
configuration guards, all four affected party/RNG/SDL suites pass again (0.65
seconds). Hygiene, attribution and whitespace checks pass.

### Multi-class creation — 2026-10-17

Read-only native research identifies race class-selection table DS:3E17
(12 bytes per race, count followed by class codes), class floor table DS:3F81
(six bytes per class), and alignment table DS:3FE7 (10 bytes per class).
The selection code sets component flags before deriving starting state. This
confirms seven supported compound class IDs across fourteen additional race/class
pairs, without importing later-game ranger/paladin or cleric/thief options.
SSI Engine's existing research-only standard rules include later-game options;
its combined-class implementation is not adopted for Pool.

Private `emulate_original_multiclass_creation.py` and
`original-multiclass-creation-report.json` execute class-flag initialization,
setup, age, all abilities and complete post-stat initialization for 168 compound
cases (14 pairs, two genders, six seeds). Per-block execution is bounded to
25,000 instructions/100 ms, with a 45-second total deadline. Original compound
age is constant (selected base plus maximum dice), consuming no age RNG. Money
rolls for every component precede all HP rolls. Both money and HP totals are
divided by component count; HP base is divided separately before storage.
The original Constitution routine grants the extra CON 17/18 fighter bonuses
only when the single-class code is 2. Compound total modifiers reflect the
separately observed calculation, rather than inheriting that extra bonus.

An additional bounded 64-seed scan of half-elf Cleric/Fighter identifies seeds
8,15,18. `emulate_original_multiclass_strength.py` runs those seeds for seven
compound classes and both genders: 42 cases, including 15 exceptional-strength
results. Its reports are `original-multiclass-strength-scan-report.json` and
`original-multiclass-strength-report.json`. Public tests retain only numeric
outcomes and normalized option IDs. Native binaries, class-table bytes, spell
records and probe implementations remain private.

The combined regression catches a conditional beyond the generic class-floor
table: Wisdom below 13 is raised to 13 for the supported cleric compounds.
`emulate_original_multiclass_wisdom_floor.py` confirms the isolated postprocessing
for single cleric and all seven supported compounds. This is represented as an
effective parent creation floor, without changing ordinary saved-party minima.
The SDL compound-creation scenario also checks that class selection, fixed age,
separate books and exactly seven synthetic draws survive the interactive flow.

Verification: all 26 public CTest suites pass (21.21 seconds); optional private
integration skips without configured inputs. All 210 original compound outcomes
match, including exceptional strength and exact final RNG. Party tests cover
cycles/missing/duplicate components, conflicting books, custom saved book IDs,
fixed-age draws and preventing oversized party output that its reader would
reject. Seeded save/reroll replay and the scripted compound SDL creator pass.
The initial SDL failure was a test input sent at the gender screen; corrected
class selection passes. A synthetic save left by that abort was preserved in
the private validation cache before the next run. Hygiene, attribution and
whitespace checks pass. Live original UI parity and remaining creation fields
remain unverified.

### Player portrait choices — 2026-10-17

The known creation call target 78:33E7 (file offset 1CD66) is the portrait
chooser, followed by a separate combat-icon chooser. Disassemble from the known
call target: the older linear overlay dump crosses embedded text and does not
preserve every subsequent instruction boundary. Creation sets native head/body
selectors to 1/1. H cycles heads 1..14, B cycles bodies 1..12 and K keeps them.
Private `emulate_original_portrait_controls.py` verifies all 26 cycle/keep cases
with bounded native blocks and no RNG calls.

The original drawing call 3D0:0034 maps selectors through DS:286F and DS:287D;
creation selects resource bank 3 at file offset 19AE7. The normalized head IDs
are 0,8,9,13,16,18,22,34,45,51,53,57,67,68 and body IDs are
1,2,3,4,7,8,18,24,26,33,35,37. This is explicit rules data. It must not be
replaced with contiguous DAX IDs or the CHEAD/CBODY combat-icon families.

Authorized resource fingerprints:

| File | Bytes | SHA-256 |
| --- | ---: | --- |
| HEAD3.DAX | 12403 | `1413773d5c2841bf62902a0d5bef7fc7439e9644fc518e03ae1b268d34ea1ed4` |
| BODY3.DAX | 21505 | `7ea261c09a93293157e6865e1ac0ba82a8cbf149882dfd95e35704efd5a8b804` |

The read-only `goldbox_image_host_tests portraits` probe composes all 21 heads
and 16 bodies in these archives: 336 combinations pass using the existing
profile layout and EGA composer. The creator offers the verified 14 by 12
subset. Only counts, IDs and fingerprints enter public reports; original art,
probe disassembly and extracted native bytes stay in the private cache.

The independent creator uses configured part lists, preserves normalized
selection IDs in saves, previews original artwork, and restores prior event
image/animation state after creation or viewing. Synthetic SDL checks cover
noncontiguous IDs, cycling/wrapping, confirmation, roster viewing, save/load,
cancel, F10 and missing artwork. Choosing a portrait consumes no RNG. Combat
icons, live original input/idle behavior and original-screen pixel parity remain
unfinished.

Verification: the serialized two-job build and all 26 public CTest suites pass
(21.81 seconds); optional private integration skips without configured inputs.
The final incomplete-resource preflight regression also passes in the affected
SDL suite (0.67 seconds). Hygiene, attribution and whitespace checks pass.

### Combat-icon reference boundary — 2026-10-17

The next native creation call is the distinct icon chooser at file offset
1D1AC. Its state includes head/weapon parts, size and six color fields. The
existing Gold Box Explorer EgaBlock reference handles raw decoding and the
combat palette, not this customization compositor; no new upstream code is
adopted. The existing format adapter already supplies that palette.

The new read-only `goldbox_image_host_tests icon-parts <archive>` probe validates
every source record through the existing strict decoder: CHEAD.DAX has 56
single-image parts; CBODY.DAX has 128. Missing files and raw EGA decoding are
therefore not the pending dependency. None of the original pixels are printed
or added to fixtures. The affected public SDL suite passes again (0.62 seconds).

| File | Bytes | SHA-256 |
| --- | ---: | --- |
| CHEAD.DAX | 3442 | `aecd2ebbcd0f6c9d6fe7fb490591ea0357c1ced8917be059cfd4e60e60d701d4` |
| CBODY.DAX | 23978 | `9928778ce936d070675233abeb008b27656838b0b41bc5a398a62f9b42151601` |

A bounded 240-second preview reference was launched from a fresh private copy.
Enter passed the installed edition's wheel prompt, repeating the earlier
verified route without a lookup. The original proceeded through dwarf/male/
fighter/alignment, score acceptance, name entry and portrait Keep. Its icon
screen displays OLD/NEW ready and action previews, with Parts, Color-1, Color-2,
Size and Exit; the Parts submenu offers Head and Weapon. This establishes the
screen sequence, not pixel parity. The controller reached its safety deadline,
terminated its owned processes and left no reference window running.

Remaining verification dependency: establish deterministic native image
expectations for pose composition, size and all six recoloring channels before
implementing a matching icon adapter. The current comparison controller remains
an unpaired preview; its success cannot stand in for a rendered-icon comparison.
Neither ordinary opaque portrait layering nor raw-part decode success proves
those operations. Full creation parity is not claimed. Combat-icon composition
and its private original-image oracle remain unfinished.

Tool friction: the first sandboxed reference launch returned invalid JSON from
the WSL launcher and no cleanup acknowledgment. Inspection confirmed no engine
or window remained; the authorized elevated retry launched correctly. Computer
Use initially captured an occluding window; explicitly activating the returned
DOSBox window corrected it. Unicode text entry did not reach DOSBox's name
prompt; a key event did. After the reference deadline, a further input reported
no foreground process; no retry was sent to a stale window.

## First PC-speaker audio slice (2026-10-17)

Read-only analysis used the same authorized Steam START.EXE, SHA-256
`d825df2b174675c9088ba1489488bdeebe66ad2a22943f17d3a198e60b6a07bd`.
The resident EXEPACK image was extracted privately without changing the installed
executable. No proprietary sound programs or PCM are included in public tests.

The observed sound driver initializes a 5041-count timer interval. Its resident
data segment begins at image offset `0x8ae0`; the four-channel effect pointer
table begins at segment offset `0x55f`. A bounded private Unicorn probe executed
the original initializer at image offset `0xa222` and PC-speaker tick routine at
`0x9f57`. Sampled entries 1..19 terminated after 9..601 ticks. Entry 20 uses
additional control commands; no music interpretation is claimed.

The first public adapter intentionally supports constant-tone FF stores to
duration, pitch and speaker gate, with four-channel priority. It does not scan
for instructions and rejects other fields/commands. Layout and the selected
preview entry 8 live in the Pool profile, not title checks in the player.
The independent C++ decoder matched every pitch/gate frame of entry 8 against
the native trace: 10 timer frames and 2027 synthesized samples at 48 kHz. This
checks register-level timing, not physical speaker filtering or recorded-audio
parity. The original driver also latches pitch before interpreting the next
command; that ordering is covered by a synthetic regression.

An explicit private original-resource test successfully opened the default
desktop audio device and submitted the decoded effect. Public tests use SDL's
dummy audio device, and scripted window tests audition twice and restart from
startup/exploration without changing serialized game state. F8 is a preview
control; no automatic ECL/gameplay sound trigger is claimed. Startup music,
Tandy synthesis, sweeps, loops and modulation remain unfinished. No startup
music was invented, and absence of separate audio files does not prove this
edition has no embedded music.

Local SDL had been built with audio disabled. Its pinned 3.2.28 dependency was
rebuilt with audio and PulseAudio enabled, under the shared lock with two
workers and fixed timeouts. WSL PulseAudio development headers were installed.
CI now also enables audio, using a dummy device for reproducible tests.
The first dependency configure timed out on the Windows filesystem; moving its
temporary build to Linux storage resolved that. A final install argument picked
up a Windows carriage return; a separate bounded install completed successfully.

Validation: focused audio/profile/window tests passed, followed by all 27 public
CTest suites (the separate private RNG integration suite skipped without its
configuration). Repository hygiene and attribution checks passed. An additional
two-second CLI smoke test loaded the original sound executable alongside a
synthetic map/ECL pair and reached the window loop; timeout closed that owned
process without writing a save. This did not exercise automatic sound events.

## Expanded effects and ECL sound routing (2026-10-17)

The independent decoder now matches all sampled effect table entries 1..19:
2,363 original timer frames, including signed pitch sweeps, modulation lookup
and FE counter loops. The public fixtures remain synthetic; every original
program, lookup table and expected pitch/gate sequence stays in private storage.
Limits on ticks, commands, sample count and segment reads still apply. Music/note
commands, unsupported envelope fields and sound controls are not inferred.

Pool's native ECL dispatcher was traced to its CALL handler (overlay 0x1f,
entry offset 0x2ccf). CALL operand 0xba03 requests a sound only for selector
values 8 or 10; the inspected original sound IDs are 10 and 11. Later-game
SOUND_EVENT opcode labels were not used as evidence for this release.

The selector is a virtual native word register at ECL address 0x03de. It maps
internally to DS:0x84cc. A write to virtual address 0x84cc does not update that
register: that false correspondence was caught by a native WRITE probe before
acceptance. Full original WRITE-plus-CALL execution matched 18 selector cases,
including 264 and 266, which must not alias 8 and 10. A further native probe
confirmed that a byte WRITE clears the previous high byte of this word register.
The engine preserves this special write behavior through its session adapter.
Partial/interior register writes fail explicitly.

The CALL token is the raw word operand, not a read from RAM at that address.
Native probes corroborated word operand forms 1, 2 and 3. Other forms remain
unsupported. The byte-address form advances differently in the original and
was deliberately excluded rather than silently interpreted as a word.

The Pool profile supplies the virtual selector, call token, effect IDs and
sound-bank bounds. The renderer receives a resolver and the VM invokes a host
interface; neither selects behavior by game title. Saves retain selector state
through the existing memory representation. Missing audio, unknown native calls,
invalid sound IDs and unsupported controls report diagnostics. Selectors without
a configured sound route reproduce the verified no-op behavior. The existing
partial-progress semantics still apply if a host request fails.

This verifies an isolated original CALL path and original effects, not successful
execution of every instruction in an imported map event. Music, Tandy output,
overlapping effect scheduling and full recorded-audio parity remain unfinished.

Final validation passed all 27 public CTest suites; the separate private RNG
integration test skipped without configuration. Added regressions cover full
WRITE-plus-CALL flows, raw call tokens, selector persistence, high-byte clearing,
invalid profiles/operands, automatic window playback and restart, and protection
of character memory when a malformed selector binding overlaps it. Native loop
counter offsets 38 and 40 were separately observed; other loop fields fail.
All 19 private effect comparisons passed again after the final bounds change,
and the default desktop audio-device check passed. Hygiene and attribution
checks passed. No original game installation was modified.

## Startup sound and title timing (2026-10-17)

Read-only research used the same pinned Steam START.EXE and GAME.OVR hashes
listed above. A private copy ran under DOSBox 0.74-3 with sound enabled, fixed
10000 CPU cycles, the SDL disk audio backend and a 45-second wall-clock limit.
The capture contained 4,431,872 signed sample values, all zero. A separate
synthetic COM program produced a PIT speaker tone through the same DOSBox/audio
configuration: 1,606 nonzero values in 40,960 captured values. The earlier dual
comparison configuration had `nosound=true` and cannot establish silence.
These observations support preserving silence for this installation's observed
startup, not a claim that all editions or later scenes lack music. No recorded
audio, executable code, original text or image payload is redistributed.

The known resident startup call at image 0x11d selects overlay 0x19, entry 0x627
(GAME.OVR base 0xd4). It loads TITLE records 1 and 2 in order. The first wait
multiplies five by 100; the second adds 500 to the clock. The resident clock at
0x7ccf computes hundredths from DOS time fields. Both waits poll for a key.
The routine then enters its credit sequence at overlay offset 0x206; the second
title is not an indefinite menu backdrop in this path.

A bounded private Unicorn probe executed the original title control flow with
mocked clock, keyboard, string and graphics helpers. Three cases covered no key,
a key during the first title, and a key during the second title. Each reached
the credit entry after loading [1,2]; the selected key shortened only its title's
wait. The routine made no direct sound request. Graphics helper internals,
credit pages and subsequent menus were not executed by this probe; it does not
prove full startup parity or recorded frame timing. Existing SSI Engine
`ClassicMode.showNextTitle` at revision
`51d4b6d409fa798754f5ff2ccb249f3c0b6d029b` independently uses a five-second
schedule. It remains research-only; no Java scheduler was copied.

The engine now selects a bounded title period from the game profile. Pool uses
5000ms and presents both titles before a separate menu. Missing/zero timing
retains the old final-title menu behavior for other callers. Any ordinary key
can skip one timed title; repeat key events are ignored. Menu commands C/B/L/E
are consumed only at the menu. Returning from party management or an error does
not replay titles; F10 does. Title deadlines use elapsed time and bounded SDL
waits rather than a polling loop. The menu continues to identify engine-save
loading and exploration accurately. Original credit pages, animated effects,
border artwork, exact menu layout and native character/save import remain
unfinished; this is not a complete original startup reproduction.

Validation passed the three focused startup/profile/renderer suites and then
all 27 public CTest suites. The separate private RNG integration suite skipped
without its configuration. Synthetic SDL regressions exercise automatic title
timeouts, per-title skipping, consumed skip keys, F10 replay, a load shortcut
and direct menu recovery after an empty-party error. Profile rejection checks
remain active in release builds. The installed TITLE.DAX records 1 and 2 also
passed the bounded 320x200 decoding check. Hygiene and attribution checks passed.
All builds used the shared lock, two workers and fixed timeouts. No owned game
or build processes were left running.

Tool friction: the live desktop helper rejected key actions after detecting
concurrent user input, so interactive screenshots were not used as a timing
oracle. The isolated recording and native control-flow probe supplied that
evidence. WSL sandbox access-denied responses were resolved by approved bounded
calls; no build parallelism was increased.

## Recovered early DOS Tandy score (2026-10-17)

The installed Steam overlay identifies itself as version 1.3. Its traced startup
does not request music. Early DOS versions differ: a separate private preservation
copy from https://archive.org/details/POR_EGA contains START.EXE (47,936 bytes,
SHA-256 `14ecf7584a43bd1ab36e04af12d9e9a3a00c5899533c853304e1e0e8af164fa2`)
and GAME.OVR (232,379 bytes,
`cbb4cdc4d245e7a3abfc1c9d2323c93d146d67b061a04d1dea0b495c829247fe`).
The exact release number is not established. ZIP fingerprints match the archive's
metadata; Defender found no threats before and after extraction. Those checks
do not guarantee safety. Only bounded static analysis was used; no downloaded
program, installer or original game code was executed.

This executable is an unpacked MZ image with a 944-byte header. Its SSI audio
segment starts at resident image offset 0x27f0. The Tandy entry table is at
segment offset 0x4ab, duration bytes at 0xba, and twelve frequency words at 0x3f3.
Entry 12 begins a three-tone score. Entries 13 through 19 reach later score
sections; these are not established as eight independent songs. Startup overlay
0x19 is at GAME.OVR offset 0x7b, size 0x814, entry 0x600. At 0x61f it tests Tandy
mode, then calls the sound wrapper with main-data word 0x2642 = 13. The wrapper
subtracts one and selects entry 12. The main data segment is 0x740.

The independent decoder accepts explicit table locations and bounded segment
data, without title names, executable scanning, or proprietary note tables in
the core. It handles six control commands, shared (not nested) calls, three-tone
chords, rests, duration scaling, transposition, attack/release envelopes, sweeps,
modulation and counter loops. Invalid fields, returns without calls, unsupported noise,
out-of-bounds data and nontermination fail explicitly. Work is limited to 200,000
operations, 30,000 ticks and 120 seconds; output sample allocation is bounded.
Existing PC-speaker decoding is unchanged.

MAME's BSD-3-Clause SN76496 implementation was evaluated at immutable revision
`23a72a0f354fdd61532ad86bce782ccc150fd18d`. Importing its device framework would
add unrelated emulator infrastructure. No source was copied; the research-only
reference is recorded in the attribution registry. The preview approximates
three square waves and logarithmic attenuation, without noise or analog filtering.
Hardware phase accuracy and recorded-audio parity are not claimed. Original music
and all generated audio remain private, outside the public repository.

Private entry-12 decoding completed with 23,905 frames at PIT divisor 5041:
4,847,751 mono samples at 48kHz, approximately 100.995 seconds. All three voices
contain varying pitches and non-muted frames. The WAV preview SHA-256 is
`4e1a517be9ad8e276fd022d79fdcf8585a1020828d668978b34798555fe065fe`.
This is a decoder-generated approximation, not a recording or audible parity
claim. Startup integration, reference-recording comparison and other editions
remain unfinished. In particular, the Steam 1.3 startup has not gained music.

The real score exposed a zero-duration envelope record: it disables subsequent
envelope steps rather than constituting malformed data. A synthetic regression
now covers that behavior. Synthetic checks also cover three-voice chords,
instrument calls, rests, finite loops, invalid returns/calls/selectors/tables,
noise rejection, deterministic PCM, amplitude bounds and nontermination limits.
The bounded build and all 27 public CTest suites passed; the separate original
RNG integration test skipped without configuration. Hygiene and attribution
checks passed. No downloaded code was executed, no proprietary input/output was
added to the repository, and no owned build or game process remains running.

## Edition-selected startup playback (2026-10-18)

The existing `--audio <file>` window option now selects the recovered early
executable by its profile-provided FNV-1a64 fingerprint `11213555104201169956`.
This is a non-cryptographic edition selector, not a safety or authenticity check;
the SHA-256 and quarantine checks above remain separate. The optional `tandyMusic`
profile object supplies the raw file segment offset (11168, including MZ header),
effect and score tables, note tables, entry and timing. Other files keep the
existing EXEPACK speaker path. Unsupported unpacked editions fail explicitly.
The core has no Pool title, filename or executable-address heuristics.

Selecting the early file with `--startup` plays its Tandy score during the title
sequence. Entering the menu stops it; returning from party management or a menu
error does not replay titles/music. F10 clears the audio queue and replays the
startup from the beginning. F8 remains an explicit speaker-effect audition and
replaces queued music. Effects continue using the matching edition's speaker
table. The installed Steam v1.3 file never receives invented startup music.
Users can keep the early executable as a non-executable `.data` file outside
the installation and select it explicitly; no installation replacement is needed.
This audio selection does not establish compatibility for other early-edition
campaign/ECL data. Noise synthesis and recorded-original waveform parity remain
outside the supported tone-preview subset.

Synthetic regressions cover Tandy submission/replacement on SDL's dummy audio
device, timed title playback, F10 replay, unmodified game state, absent music
configuration and invalid profile bounds/fingerprints. All 27 public suites
passed again; the separately configured original RNG integration suite skipped.

A private wrapper called the actual explore CLI main function, using installed
TITLE/GEO/ECL data and each audio file in turn. Scripted SDL keys restarted after
two seconds, skipped both titles after two more, and exited after one second at
the menu. Both runs returned zero using the default desktop audio device, then
both returned zero again with SDL's disk backend. The early-source capture had
1,254,312 nonzero bytes in 1,736,704 bytes, with its final 65,536 bytes silent;
the v1.3 capture had zero nonzero bytes in 1,794,048 bytes. This verifies engine
output and menu stopping, not parity with a recording of the original player.
All probes had fixed timeouts and the shared compile lock. Only our engine code
ran; downloaded programs and original installation files were untouched.

## Original new-game opening (2026-10-18)

The installed DOS v1.3 resources now drive a profile-selected campaign Begin.
Pool's initial GEO/ECL record is zero in GEO3.DAX/ECL3.DAX; its own initialize
entry writes the initial (15,1), west, position. The adapter resolves the explicit
archive families, keeps the created party and its RNG continuation, clears fresh
script state, and delegates presentation through the existing host interfaces.
Preview/JSONL launches retain their explicit map/ECL behavior.
The opening's sprite 12, head 8, body 9 and wall definition zero are in group 3,
separate from the group-1 preview/creation resources. The profile now binds that
startup artwork explicitly, and the SDL host switches resources on Begin/load
while retaining creation portrait sources. A synthetic SDL regression uses an
opening picture absent from the launch assets to catch incorrect resource reuse.

The opt-in `goldbox_campaign_startup_tests` replay completed 761 instructions,
42 text callbacks and eight deterministic menu choices with original wall,
sprite and head/body decoding. The first-choice city-tour route ends at (0,4),
west, in program zero. Repeating from the same created-party state produced an
identical serialized result. Saving and restoring preserved all serialized
script memory/strings, map/program bindings, wall selection, party and RNG state.
The final fixture uses a male dwarf fighter named TEST, lawful good, matching
the reference driver's creation choices. The native clock-seeded ability scores
are not compared by this startup check.

`scripts/pool_startup_reference.py` then drove the authorized installed game in
a fresh private DOSBox copy: create, keep the portraits/icon, add the character,
Begin and advance the tour. It waits for fingerprinted native menu captures
before creation input, reads only its owned process, and stops at a stable
matching checkpoint. Original program/menu resources were never patched.
The native comparison passed for:

- Position (0,4), west; the native direction byte 6 normalizes to west (3).
- Opening flags 0x4A17=1, 0x4AC4=0, 0x4AC5=1. All three were zero before Begin.
- Interpreter return address 44681, corresponding to code base 39168 plus
  engine EXIT offset 5512 plus its one-byte instruction size.

The engine checkpoint SHA-256 is
`8ca96f8e31d6ef5e58e0a53a9ef54796e98a28b1ac516a9b920934ea65bc3973`.
The original comparison recorded `matched=true`, `fullParityVerified=false`
and verified owned-process cleanup. Its private workspace artifact is
`.cache/goldbox-validation/startup-reference-j00bar_s/comparison.json`, outside
the checkout; screenshots and copied game resources stay alongside it privately.
The original START.EXE/GAME.OVR and installed DOSBox are fingerprinted before
launch. The installed hardware setting `tandy=on` is retained while output is
muted. Quarantined downloaded programs were not executed.

The complete public suite passed (28 passed, two private-resource checks skipped).
The installed opening check and live native comparison passed separately.
The SDL regression also exercises campaign Begin, F10 restart and continuing
through a sound request without optional audio resources. Attribution and
hygiene checks passed. Builds used the shared lock, two workers and fixed limits.

Reproduce by setting `GOLDBOX_POOL_INSTALLATION` to the authorized GAME/POOLRAD
directory, then running `build/tests/goldbox_campaign_startup_tests` with the
Pool profile path and redirecting its JSON output to a private checkpoint file.
On native Windows, invoke `scripts/pool_startup_reference.py` with four positional
paths: installation root (containing GAME and DOSBOX), existing private parent
directory outside the checkout and installation, fingerprinted private resident
load image used by the RNG laboratory, and that engine checkpoint JSON file.

This establishes a narrow opening checkpoint, not full campaign parity. Native
party scores/RNG streams, all remaining RAM, text pagination, pixels, sound,
timing and original SSI saves are outside this comparison. Per-title temporary
memory resets on NEW_ECL and later campaign transitions remain unverified.
Automatic graphics changes for later programs/resource groups are also pending.

## Location routing and boundary dependency (2026-10-24)

The static scanner now follows numeric COMPARE and all six IF conditions using
known profile-bound coordinates. False conditions skip one decoded instruction;
AND refreshes the comparison flag. Synthetic coverage includes equal selectors
at different locations, all condition outcomes, stale comparison flags, truncated
skipped instructions and conflicting opcode definitions. No byte-pattern search
is used. Unknown memory remains unknown and its address is reported.

Scanning the authorized GEO1/ECL1 record 18 identifies 179 selector-bearing cells.
Their move routes reach COMPARE at code offset 185, reading logical 0x6DD5
against zero. SSI Engine's pinned Pool configuration names this TRIED_TO_LEAVE_MAP;
its DungeonMovementHandler prepares the flag and runs MOVE before movement,
then SEARCH afterward. This is research-only evidence, not a copied adapter.

The installed original WRITE_MEM handlers independently establish that 0x6DD5
uses the separate word buffer at DS:0x49D6, offset 0x5AA. The original routine at
GAME.OVR 0x16D58 (overlay 0x72, entry 0x68D) clears this flag, queries the source
edge, and sets it if a nonzero door result would step outside 0..15. A bounded
Unicorn probe passed 32 cases: four directions, boundary/interior positions and
four synthetic door-query results. Only the door query was mocked; original
instructions prepared the flag and retained the clamped source coordinates.
The probe does not establish original door decoding or complete movement order.

A separate code-driven DOSBox run reproduced the existing opening checkpoint,
then pressed forward while facing west at (0,4). The original reached (15,4),
west, displayed the next area's description, changed 0x4A17 from 1 to 0, and
returned at native PC 39716. Movement-block and exit-attempt words were both zero
at the settled checkpoint. This does not imply the flag stayed zero during the
transition. The run used a private installation copy and cleaned up its own
process; it did not modify the installed game or user saves.

Private evidence lives outside the checkout at
`.cache/goldbox-validation/startup-reference-nqg4qhrk/comparison.json` and
`.cache/goldbox-validation/original-boundary-flag-report.json`. The private
`probe_movement_lifecycle.py` replays the fingerprinted startup adapter and its
fixed keypad sequence; `probe_original_boundary_flag.py` repeats the 32 native
routine checks. Images, memory snapshots and original executable bytes remain
private. Public startup observations now include the two word-buffer values,
with synthetic address, width, truncation and pointer-bound regressions.
The updated public capture path also passed a second live run at
`.cache/goldbox-validation/startup-reference-x96jmvsn/comparison.json`, reproducing
the same position, facing, flags and return PCs. Capture hashes differed;
clock-seeded native party rolls and timing are outside this comparison.

The serialized two-worker build and full CTest run passed 30 suites; the native
Windows RNG integration suite skipped under WSL. The original-resource opening
suite ran with the authorized installation. Focused Python tests also passed
on Windows using a workspace-local temporary directory. Attribution and hygiene
checks passed. The private build/test log is
`.cache/goldbox-validation/location-routing-20261024-tests.log`.

The remaining runtime gap is explicit: the engine currently rejects boundary
movement before ECL, executes MOVE at the destination after successful movement,
and does not automatically invoke SEARCH. It therefore cannot reproduce this
original boundary transition. A profile-selected lifecycle fix must account for
movement blocking, program/map changes, subsequent search and the outstanding
graphics-group binding; this evidence does not establish full map-18 parity.

## Profile-selected movement lifecycle (2026-10-24)

The preceding movement-order gap is now addressed by an optional profile
binding, shared by SDL, terminal and JSONL controls. Each attempt prepares block
and exit state, runs source-cell MOVE, applies a permitted source-edge step,
then runs destination SEARCH, including blocked attempts. A program transfer
replaces the physical-step continuation; self-transfers are counted explicitly.
Script repositioning and map loads are validated before continuation. The
compound operation shares its instruction budget and retains the existing
explicit partial-progress failure policy. Unconfigured sessions keep the older
prototype behavior. Synthetic tests cover these paths, closed doors, all four
boundary directions, malformed bindings, unsupported code, limits and saves.

Independent native probes refined the exit flag: the original bounds, wall and
door queries, running on synthetic GEO planes, passed 64 cases. An open boundary
sets the flag even when its door bits are zero. A source wall with zero door
bits does not. The probe used original code without callback mocks; its private
script/report are `probe_original_boundary_edge.py` and
`original-boundary-edge-report.json` under `.cache/goldbox-validation`.

Pool's profile also binds native CALL 0xC01E and wall context 0xC04E. The original
CALL/forced-step routines passed 36 bounded cases: advance in the facing direction,
wrap at 16 cells and refresh square/wall context without collision checking.
The two map lookups returned synthetic selector 37 and wall 7; presentation was
disabled. Private script/report: `probe_original_forced_move.py` and
`original-forced-move-report.json`. The independent host implementation gets its
token/register from the profile. The original routine's coordinates and context
writes were checked; this is not a graphics or timing comparison.

The original route uses LOAD_AREA_MAP `(255,255,127)` as a temporary no-resource
area. The shared adapter now derives inactivity from that existing area tuple,
keeps the prior map cached, clears presentation images and hides/rejects traversal
of inactive geometry. A later dungeon load reactivates it. Synthetic save and
renderer checks cover this state. Other unsupported area modes still fail.

The opt-in original-resource test still completes the same 761-instruction
opening and exact deterministic engine save round-trip. Its additional boundary
probe now reaches imported map/program 20 at (15,4), west, then stops at code
offset 421 with `Unknown ECL opcode 29`. Pool's pinned configuration assigns
0x1D to one-operand PARTY_STRENGTH; its SSI Engine handler is empty. Another
title's zero-operand INPUT_RETURN meaning must not be substituted. No speculative
party-strength implementation or input dialect was adopted.

This stop is reported separately as `boundaryProbe.status=unsupported` in
`.cache/goldbox-validation/movement-lifecycle-engine-checkpoint.json`. It is a
regression for bounded execution and the known stop, not a successful original
boundary comparison. The original later checkpoint has opening flag 0x4A17=0,
while the engine's stopped probe retains 1. SSI Engine clears 0x4A00..0x4A1F and
0x6E79..0x6E82 on transfers, but the original's clear/restore behavior and per-area
persistence have not been established. Verify transfer-local memory before
assuming the stopped PARTY_STRENGTH path is the correct next path; do not copy a
blanket reset or invent a calculation. The graphical adapter also retains its
explicit guard against changing programs without corresponding graphics bindings.

The full serialized two-worker build and CTest run passed 30 suites; the native
Windows RNG suite skipped under WSL. Its log is
`.cache/goldbox-validation/movement-lifecycle-full-tests.log`. A fresh native DOS
run matched the unchanged opening position, facing, three flags and return PC:
`.cache/goldbox-validation/startup-reference-77qnernj/comparison.json`, with owned
process cleanup confirmed. That comparison does not include the unsupported
engine boundary probe. Hygiene and attribution checks passed. Private captures,
original bytes and user saves remain outside the checkout and unchanged.

## Original interpreter transfer reset (2026-10-25)

The complete installed interpreter reset routine (overlay 0x2B, entry 0x01B6)
was executed under bounded Unicorn with a synthetic five-entry header and
sentinel word buffers. Four checks used native preserve-guard values 0, 1, 2
and 255. With zero, the routine cleared exactly the virtual numeric ranges
0x4A00..0x4A1F and 0x6E79..0x6E82; adjacent words remained unchanged. Each
nonzero guard preserved both ranges and reset itself to zero. No instruction
callbacks replaced original code. Private script/report:
`.cache/goldbox-validation/probe_original_transfer_reset.py` and
`original-transfer-reset-report.json`. Existing native movement snapshots have
the guard zero before and after the program-zero to program-20 transfer.

The shared session now clears profile-defined numeric ranges after successful
NEW_ECL resolution and validation, before initialize/move/search. Failed
resolution retains them; an absent binding retains prototype behavior. Pool's
profile supplies the two verified ranges. Synthetic tests cover exact bounds,
follow-up execution, invalid ranges, protected-memory overlaps and save restore.
Engine restore preserves saved RAM without executing a program reset; the
native save format and its one-shot preservation path are not implemented.
This does not establish string-buffer or all interpreter-scratch parity.

The original-resource replay still completes the 761-instruction opening.
Its boundary probe now has 0x4A17=0, matching the original later checkpoint;
global flags 0x4AC4=0 and 0x4AC5=1 remain unchanged. It still stops at imported
map/program 20, (15,4), west, bytecode offset 421, unsupported PARTY_STRENGTH.
Checkpoint: `.cache/goldbox-validation/transfer-reset-engine-checkpoint.json`.
The prior flag mismatch is resolved; complete boundary, party-strength and
graphics-group parity remain unfinished. No proprietary bytes were added to
the repository or modified in the installed game.

The serialized two-worker full build/CTest run passed 30 suites, with the native
Windows RNG integration suite skipped under WSL. The authorized original-resource
test ran, including the corrected boundary-flag regression. Log:
`.cache/goldbox-validation/transfer-reset-full-tests.log`. Hygiene and attribution
checks passed. The four native-reset checks are separate from full-game parity.

## PARTY_STRENGTH arithmetic and host-input gate (2026-10-25)

The installed interpreter's opcode dispatch compares 0x1D at overlay 0x1F
offset 0x3033 and calls handler 0x12C6. This establishes the handler through
executable dispatch, without scanning ECL bytes for instruction patterns.
The handler decodes one numeric destination, walks the party list, computes
one contribution per member and writes the byte total through its ordinary
operand writer. It does not read the Strength ability or filter by status.

For unsigned native record bytes H=0x11B, A=0x111, T=0x110, M=0x9B and C=0x96:

```
member = floor((H + 5*max(A-60,0) + 5*max(T-39,0) + 8*M + 4*C) / 10)
party = sum(member) modulo 256
```

Rounding occurs per member before summation. The empty list produces zero.
SSI Engine's pinned Pool CharacterValues identifies H as current HP and M/C
as mage/cleric levels. Gold Box Explorer's pinned FruaCharacter annotations
identify T/A as current THAC0/AC, encoded as 60 minus the displayed value.
Those labels corroborate the fields; neither reference establishes their
complete derivation from equipment, effects or character progression.

Private `probe_original_party_strength.py` ran the original handler, operand
decoder and destination writer with synthetic linked records and ECL operands.
All 205 cases passed: empty party, each of five bytes varied over 14 boundary
values, all-max parties of sizes 1..6, and 128 deterministic generated parties.
The checks cover threshold subtraction, per-member division, multi-member
accumulation and byte overflow. Each invocation had a 15,000-instruction and
one-second bound; the batch had a 45-second deadline. Every call returned and
advanced the synthetic instruction PC by four bytes. No instruction callbacks
replaced original code. Private report:
`.cache/goldbox-validation/original-party-strength-report.json`.

The runtime gate remains intentional. `Character` has current HP, but does not
represent current AC/THAC0 or per-class levels, and the selected-character
adapter supplies none of these missing fields. Deriving levels solely from a
class name, assuming unequipped combat stats, or storing an arbitrary cached
rating would not reproduce the original state. The next implementation needs
typed current combat/level state and verified creation/update bindings before
enabling this opcode through the per-game adapter. No complete combat engine
is required to expose those inputs, but equipment/effect behavior cannot be
silently assumed. The existing original-resource test continues to enforce
the explicit opcode stop at offset 421. This research changes no runtime code
and establishes arithmetic interoperability, not complete boundary parity.

## Character class-level inputs (2026-10-25)

Private `probe_original_creation_levels.py` exercised the original creator's
class-selection level-initialization block, overlay 0x78 offsets 0x0BA7..0x0CB8,
with zeroed synthetic character records. All 17 native class-code cases passed:
single classes set their own level byte to one; multiclasses set each component
to one and leave other class bytes zero. Each case had a 1,000-instruction and
100 ms bound, with a 20-second batch deadline and no substituted instructions.
Report: `.cache/goldbox-validation/original-creation-levels-report.json`.
This checks the isolated initialization block, not complete character creation
or the unsupported native classes absent from our rules.

The shared creation rules now optionally select a starting level. Pool selects
one; both raw-roll creation and finish-character creation publish complete
component level maps without additional random draws. Saved characters retain
their explicit levels. Legacy state remains unknown instead of being upgraded
from a class name. Validation rejects missing/extra components and zero levels.

Pool's profile selects four supported numeric field bindings: cleric 0x96,
fighter 0x98, magic-user 0x9B and thief 0x9C. The existing read-only selected-
character adapter returns a known component level or zero for a known inactive
class, and rejects unknown state, oversized byte values, writes and conflicting
bindings. Core code contains no Pool class codes or offsets. Current AC/THAC0
initialization and updates remain the missing PARTY_STRENGTH inputs; the opcode
and original boundary checkpoint still retain their explicit unsupported gate.

The focused five-suite run passed, followed by a full serialized two-worker
build and CTest run: 30 suites passed and the native Windows RNG integration
suite skipped under WSL. Original-resource opening/replay and the explicit
boundary-stop check ran successfully. Synthetic coverage includes all eleven
supported single/multiclass choices, both creation paths, exact level save/load,
invalid/incomplete component maps, legacy unknown state, alternate starting
levels, selected-character reads and read-only/overlap/byte-range failures.
Logs: `.cache/goldbox-validation/class-levels-focused.log` and
`class-levels-full-tests.log`. Hygiene and attribution checks passed. No original
save files or installed resources were modified.

## Unequipped combat state and executed PARTY_STRENGTH (2026-10-25)

The original creator initializes encoded base AC to 50 (displayed AC 10) and
computes base THAC0 from active class levels. Current values are populated by
the complete resident refresh routine at image offset 0x1758, rather than
copied directly from those bases. Its equipment/effect paths are distinct.
Private probes use the actual creator base-THAC0 loop and complete refresh,
with synthetic records containing no equipment or effects.

`probe_original_starting_combat.py` passed 198 checks: all eleven supported
class combinations at level one, each with Dexterity 3..20. Base/current THAC0
was 20 throughout. Starting current AC was 10 plus the observed Dexterity
modifier. A further 629 original refresh invocations covered Strength and
Dexterity 3..25 and every exceptional-Strength percentile 1..100. These results
were asserted against the same unequipped policy: no Strength-dependent THAC0
change, and the verified Dexterity AC adjustments. Per-call execution limits
were 20,000 instructions/100 ms; batches had fixed deadlines. No original
instructions were replaced. Private reports:
`.cache/goldbox-validation/original-starting-combat-report.json` and
`original-unarmed-combat-report.json`. This does not verify armed attacks,
equipment bonuses, spell effects or higher-level combat progression.

Shared creation rules now optionally supply `startingCombat`; Pool selects
these verified level-one unequipped settings. Both creation paths compute
current combat state without extra RNG draws. Saves retain explicit normalized
AC/THAC0, while legacy records remain unknown. The broad normalized bounds are
independent of the narrower native byte encoding required by the rating adapter.
No title-specific class IDs, addresses or creation defaults enter the VM.

Profile `partyStrength` selects a weighted-rating dialect and class IDs. The VM
opts into unary opcode 0x1D and writes the host's byte result through the existing
typed, protected destination interface. The host implements the independently
verified 205-case formula above, using current HP/combat/levels. It rejects
unknown or unrepresentable state; it does not infer defaults during save load.
Synthetic regressions check native rounding/overflow cases, unknown inputs,
dialect conflicts, invalid operands and read-only destinations. Creation and
save tests cover the Dexterity AC boundaries and alternate rules without that
ability. The earlier alternate-rules fixture explicitly disables the inherited
combat policy after removing its defense ability.

The authorized-resource boundary probe now executes through EXIT at offset 615
in map/program 20, at (15,4), west. Block and exit-attempt are zero; flags are
0x4A17=0, 0x4AC4=0, 0x4AC5=1. The opening remains 761 instructions, 42 text calls,
eight menus, (0,4), west, EXIT 5512, with deterministic engine save/replay intact.
Private checkpoint: `.cache/goldbox-validation/party-strength-engine-checkpoint.json`.
This is headless execution with an engine-created party. A prior native capture
after a subsequent turn has return PC 39784, consistent with EXIT 615 plus one
at base 39168, but its input/facing differ; no matched native boundary comparison
or complete party equivalence is claimed. Graphical program/graphics rebinding
still has its explicit guard and remains unfinished.

The final serialized two-worker build/CTest run passed 30 suites, with the native
Windows RNG integration suite skipped under WSL. The original-resource opening
and executed boundary checks ran successfully. Log:
`.cache/goldbox-validation/party-strength-full-tests.log`. The initial focused
run found the alternate-rules fixture's removed-ability configuration; this was
corrected before the full green run. Hygiene and attribution checks passed.
Private original bytes and captures remain outside the public checkout; the
installed game and user saves were not modified.

## Matched boundary state and scene checkpoint (2026-10-25)

Fresh bounded native runs now compare the same westward boundary input, without
the subsequent turn used in the earlier exploratory capture. From the matched
opening at (0,4), west, keypad-forward and dialogue acknowledgements reach
(15,4), west. The tracked flags match the engine: 0x4A17=0, 0x4AC4=0, 0x4AC5=1.
Both movement-control words are zero. The native return PC is 39784, matching
39168 + EXIT offset 615 + one. The native resource-group byte is 2. These are
matching observations; native map/program record IDs are not independently read
by this capture adapter.

The native 320x200 capture's rectangle (24,24,88,88), converted to RGB in row-major
channel order, has FNV-1a 64 fingerprint `15040589715104877255`. The shared C++
wall compositor produces that same fingerprint from the engine boundary state,
group-2 wall banks, original palette and configured backdrop. The optional
original-resource CTest now asserts this numeric fingerprint when the view
library is available. No original pixels are embedded in public fixtures.
This checks the composed dungeon view, not an entire SDL window or full screen.

Private evidence: `startup-reference-96srs_fp/comparison.json`,
`parity-scene-engine.json` and `boundary-parity-2026-10-25.json`, under
`.cache/goldbox-validation`. The updated public reference harness was then run
separately: `startup-reference-2rec5fm3/comparison.json` reports openingMatched,
boundaryMatched, matched and cleanupVerified all true. Its two stable boundary
captures at return PC 39784 have the same scene fingerprint. The live harness
continues to compare state only; the screenshot fingerprint comparison is a
separate private check plus the original-resource compositor regression.

Completed engine boundary probes now cause that harness to send one forward
input, acknowledge dialogue within ten observations and require two stable
matches. An opening-only match cannot pass a requested boundary comparison.
Synthetic tests reject divergent coordinates, facing, flags, movement words,
resource group, return PC and malformed boundary checkpoints. The five Python
tests and three focused CTest suites passed; hygiene and attribution passed.
Build log: `.cache/goldbox-validation/parity-final-focused.log`.

Party rolls/stats were not synchronized with the native runtime. Full-screen
UI, dialogue pacing, input scheduling, sound/timing, other locations and native
save formats remain unverified; `fullParityVerified` stays false. No installed
game files or user saves were changed. Only owned private reference processes
were launched and cleaned up.

## Program graphics rebinding (2026-10-25)

Private native snapshots show the active resource-group byte at DS:0x5376
changing from 3 before the opening boundary to 2 afterward. The latter snapshot
contains ECL2/WALLDEF2/8x8d2 filenames; stale group-3 strings elsewhere in memory
are not treated as active bindings. Archive inventory places program/map 20 in
ECL2/GEO2 and opening program/map 0 in ECL3/GEO3.

Twelve bounded original-code checks executed the common graphics filename
builder (overlay 0x3D0, entry 0xA2), stopping at 0x16A before archive I/O.
Synthetic PIC, SPRIT, HEAD, BODY, WALLDEF and 8x8d family prefixes each selected
the exact group-2 or group-3 stem according to DS:0x5376. Each call had a
5,000-instruction/100 ms bound; no instructions were substituted. Report:
`.cache/goldbox-validation/original-graphics-group-report.json`. Original bytes
and disassembly remain private. This verifies the common selection mechanism
without guessing from numbered filenames. Shared 8X8D1 remains the existing
common-tile configuration.

The Pool profile now explicitly binds program 20 to PIC2, SPRIT2, HEAD2, BODY2,
WALLDEF2 and 8X8D2, with shared 8X8D1. The shared engine has no title-specific
resource search: it notifies the host after candidate validation and before
publishing the new program. The application preloads explicit immutable bundles;
SDL switches pictures, portraits and wall sources and clears stale presentation.
Save restore selects its saved program and validates decoded wall banks before
publishing. Unconfigured programs continue to fail explicitly.

Synthetic tests cover malformed/duplicate bindings, failed host notification,
transfer selection, SDL blue-to-red picture pixels, restart and saved-program
rebinding. The first renderer fixture tried to overwrite its own earlier save;
the existing refusal policy correctly rejected that. The corrected fixture saves
only the destination before restart/reload; save-overwrite behavior is unchanged.
The authorized-resource test parses all seven group-2 archives and decodes the
selected wall banks, checking active-map wall references. The original boundary
still reaches EXIT 615 in program/map 20 with the established flags. This does
not establish equivalent native party state, matched original scene pixels,
UI/timing parity or support for further resource groups.

The serialized two-worker build passed all 30 runnable CTest suites; the native
Windows RNG suite was skipped under WSL. Four focused suites also passed. Log:
`.cache/goldbox-validation/program-graphics-full-tests.log`. Installed assets
and user saves were not modified; public tests contain synthetic graphics only.

## Actual original-mode SDL frame comparison (2026-10-25)

The optional original-resource test now drives the real SDL host from the
completed opening through the west boundary, using queued forward/Return/escape
inputs. Linux renderer builds use the dummy video and software rendering
backends, with the existing 30-second test deadline. A linker wrapper reads the
rendered surface before presentation and samples logical pixel centers for
comparison with the native 320x200 frame. It stores only numeric RGB FNV-1a 64
region fingerprints in test output, never extracted pixels or game text.

Reference: the stable boundary capture at
`.cache/goldbox-validation/startup-reference-2rec5fm3/boundary-03.png`.
Engine output and private region comparisons are `parity-window-engine.json`,
`parity-window-regions-before.json` and `parity-window-regions.json` in the same
private validation cache. This run reused that matched native checkpoint; it did
not launch another native game session or synchronize its character RNG.

The top border, 88x88 dungeon scene, party headers and selected TEST name matched
immediately. AC did not: the renderer still showed a question mark despite the
now-known normalized current combat state. The shared display now draws that
value right aligned, preserving `?` only when combat state is unknown. Its native
AC-region fingerprint now matches `10896576774587328698`. No game-specific
condition or default AC was introduced. Synthetic rendered-pixel checks cover
AC 10, zero, -2 and unknown state, including alignment and color. The installed
test asserts all five matching region fingerprints after the transition.

Confirmed remaining differences are dialogue-region content/layout, command
labels and the position/clock line. The engine omits the native clock and uses
its current exploration command labels. The HP region also differs, but this
party's rolls were not synchronized, so that is not evidence of a glyph or HP
display defect. Other screen pixels and interaction/timing equivalence remain
unverified; complete screen parity is not claimed. The next presentation check
should resolve the dialogue-region mismatch before expanding gameplay controls.

The two focused suites passed, followed by the full serialized two-worker run:
30 suites passed; the Windows-only original RNG integration suite was skipped
under WSL. The installed-resource window test ran successfully. Logs are
`parity-window-fixed.log` and `parity-window-full-tests.log` in the private
validation cache. Hygiene and attribution checks passed. No user saves or
installed resources were modified, and no user-owned windows were closed.

## Boundary dialogue exact-width wrap (2026-10-25)

A temporary private capture of the actual SDL test frame localized the mismatch.
The first two dialogue rows matched native text and color. Native trimmed row
lengths were 37, 35, 34 and 30 characters; the engine produced 37, 35, 38 and 26.
The last word on the engine's third row exactly filled column 38. The native
formatter moved it to the fourth row, while retaining the already matching
37-character first row. Colors and the configured 38-column region were correct.

The shared formatter now wraps a word appended to an occupied row on an exact
fit as well as overflow. It retains existing bounded handling of a full-width
initial word and long unbroken tokens; native behavior for every such case is
not claimed. Synthetic regressions cover exact-fit wrapping, one spare column,
inline colors, split PRINT continuation and avoiding an empty initial row.
The pinned SSI Engine AbstractStateRenderer was consulted as a research-only
reference; its strict-overflow comparison does not establish this native edge
case. No upstream wrapping implementation was adopted.

The actual SDL boundary dialogue region now matches native RGB FNV-1a 64
fingerprint `3056256239859949209`. The installed-resource test asserts it along
with the existing scene, top-border, party-header, name and AC fingerprints.
The temporary capture instrumentation was removed; public tests output only
numeric fingerprints. Native text reconstructed from glyph cells and private
captures remain outside the checkout in `.cache/goldbox-validation`:
`dialogue-cell-text-private.json`, `parity-dialogue-engine.bmp` (before fix) and
`parity-dialogue-engine.json` (corrected fingerprints). The native reference is
still `startup-reference-2rec5fm3/boundary-03.png`; no new original session or
synchronized-party comparison was needed for this presentation correction.

The two focused suites passed. Remaining confirmed screen differences are the
command bar and position/clock line; HP still has unsynchronized party inputs.
Other dialogues, pagination, typewriter pacing and audio/timing remain open.
This verifies the captured boundary dialogue rather than complete game text
compatibility. User saves, artwork and installed game resources were unchanged.

The final serialized two-worker run passed 30 suites; the Windows-only original
RNG suite was skipped under WSL. The installed-resource renderer check ran and
asserted the corrected dialogue fingerprint. Hygiene and attribution passed.
Private logs: `parity-dialogue-focused.log` and `parity-dialogue-full-tests.log`.

## Read-only native clock presentation (2026-10-25)

The unmodified native position formatter reads script globals 0x49C7 (minute
units), 0x49C8 (minute tens) and 0x49C9 (hours). Its two-character components are
zero-padded or truncated from the leftmost digits, including unusual values.
Fifty-four bounded native-code cases verified hours 0..255 and minute totals
0..255, using synthetic word buffers and stopping before rendering. Each call
had a 30,000-instruction/100 ms limit. The initial probe required correcting its
resident CS and initializing the separate search-flag buffer; only the final
54 passing cases support this conclusion. Private evidence:
`.cache/goldbox-validation/original-position-clock-report.json`.

Optional profile `originalDisplay.clockAddresses` supplies those three addresses
to the shared display. It reads ordinary saved script memory and does not invent
time advancement. The actual SDL boundary position/clock region now matches
native fingerprint `13091622508662674760`. Synthetic cases check the observed
formatting, absent bindings, invalid direction and malformed profile addresses.
The three focused suites and full 30 runnable suites passed; the Windows-only
RNG integration suite was skipped under WSL. Final focused run:
`parity-clock-final-focused.log`; broad run: `parity-clock-full-tests.log`.

A fresh boundary control probe reached a random goblin encounter instead of
the previous non-encounter path. Its opening matched, but its boundary comparison
correctly failed; it cannot be used as matched-path evidence. Private report:
`original-exploration-controls-report.json`, native root `startup-reference-zae_dqy4`.
An independent opening-only probe (`original-opening-controls-report.json`, root
`startup-reference-ochy8jv8`) processed 24 turns without advancing the clock. Two
processed LOOK inputs advanced it by ten minutes each; subsequent inputs did not
establish reliable general advancement semantics. These observations do not
justify a complete host time-progression policy. Owned processes were cleaned up.

Thirty-two deterministic engine boundary replays then found two encounter paths,
seeds 10 and 19, stopping at unsupported opcode 0x06 (DIVIDE), program 20 offset
616. That temporary inventory instrumentation was removed. The next executable
compatibility work is the native arithmetic handler, rather than a fabricated
combat result or inactive original command labels. No game assets, native text,
user saves or instructions were committed.

## Native arithmetic and the next encounter blocker (2026-10-25)

Thirty bounded checks executed the unmodified shared arithmetic handler plus
original operand decoder and writer, using synthetic inputs and a sentinel
remainder word. The native dispatcher opcode variable was initialized explicitly;
this is a handler probe, not full-game execution. Correct overlay-relative entry
selection and opcode initialization were necessary during probe setup. Only the
final passing cases support the result. Subtraction is second minus first;
division is unsigned first divided by second; multiplication retains the low
16 bits. The presumed 0x6E3F remainder register is unchanged. The SSI Engine
reference's remainder assignment was therefore not adopted. Inputs include
0/1, unequal operands, values over 255, 32768 and 65535. Each call was bounded
to 20,000 instructions/100 ms. Private report: `original-arithmetic-report.json`.

Profile `arithmeticDialect: ssi-unsigned16-v1` selects the independent shared
implementation; other profiles default to disabled. Arithmetic reuses typed,
protected destination writes, preserves comparison state and diagnoses division
by zero rather than invoking a host CPU fault. Synthetic cases check all 30
native numeric results with byte and word stores, overflow/underflow, zero
division, invalid/protected destinations, malformed dialects and opcode conflicts.
The first test fixture incorrectly expected pre-invocation comparison state to
survive the existing VM reset; it now establishes COMPARE inside the program
before asserting that arithmetic preserves it. No VM reset policy changed.

The original-resource seeded boundary probes now advance from offset 616 to
offset 769 in program 20 for seeds 10 and 19. Both stop with
`Unknown ECL opcode 34` (0x22), and the integration test asserts that exact gate.
The pinned Pool profile calls this instruction `PARTY_SKILL_CHECK2`, while its
VM implementation is empty. Other dialects assign 0x22 to yes/no input or other
skill checks. Native operand/result and party-skill semantics remain unverified;
substituting a generic confirmation or always-success result would invent game
state. This is the current executable encounter blocker. The ordinary opening,
non-encounter boundary and all seven previously matched rendered regions remain
unchanged. No full encounter, combat or synchronized-party parity is claimed.

Five focused suites passed. The command bar still exposes the supported engine
controls rather than unsupported native actions. Host time advancement, the
native skill-check adapter, remaining commands and combat remain unfinished.
All original assets, native text and captures stayed in the private cache; user
artwork, installed resources and user saves were not modified.

Final arithmetic validation passed all 30 runnable CTest suites; the Windows-only
original RNG suite was skipped under WSL. The actual installed-resource opening,
ordinary boundary, rendered-region fingerprints and two unsupported skill-check
checkpoints ran. Hygiene and attribution checks passed. Private logs:
`parity-arithmetic-final-focused.log` and `parity-arithmetic-full-tests.log`.

## Native opcode 0x22 class presence (2026-10-25)

The native interpreter dispatches 0x22 to its two-operand handler at overlay
1F:1629. The handler traverses the party list and tests only class byte 0x2F:
codes 4 or 10 set the first result to one; otherwise it is zero. The second
result is always zero. Both operands are destinations, stored in order. The
handler does not inspect HP, status, levels or ability scores, and does not roll
random numbers. This is narrower than the reference name PARTY_SKILL_CHECK2.

263 bounded original-code cases passed: empty party, every class byte 0..255,
linked mixed parties and a six-member party with the eligible class last.
All actor HP fields were zero. The probe used the original handler, operand
decoder and writer without instruction substitution; each call was bounded to
20,000 instructions/100 ms, with a 40-second batch deadline. The RNG state was
unchanged and the program counter advanced seven bytes. Private report:
`.cache/goldbox-validation/original-party-class-check-report.json`. The pinned
ClassSelections reference corroborates Ranger and Cleric/Ranger labels for
4 and 10. Its class table was not copied; the reference opcode handler is empty.

The shared adapter now selects `ssi-class-presence2-v1` using profile
`partyClassCheck`, with explicit eligible symbolic class IDs. Pool lists
`ranger` and `cleric-ranger`; these reserved identities do not expand its
character creator. The host reads normalized classes, without native record
offsets or title conditions in the core. The VM performs ordered typed stores,
so aliased destinations end at zero. Comparison, RNG and party state remain
unchanged. Malformed destination forms fail before querying the host; later
protected-write errors follow the existing partial-progress policy.

Synthetic tests cover both presence outcomes, empty and mixed-class state,
alternate configured class IDs, ignored HP/status, unknown class state, output
widths/aliasing, invalid operands and bindings, disabled dialects and conflicting
opcode meanings. The original-resource seeds 10 and 19 now pass offset 769 and
stop at program 20 offset 776 with `Unknown ECL opcode 35` (0x23). That exact
new stop is asserted, replacing the previous 0x22 gate. The reference calls 0x23
SURPRISE and assigns four operands; its native behavior and encounter effects
are the next unresolved dependency. No random skill result, yes/no behavior or
combat outcome was substituted.

Six focused suites passed. Existing opening, ordinary boundary and seven native
rendered-region fingerprints remain valid. Private focused log:
`party-class-check-focused.log`. Original assets/captures stayed private, and
installed files, user saves and artwork were not modified.

The final serialized two-worker run passed 30 suites; the Windows-only original
RNG integration suite was skipped under WSL. The installed-resource opening,
ordinary boundary, seven rendered regions and two new 0x23 stops were exercised.
Hygiene and attribution checks passed. Full private log:
`party-class-check-full-tests.log`.

### Pool SURPRISE (0x23), 2026-10-25

The pinned SSI Engine metadata names a four-input SURPRISE instruction, but its
VM handler is empty. Reuse would not supply executable behavior. The independent
implementation instead follows bounded observations of the authorized installed
Pool handler (overlay 1F offset 0x16C9), its original operand decoder/writer,
original overlay B0 dice helper and resident RNG. No original instructions or
game content are included in the repository.

512 native cases passed with synthetic operands: a Cartesian set of byte
extremes, additional byte patterns and six initial RNG states. Alternating cases
used immediate bytes or immediate words with nonzero high bytes. All runs
returned within the per-call 100 ms / 20,000 instruction bounds and the 40-second
batch limit. Four inputs are truncated to bytes. Both thresholds wrap to signed
bytes: fourth + 2 - first, and second + 2 - third. Exactly two resident RNG calls
with bound 6 produce one-based rolls. The second threshold test overwrites the
earlier both-success result: final outcomes are 0, 1 or 2, never 3. The native
result goes to logical global 0x6DCB; PC advances 9 or 13 bytes according to the
synthetic operand encodings. Final RNG states were checked exactly.

The shared engine selects this through `ssi-signed-threshold2-v1` and an explicit
profile result address, rather than recognizing a title or private native memory
layout. Existing saved RNG and logical globals carry its state. Synthetic golden
tests exercise 12 native-observed cases with all six supported numeric operand
forms, both-success precedence, signed wrapping, adjacent-memory preservation,
comparison preservation and exact two-draw final states. Negative cases cover
missing RNG/adapter, invalid operands, protected output and conflicting dialect
definitions. The observation supports this instruction, not complete encounter
or combat parity.

Original-resource replay now advances both seeded encounter branches beyond
SURPRISE: seed 10 stops at program 20 offset 965, seed 19 at offset 1150, both
with `Unknown ECL opcode 41` (0x29). The pinned Pool metadata names that next
instruction ENCOUNTER_MENU with 14 operands; other titles assign different
meanings. These precise stops replace the earlier offset-776 SURPRISE gate.
Tests restore each pre-move save and repeat it, checking the exact stop and
serialized resulting state including RNG. Encounter-menu behavior and its
combat/movement dependencies remain unimplemented and require separate native
verification; no successful encounter or full-game parity is claimed.

The ordinary boundary path still completes at program/map 20, (15,4), west,
EXIT 615. All seven previously matched rendered region fingerprints remain
unchanged. The private probe/report and build logs remain outside the repository.

Final serialized two-worker build/test run: 30 suites passed, zero failures,
36.71 seconds of tests. The Windows-only original RNG integration suite skipped
under WSL; the installed-resource campaign replay, both seeded save/replays and
seven rendered-region comparisons ran and passed. Hygiene and attribution
checks passed. Private full log: `surprise-full-tests.log`; native evidence:
`probe_original_surprise.py` and `original-surprise-report.json` in the external
validation cache. Installed assets, user saves and artwork were not modified.

### Pool encounter menu (0x29), 2026-10-25

The pinned SSI Engine VM provides a 14-operand ENCOUNTER_MENU reference, with
assumed party speeds 6/12 and a movement-rate TODO. Independent observations of
the installed Pool handler (overlay 1F offset 0x1EF8) found material differences:
cautious WAIT advances enemies at positive distance, and equal maximum party/
enemy speed permits combat against cowardly enemies. Those upstream policies
were not adopted. No Java or native instructions were copied or ported.

72 bounded native-handler cases passed: all five behavior selectors, four menu
choices and three distances (60 cases), plus 12 synthetic-description fallback
cases. The original operand decoder, string handling, branch logic and result
writer ran unchanged. These probes supplied synthetic party speed, visibility
and menu-choice boundaries and suppressed presentation. They verify instruction
decisions, not the native renderer or timing. Results, distance changes, sprite
requests, description selection/acknowledgement and unchanged RNG were recorded.
All runs returned within 100 ms / 40,000 instructions per call and the 40-second
batch limit. Empty descriptions do not request acknowledgement; nonempty
descriptions and wait/flee messages do. Description lookup cycles from current
distance through the other two slots.

72 separate checks ran the original visibility helper (overlay 2B offset 0x482)
and wall query unchanged, using synthetic interior map planes. Four directions,
three obstruction distances, three wall values and zero/nonzero view mode were
checked. Visibility tests only the current-facing wall graphic, not door flags
or the neighboring cell's reciprocal edge. It checks at most two cells. The
independent GEO16 adapter follows these interior semantics. Native map-edge
queries have area-mode-dependent handling; the current adapter rejects a query
that crosses the map edge rather than guessing that policy.

67 further checks ran the original party movement aggregator (overlay 2B offset
0x1FB1) and resident effect-list search unchanged against synthetic linked
actors/effects. The source is the current movement byte at record offset 0x11C.
Effect 0x27 doubles with byte wrapping and takes precedence over 0x2A, which
halves with integer truncation. Both extrema are initially seeded with the
first member's unadjusted movement, so a one-member hasted/slow party does not
simply produce two equal adjusted rates. None of these cases consumed RNG.

This establishes the missing runtime dependency precisely: the normalized party
does not yet store current movement or active effects, nor update those values
from equipment/encumbrance and effects. The encounter VM queries an explicit
host provider only for decisions requiring those extrema. The runtime reports
`Current party movement and effects unavailable` for such choices; it never
supplies the upstream guessed 6/12. Implementing this provider requires current
state, persistence and verified update rules as a separate increment.

The profile selects `ssi-encounter14-v1`, mode address 0x49E6 and requested/current
distance globals 0x6DC0/0x6DC1. The shared VM has no title selection or native
record offsets. Each action round consumes the existing execution-step budget.
Headless synthetic regressions retain the 60 observed decision tuples, 12
description cases, exact distance/sprite transitions and unchanged RNG/comparison,
plus invalid input/host/dialect failures and a nonterminating WAIT limit. Session
tests exercise all 72 interior visibility cases with both closed and open door
flags, preserving party coordinates; profile tests reject malformed bindings.

Both installed-resource encounter replays now pass the menu with deterministic
COMBAT choices and stop at offset 6319 with `Unknown ECL opcode 28` (0x1C,
metadata name CLEAR_MON). Both pre-move save/replays agree exactly through that
new checkpoint, including RNG and script globals. No monster/combat success was
invented. The ordinary boundary and all seven previously matched rendered
regions remain unchanged. This is partial encounter execution, not verified
complete encounters, original encounter UI parity or complete game compatibility.

Private evidence remains in the external validation cache:
`probe_original_encounter.py`, `original-encounter-report.json`,
`probe_original_encounter_distance.py`, `original-encounter-distance-report.json`,
`probe_original_encounter_movement.py`, `original-encounter-movement-report.json`.
The first focused run passed five suites; log: `encounter-focused.log`.

The final serialized two-worker run passed 30 suites with zero failures in
37.86 seconds of tests. The Windows-only original RNG integration suite skipped
under WSL; installed-resource seeded replays and rendered-region checks ran.
Hygiene and attribution passed. Full private log: `encounter-full-tests.log`.
Installed game files, user saves and artwork were not changed.

### Complete native opcode dispatch catalog, 2026-10-25

All 256 byte values were exercised through the unchanged installed interpreter
dispatcher at overlay 1F offset 0x2F2D, with a stop immediately before a selected
handler executes. Values without a selected handler were allowed to return.
The bounded 5,000-instruction / 100 ms per-value, 20-second batch probe passed:
61 values selected 49 distinct native handlers. Exactly 0x1F and 0x3E..0xFF
(195 values) had no handler entry. This does not classify them as valid no-ops
in the full fetch/lifecycle path. Earlier observed 0x22/0x23/0x29 entries agree.

The public [offset catalog](pool-native-offset-map.md) includes every routed
opcode, raw overlay-file offsets, reference operand counts, engine coverage,
known memory bindings and grouped dependencies. Operand layouts are identified
as reference metadata, not an unperformed bulk native behavior validation.
SSI Engine's later Pool opcode labels are not assumed valid for this release.
Private probe/report: `map_original_dispatch.py` and
`original-opcode-dispatch-map.json`. No proprietary code or assets were copied
into the repository, and no installed files or user sessions were changed.

The private `check_dispatch_catalog.py` validates all 61 published rows against
the observed targets and raw-file offset arithmetic, confirms the complete
195-value unrouted set, checks release fingerprints/links and reconciles every
current C++ opcode execution case with the catalog (41 VM paths plus one combat
callback boundary). It passed, as did hygiene and attribution. This increment
changes documentation only; no compile or game launch was needed.

### Monster setup group and combat boundary, 2026-10-25

The pinned SSI Engine VM has an empty LOAD_MON handler and a CLEAR_MON placeholder
that clears the sprite. Neither supplies the native behavior needed here, so no
Java implementation was adopted. The native handler group was investigated
together using the complete dispatcher map, rather than implementing guessed
meanings one opcode at a time.

60 bounded cases ran the original LOAD_MON handler (1F:0450), operand decoder
and native record-copy logic with synthetic template, allocator and presentation
boundaries. They cover initial counts 0/1/62/63, requests 0/1/2/63/255 and group
bytes 8/254/255. A request of zero becomes one; additions stop at 63. The group
byte increments once per accepted template request, with byte wrapping, and all
its clones receive the same group. A setup already at capacity skips the resource
loader and keeps the group/RNG unchanged. The template source was synthetic;
this probe does not measure the real template initializer's RNG consumption.

Three separate CLEAR_MON cases (1F:125D) reset count/group bookkeeping to 0/8,
clear the native 28-byte auxiliary area and release synthetic auxiliary lists
of lengths 0/1/3. No sprite host call or RNG consumption occurs. This resets
active setup bookkeeping; it is not evidence for a complete native actor-list
cleanup implementation.

54 checks then ran the original resource initializer block (7C:0FA5..106D) and
its original dice/RNG helpers with six seeds and nine synthetic race codes.
Exactly five one-based d6 draws initialize offsets C1/C2/C3/C5/C6 as wrapped
`128 + 17*roll`; C4 receives C4. Initialization happens once per template, before
the handler copies clones. Native final states for initial seeds 0, 1, 10, 19,
FFFFFFFF and 12345678 were respectively 1172187917, 2884733762, 1117777183,
3645787900, 3754609368 and 3859354085. These are initializer-block observations,
not a full native campaign/combat RNG parity claim.

16 further native COMBAT preamble cases (1F:179B) observed its entry routing.
With neither native preparation flag set, logical globals 6E6C and 6DE2 select
entry 28:0025, 22:0025 or 25:0025. When either flag is set, native visibility
clamps current distance before preparation at 35:0025 and battle entry 25:0025.
These probes supplied synthetic visibility/preparation boundaries and stopped
before a battle engine executed. They neither inject nor verify combat outcomes.
The remaining preamble, actor initialization and post-combat state need a real
combat adapter; the existing C++ host remains explicitly unavailable.

The implementation adds opt-in `ssi-monster-setup-v1` callbacks and source-bound
prepared-group recipes, not native-pointer replicas or complete combat actors.
Profile data binds programs 0/20 to native-observed resource groups 3/2. Runtime
bundles retain immutable DAX snapshots and include filename/content fingerprints
in saved source identities. Selected character resources must be 285 bytes;
items/effects must be multiples of the observed 63/9-byte record sizes. Optional
missing item/effect records are empty. This checks record structure only; it
does not validate every combat/inventory/effect field. Native pointer fields
are never deserialized into host pointers.

Each accepted setup recipe saves program/resource ID, effective count, group,
the uninterpreted third operand and its pre-initialization seed. Five draws
advance the shared stream now; the future combat adapter can reproduce template
initialization from the recipe seed without advancing it again. Clone counts
do not add draws. Limits, resource shape failures, zero counts, reset behavior,
numeric-byte truncation, conflicting dialects and atomic save rejection have
synthetic regressions. Legacy saves retain unknown setup until CLEAR_MON.
Campaign restart forgets prior setup; standalone previews do not eagerly load
unrelated campaign monster archives.

Both installed-resource seeded paths now pass CLEAR_MON/LOAD_MON and stop at
COMBAT offset 6330 with `Combat host unavailable`. Each has one prepared group
numbered 8, next group 9 and exactly the expected five-draw RNG state. Replaying
from each pre-move save yields identical stops and serialized resulting state.
The ordinary opening/boundary and seven matched rendered regions remain valid.
Native loader status text, normalized monster character windows, inventory and
effect instances, complete actor decoding, combat execution and aftermath are
still absent. Prepared recipes do not establish monster/combat compatibility.

Private evidence: `probe_original_monster_setup.py`,
`original-monster-setup-report.json`, `probe_original_monster_initial_rng.py`,
`original-monster-initial-rng-report.json`, `probe_original_combat_boundary.py`
and `original-combat-boundary-report.json` in the external validation cache.
133 bounded native cases passed across these probes. No game assets, generated
proprietary records or status text were committed; installed files and user saves
were not changed. The repaired focused build passed five suites. The full
serialized two-worker run passed 30 suites with zero failures in 45.32 seconds;
the Windows-only original RNG integration suite skipped under WSL. Full private
log: `monster-setup-full-tests.log`.

The final targeted rerun passed VM/exploration suites after adding multi-load
cap and malformed-resource regressions (0.76 seconds): counts 62 then 2 prepare
62+1 creatures in groups 8/9, and a third load consumes no RNG. Exact ten-draw
state is 1828254910 from seed zero. Log: `monster-setup-final-focused.log`.
Hygiene, attribution and the updated dispatch-catalog/source cross-check passed.

## Detached monster template decoding (2026-10-25)

The next format-layer increment decodes the observed 285-byte character and
ordered 63-byte item / 9-byte effect resources into detached local snapshots.
Known serialized native list pointers are discarded; no file pointer is followed.
Current HP, AC, THAC0 and movement are typed fields derived from the offsets
already checked in native strength/movement research. Race/class codes are not
mapped through player creation enums. Item/effect payloads remain encoded.

Private `probe_original_monster_snapshots.py` ran 54 synthetic character cases
(six seeds by nine race codes) through native `7C:0F61..106D`. This includes
pointer resets and the full initializer, using original dice/RNG instructions.
All 285 result bytes matched the independent expected transformation. Six
whole-record native fingerprints for race zero are public synthetic regression
vectors. Another six cases ran the native item/effect list-copy blocks with
1/2/3 records, replacing only heap allocation/free boundaries. All non-link
payload bytes and list ordering matched, with correct termination. Per-call
bounds were 100 ms / 20,000 instructions and the batch deadline was 25 seconds.
Private report: `original-monster-snapshots-report.json`. Original resources and
derived records remain outside the repository.

Public tests cover pointer removal, unknown-byte preservation, typed fields,
all byte-valued race codes, native-derived initializer fingerprints, invalid
rolls/counts, empty lists and clone independence. The opt-in installation test
materializes both seeded encounter groups from their immutable source bundles,
checks typed fields and verifies identical snapshots after save/replay.

Precise remaining dependency: a faithful normalized inventory/effect adapter
needs verified meanings for the 63-byte item's payload fields and the 9-byte
effect's parameters, duration and combat application. The examined FRUA parser
uses a different item layout and cannot supply those offsets. Snapshots are not
yet integrated as runtime normalized combatants. Combat still stops explicitly
at offset 6330; no combat outcome or complete game parity is claimed.

Validation: the serialized two-worker full build/test run passed 30 suites with
zero failures in 35.76 seconds, including the installed-resource campaign test
with both seeded snapshot/replay checks. The Windows-only original RNG suite
skipped under WSL. Private log: `monster-snapshots-full-tests.log`. Repository
hygiene and attribution checks passed (13 records, five adopted local files).

## Typed monster inventory and effect views (2026-10-25)

Research compared cached SSI Engine's unimplemented inventory methods, cached
Gold Box Explorer's incompatible FRUA item parser, Stephen S. Lee's PC v1.3
research (FAQ 2.00, sections 12.2.3/4 and 12.6), and marainein's older forum
item-format notes. The older notes place the type byte at `2D`, overlapping the
next-item far pointer; the installed code reads it at `2E`. No parser or guide
text/tables were adopted. See THIRD_PARTY.md for URLs, authorship and licenses.

Private `probe_original_monster_fields.py` ran native resident `BA:0BB8..0D84`,
stopping after inventory accounting and before money/remaining stat processing.
All 240 cases passed: weights 0/1/255/256/65535, quantities 0/1/2/255, equipped
bytes 0/1/2/255, and types 0/7/255. The item type selects the corresponding
16-byte base-item table entry. Equipped bytes are tested as nonzero, not exactly
one. Quantity zero contributes one unit; multiplication and accumulation use
16-bit wrapping. Unit weight is word `37`, quantity byte `39`, equipped byte
`34`, type byte `2E`. The probe uses a synthetic base-item table and stops before
any complete equipment/combat-stat recalculation claim.

Another 192 cases executed native `8C:0000` effect timing and `B0:0026` removal:
timer words 0/1/2/10/11/255/256/65535, minute deltas 0/1/2/10/11/255, removal flags
0/1/2/255. Zero timers remain; nonzero timers decrement or expire at the boundary.
Expired effects unlink and free a nine-byte node; nonzero byte `4` invokes the
removal callback with the effect ID. Only free and effect-handler boundaries
were substituted; traversal, timer arithmetic and unlinking stayed original.
Every case returned and preserved RNG. Calls were capped at 100 ms and 30,000
instructions (10,000 for inventory), with a 30-second batch deadline. Report:
`.cache/goldbox-validation/original-monster-fields-report.json`.

Typed views expose only independently checked fields. Effect byte `3` remains
an uninterpreted parameter; timer-word interpretation can be handler-specific.
No timer scheduler or effect callback implementation is claimed. The individual
encounter movement helper uses the previously native-checked `27`/`2A` presence
rules (67 native aggregator cases), preserving precedence, byte wrapping and
integer halving. It does not implement the native first-member min/max quirk or
provide a runtime party movement source.

Regression tests cover the native inventory grid, timer-word/flag boundaries,
malformed record lengths, immutable views, and all 256 movement speeds with
seven effect-list combinations. Both installed-resource seeded encounter checks
decode available item/effect records and compare clone movement. Complete base
item properties, equipment/damage calculation, special effect handlers and
runtime normalized combatants remain the next dependencies. COMBAT still fails
explicitly at the existing unavailable-host boundary.

Validation: full serialized two-worker build passed 30 CTest suites with zero
failures in 35.70 seconds, including installed-resource campaign startup and
both seeded encounters. The Windows-only original RNG suite skipped under WSL.
Private log: `monster-fields-full-tests.log`. Hygiene and attribution checks
passed (13 records, five adopted local files). No original assets, saves,
screenshots or derived item/effect records were committed.

## Base-item table and weapon-stat adjustment (2026-10-25)

The authorized installation's `ITEMS` is 2050 bytes: a two-byte prefix and 128
complete 16-byte definitions. Native initialization `3E:05B7..05E5` seeks to byte
two, requests `0810` bytes into the table at DS:5582 and does not interpret the
prefix as a record count. The installed file supplies 2048 table bytes; the
request's extra record is not treated as an extra decoded definition. Our
explicitly selected layout adapter bounds references to actual complete records
and rejects malformed sizes instead of exposing adjacent native memory.

The base-item layout was cross-referenced with pinned Gold Box Explorer's
`ReadItemDetails` and Stephen S. Lee's PC-specific FAQ 2.00. The independent view
preserves unknown bytes 8/F, raw flag/descriptor fields and signed dice modifiers.
Native weapon-stat execution confirms small/medium dice at 9/A/B and flag byte E;
previous inventory probes establish slot/hands at 0/1. Large dice and other
descriptors are decoded as documented layout fields; complete large-target,
range/rate, armor and equipment semantics are not yet independently validated.

Private `probe_original_weapon_stats.py` ran unmodified resident `BA:0000` with
synthetic actor, weapon, ammunition and base-item records. Only three ability
helper boundaries supplied authored signed-byte adjustments; no weapon-stat
instructions were substituted. All 960 cases passed across eight flag patterns,
eight type IDs, three race codes and five weapon bonuses. Baseline accuracy,
base damage modifier, ability inputs, absent/present ammunition and signed-byte
boundaries varied deterministically. All calls returned and preserved RNG.

Native behavior includes signed wrapping of combined weapon/ammunition bonuses,
Dexterity accuracy via flag 2, Strength accuracy/damage via flag 4, bolts via
flag 80 and arrows via flag 1. Race code 2 gets the extra accuracy for type
36/37/41..44 only, after damage assignment. No equivalent extra damage is added.
The public synthetic grid fingerprint is `5567199630209685347` (FNV-1a64 over
encoded THAC0, dice count, sides and wrapped modifier per case).

An additional 128 native cases used all installed base definitions with authored
context (encoded accuracy 45, race 2, Dexterity accuracy -2, Strength accuracy 3,
Strength damage -4, weapon/arrow bonuses -1, bolt bonus 2). Fingerprint:
`4988089530893162104`. The opt-in installed-resource C++ test reproduces it and
bounds both seeded encounter groups' item references. These are real definitions
with synthetic actor inputs, not verified combat stats for the actual monsters.
No original record bytes or item names are output or committed. Each native call
was bounded at 100 ms / 5,000 instructions; the batch deadline was 35 seconds.
Private reports: `original-weapon-stats-report.json` and
`original-weapon-stats-fingerprints.json`.

The stat API fails for missing required ability inputs, out-of-range signed
modifiers and invalid base-item references. It does not silently fabricate
Strength/Dexterity state. Remaining dependencies are runtime ability/equipment
resolution, large-target adjustment, actual hit/damage rolls and effect handlers.
COMBAT remains an explicit unavailable-host boundary.

Validation: the serialized two-worker full run passed 30 suites with zero
failures in 42.92 seconds, including the installed-resource 128-definition
fingerprint and both seeded encounter reference checks. The Windows-only native
RNG integration suite skipped under WSL. Full private log:
`weapon-stats-full-tests.log`. Hygiene and attribution passed (13 records, five
adopted local files). The final focused suite also covers direct signed decoder
boundaries 0/127/128/255 for item and base-damage bonuses.
It passed in 0.74 seconds; private log: `weapon-stats-final-focused.log`.
