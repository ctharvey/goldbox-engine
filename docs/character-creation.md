# Character creation pilot

The engine stores a party independently of ECL memory and original character
records. Each character has a name, race ID, gender ID, single-class ID,
alignment ID and ordered ability values with optional percentiles. Creation
rules supply the ability names/count, option labels, race/gender bounds, legal
classes/alignments, class minima, percentile policy and party/name limits.
The shared model and SDL creator do not branch on a game ID or filename.

## Rules selection

A game profile may select `characterCreationRules` as an explicit path relative
to that profile. Its rules `id` must match `rulesProfile`. Absent configuration
preserves the existing exploration/startup behavior. The creation JSON has
`schemaVersion: 1`, `id`, `partyLimit`, `nameLimit`, `dice` (`count`, `sides`,
`dropLowest`), labeled `abilities`, `genders`, `alignments`, `classes`, and `races`.
An optional `percentile` selects an ability ID and triggering score (`at`).
Classes provide `minimum`, allowed alignment IDs and a percentile flag; races
provide `minimum`, gender-keyed `maximum` and `percentileMaximum` maps and class
IDs. Unknown fields, duplicate IDs, invalid references, inverted bounds and
mismatched vector sizes fail before use. The supplied profile is the canonical
example. Option counts, numeric ranges, dice and generated-party sizes are bounded.

## Current AD&D profile

Pool selects `adnd-first-edition-creation-v1`: six player races; cleric, fighter,
magic-user and thief; six PCs; names limited to 15 ASCII characters. The installed
manual confirms race/gender ability caps and single-class availability. Basic
class minima and upstream alignment choices are provisional profile facts,
not verified original-game compatibility. Neither Krynn nor Buck Rogers is
implicitly assigned this profile.

The prototype rolls four six-sided dice, drops the lowest, and rejects complete
rolls outside race/gender limits or without any eligible class. At most 256
attempts are made. Dice use an explicit caller-supplied MT19937 generator with
a bounded unbiased mapping; tests use fixed seeds. UI creation seeds its own
generator from SDL's performance counter, separately from ECL execution.
Exceptional strength is a configured percentile ability: fighter strength 18
receives a uniform percentile within the configured race/gender cap (100 is
shown as 00). This is a provisional distribution, not an original RNG claim.

Racial stat adjustments are not yet applied; ordinary 4d6 rolls cannot generate
19 even when a race cap permits it. Multi-classing, HP/AC, portraits, equipment,
spells, experience/advancement, original character import and original ECL party
memory are unfinished. Upstream placeholder HP and racial modifiers are not used.

## Saves and lifecycle

Exploration saves optionally contain `party: {rulesId, characters}`. Every
character and the complete party are validated before a restored session is
published, including class eligibility, alignment, percentile, duplicate names
and capacity. Missing/mismatched rules fail; no-party legacy saves load an empty
roster. Party state is included in the window's initialization rollback snapshot
and F10 launch-state reset. Saves store normalized values, not character bytes.

Synthetic tests cover deterministic rolling, every supported race/gender,
exceptional caps, alternate ability lists, invalid rules/parties, capacity,
roundtrip/atomic restoration, and SDL creation/removal/load/cancellation.
Original DOS creation parity and character-dependent gameplay are unverified.

A local SDL smoke test on 2026-10-16 used the authorized Steam Pool resources:
a named dwarf fighter was created, reviewed and added, map/ECL record 18
initialized successfully, and the private engine save retained one character,
six ability values, the creation rules ID, map 18 and wall banks 1/3/5. This
checks the custom flow with original resources, not DOS creation parity.
