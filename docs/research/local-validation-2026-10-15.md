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
