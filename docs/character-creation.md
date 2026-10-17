# Character creation pilot

The engine stores a party independently of ECL memory and original character
records. Each character has a name, race ID, gender ID, single-class ID,
alignment ID and ordered ability values with optional percentiles. Creation
also records a symbolic status (`okay` on completion); status is independent of
ability values. An absent status in older saves remains unknown.
Creation rules supply the ability names/count, option labels, race/gender bounds, legal
classes/alignments, class minima, percentile policy and party/name limits.
The shared model and SDL creator do not branch on a game ID or filename.

## Rules selection

A game profile may select `characterCreationRules` as an explicit path relative
to that profile. Its rules `id` must match `rulesProfile`. Absent configuration
preserves the existing exploration/startup behavior. The creation JSON has
`schemaVersion: 1`, `id`, `partyLimit`, `nameLimit`, `dice` (`count`, `sides`,
`dropLowest`), labeled `abilities`, `genders`, `alignments`, `classes`, and `races`.
An optional `percentile` selects an ability ID and triggering score (`at`).
Its optional `rollMaximum` (1..100) sets the die bound before capping the result
to the race/gender limit. Absent configuration rolls within the limit directly.
Optional `dice.bounds` selects `reject` (legacy default) or `clamp`; unknown
policies fail. Clamp applies racial bounds after modifiers without extra draws.
Classes provide `minimum`, allowed alignment IDs and a percentile flag; races
provide `minimum`, gender-keyed `maximum` and `percentileMaximum` maps and class
IDs. Unknown fields, duplicate IDs, invalid references, inverted bounds and
mismatched vector sizes fail before use. The supplied profile is the canonical
example. Option counts, numeric ranges, dice and generated-party sizes are bounded.
Each race can optionally supply `adjustments`, one signed integer (-100..100)
per ability in the configured order. Absent adjustments preserve unmodified
rolling. Modifiers apply once to rolled scores before eligibility checks; saves
already contain adjusted values and do not reapply them on load.

## Current AD&D profile

Pool selects `adnd-first-edition-creation-v1`: six player races; cleric, fighter,
magic-user and thief; six PCs; names limited to 15 ASCII characters. The installed
manual confirms race/gender ability caps and single-class availability. Basic
class minima and upstream alignment choices are provisional profile facts,
not verified original-game compatibility. Neither Krynn nor Buck Rogers is
implicitly assigned this profile.

The shared rules retain four six-sided dice with the lowest dropped. A game
profile can override `characterCreationDice: {count, sides, dropLowest, keepBestOf}`;
Pool selects the best of six 3d6 trials for each ability, matching the isolated
original human raw-score loop. Five seeded sets and final RNG states are public
numeric regression fixtures from 540 original runtime draws. The selected rules
now clamp racial/gender bounds, matching the isolated original bound blocks.
Rolls without any eligible class still retry at most 256 times; native class
minimum adjustments and the prototype's class-choice order remain unimplemented.

Pool's `random` profile selects `algorithm: lcg32-high16-mod-v1`, multiplier
134775813, increment 1 and startup seed 1. Character rolls, rerolls, exceptional
strength and ECL RANDOM consume the same session stream. `--seed` accepts a
decimal or hexadecimal uint32 override; F10 restores the launch seed. Profiles
without this configuration keep the existing caller-supplied MT19937 path.
Exceptional strength is a configured percentile ability: fighter strength 18
rolls 1..100 and clamps to the configured race/gender cap (100 is shown as 00).
This makes the maximum more frequent when the cap is below 100, matching 72
isolated original cases. Native percentile timing remains unimplemented: the
original can draw immediately after strength, before later abilities, while
this creator draws at character completion after selecting a class.

The rules now apply independently verified racial modifiers: dwarf CON +1 and
CHA -1; elf DEX +1 and CON -1; halfling STR -1 and DEX +1. Gnome, half-elf and
human have no modifier in the tested original loop. Thirty seeded racial sets
match the raw original scores and final states; additional dwarf/elf cases
verify legal scores of 19 and their party save roundtrip. Modifiers consume no
additional random draws. The original also draws before the raw
ability loop, applies age processing before the racial clamps and class floors
afterward; complete
character-creation draw alignment is unverified. Multi-classing, HP/AC, portraits, equipment,
spells, experience/advancement and original character import are unfinished.
Upstream placeholder HP and racial modifiers are not used.

## Saves and lifecycle

Exploration saves optionally contain `party: {rulesId, characters}`. Every
character and the complete party are validated before a restored session is
published, including class eligibility, alignment, percentile, duplicate names
and capacity. Missing/mismatched rules fail; no-party legacy saves load an empty
roster. Party state is included in the window's initialization rollback snapshot
and F10 launch-state reset. Saves store normalized values, not character bytes.
Configured RNG state is saved alongside the party and restored atomically after
algorithm/parameter/dialect checks. Legacy saves without RNG state load with
unknown RNG state; Pool creation and RANDOM then require a new seeded session.

Synthetic tests cover deterministic rolling, every supported race/gender,
exceptional caps, alternate ability lists, invalid rules/parties, capacity,
roundtrip/atomic restoration, and SDL creation/removal/load/cancellation.
Complete original DOS creation parity remains unverified.

A local SDL smoke test on 2026-10-16 used the authorized Steam Pool resources:
a named dwarf fighter was created, reviewed and added, map/ECL record 18
initialized successfully, and the private engine save retained one character,
six ability values, the creation rules ID, map 18 and wall banks 1/3/5. This
checks the custom flow with original resources, not DOS creation parity.

## Read-only script adapter

An optional game-profile `partyMemory` selects `ssi-selected-character-v1` and
defines `recordBase`, `recordSize`, `countAddress` and `abilities` bindings
(`offset`, normalized `ability` ID, `percentile`). It requires creation rules.
An optional `selectedAddress` enables WHO's selected-PC RAM update; Pool uses
0x6DB1. It must be outside the character window/count, event context and program.
An optional `status: {offset, values}` maps symbolic status IDs to script bytes.
Offsets must not collide with ability fields; encodings have at most 64 bounded
IDs and byte-valued codes. Several conditions may share a script code.
The shared adapter contains no title-specific addresses or ability ordering.
Unknown dialects, overlapping count/record ranges, duplicate offsets and invalid
ability references fail validation. Event-context and program overlaps fail
before script execution.

Pool selects the research window 0x6B00..0x6CFE, party count 0x6E3E, ordinary
ability offsets 0x10..0x15, and exceptional strength 0x16. LOAD_CHAR (0x0A)
selects the loaded character by zero-based party index. It does not change
the game's separate selected-PC RAM index. Invalid indices fail explicitly,
unlike SSI Engine's silent ignore. Numeric reads resolve configured fields;
string references anywhere in the window resolve the loaded character's name.
Byte/word reads and indexed COPY_MEM use the same adapter. Count remains
readable for an empty party; character reads require a loaded member.

WHO (0x39) takes a text prompt and presents names in roster order through the
existing host menu. A valid zero-based choice updates both the loaded character
and the configured selected-PC RAM byte. Empty parties, absent selection-address
configuration, invalid prompts and out-of-range host choices fail explicitly;
selection is published only after a valid response. This uses the same menu
interface in SDL, the terminal and JSONL automation. Roster removal adjusts both
indices. The selected-PC byte retains ordinary RAM semantics: a direct script
write to it does not implicitly perform LOAD_CHAR. Original WHO interaction,
focus/default selection and matched-event parity remain unverified.

Pool's researched status binding uses offset 0x100: `okay` maps to 1; animated,
temporarily-gone, running, unconscious, dying, dead, petrified and gone map to 0.
This is the script-facing readiness byte, not the native character file's status
ordinal at offset 0x10C. Unknown/unsupported symbolic conditions fail explicitly.
Status writes remain unsupported: no native ordinal/flag decoding, damage,
revival or spell effects are implied. Other games can supply different encodings.

Unsupported fields and all script writes to this window/count report errors,
including word writes crossing a boundary. They never fall back to stale RAM
or modify normalized characters. Current/natural abilities share the same
read-only values; HP, race/class codes, equipment and mutable effects are not
implemented. Ordinary addresses retain existing RAM/string behavior.

Adapter-enabled engine saves preserve `loadedCharacter`; legacy saves default
to index zero. Restore validates the index against the restored roster before
publishing state. This is not an SSI character-file or save-file encoder.

Synthetic tests cover selection, names, byte/word/indexed reads, count, read-only
boundaries, unknown fields, alternate layouts, conflicting opcode metadata and
atomic save validation. SSI Engine at the pinned attribution revision supplied
research facts, not copied Java code. Original character-access parity remains
unverified.

On 2026-10-17 the adapter-enabled read-only probe initialized authorized Steam
GEO/ECL record 18: exited in 15 steps at offset 184, retained map 18, and bound
wall banks 1/3/5 once, with no text calls. This checks initialization regression
only; it does not exercise or verify original character-dependent events.
