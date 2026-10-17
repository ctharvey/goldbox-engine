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
