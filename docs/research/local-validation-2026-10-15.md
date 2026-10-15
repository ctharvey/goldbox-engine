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
