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
