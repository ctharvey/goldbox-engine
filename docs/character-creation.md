# Character creation pilot

The engine stores a party independently of ECL memory and original character
records. Each character has a name, race ID, gender ID, single-class ID,
alignment ID and ordered ability values with optional percentiles. Creation
also records a symbolic status (`okay` on completion); status is independent of
ability values. An absent status in older saves remains unknown.
Optional age is stored in years; older saves without age remain unknown.
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
An optional class `creationMinimum` contains one floor (0..1000) per ability.
Creation choices include a class when raising scores to those floors would meet
its ordinary requirements without exceeding racial/gender maxima. Finishing
the draft raises scores before exceptional strength; it consumes no extra draws
for the floors. Absent configuration retains strict score-based eligibility.
Saved-party validation still uses `minimum` and never applies creation floors.
Optional race `startingAge` maps each supported class ID to `{base, count, sides}`.
Generation adds the configured dice to the base (count 1..10, sides 2..100), with
a result bounded to 1..65535. Missing age configuration consumes no draws.
Every supported class must have a valid entry when a race opts in; unknown,
missing, malformed and overflowing entries fail explicitly.
Optional `initialRandomBounds` declares up to 16 setup draws before starting age;
Pool selects `[256]`. Each race can supply up to 32 ordered `ageAdjustments`.
Entries have inclusive `minimum`/`maximum` ages and per-ability signed `values`.
All applicable entries accumulate before racial clamping. `positiveOnly` lists
ability IDs whose adjustment is skipped at zero; optional `wrapByte` selects
unsigned byte arithmetic. These settings reproduce the original ordered byte
operations without putting title IDs or native memory offsets in the core.

## Current AD&D profile

Pool selects `adnd-first-edition-creation-v1`: six player races; cleric, fighter,
magic-user and thief; six PCs; names limited to 15 ASCII characters. The installed
manual confirms race/gender ability caps and single-class availability. Basic
saved-party class minima and upstream alignment choices are provisional profile facts,
not verified original-game compatibility. Neither Krynn nor Buck Rogers is
implicitly assigned this profile.

The shared rules retain four six-sided dice with the lowest dropped. A game
profile can override `characterCreationDice: {count, sides, dropLowest, keepBestOf}`;
Pool selects the best of six 3d6 trials for each ability, matching the isolated
original human raw-score loop. Five seeded sets and final RNG states are public
numeric regression fixtures from 540 original runtime draws. The selected rules
now clamp racial/gender bounds, matching the isolated original bound blocks.
Rolls without a usable creation class still retry at most 256 times. Verified
class creation floors apply during the complete character roll. With starting-age configuration,
the UI selects class and alignment before rolling age and abilities; other profiles retain
their existing roll-then-class flow. Reroll keeps the selected age and class.

The supplied rules define independently verified starting-age dice for all
17 supported race/class pairs. For seed 0x12345678, a human fighter starts at
19 years (15+1d4), an elf fighter at 148 (130+5d6), and a dwarf fighter at 52
(40+5d4). Age is displayed in the character summary and saved beside abilities.
Age-based ability modifiers now apply before racial/gender clamping. Class
floors follow clamping, and exceptional strength rolls immediately after its
ability, before later ability rolls. The initial setup draw also consumes the
shared stream. This matches 353 isolated original-code snapshots, including age
threshold boundaries and exceptional-strength cases, with exact final states.
It does not establish full interactive creation or all native character fields.

Pool's `random` profile selects `algorithm: lcg32-high16-mod-v1`, multiplier
134775813, increment 1 and startup seed 1. Character rolls, rerolls, exceptional
strength and ECL RANDOM consume the same session stream. `--seed` accepts a
decimal or hexadecimal uint32 override; F10 restores the launch seed. Profiles
without this configuration keep the existing caller-supplied MT19937 path.
Exceptional strength is a configured percentile ability: fighter strength 18
rolls 1..100 and clamps to the configured race/gender cap (100 is shown as 00).
This makes the maximum more frequent when the cap is below 100, matching 72
isolated original cases. The complete creator now draws it at the original
point after strength. Confirmation validates prepared scores without drawing
another percentile.

The rules now apply independently verified racial modifiers: dwarf CON +1 and
CHA -1; elf DEX +1 and CON -1; halfling STR -1 and DEX +1. Gnome, half-elf and
human have no modifier in the tested original loop. Thirty seeded racial sets
match the raw original scores and final states; additional dwarf/elf cases
verify legal scores of 19 and their party save roundtrip. Modifiers consume no
additional random draws. The original also draws before the raw
ability loop, applies age processing before the racial clamps and class floors
afterward; complete
live interactive creation parity remains unverified. Multi-classing, AC, portraits, equipment,
spell casting/memorization, experience/advancement and original character import are unfinished.
Upstream placeholder HP and racial modifiers are not used.

`roll_character` prepares a draft with known race/gender/class. A draft without
age consumes setup draws and generates age; an existing age means an ability
reroll, retaining that age and skipping setup. Invalid inputs leave the caller's
RNG unchanged. Supply/validate name and alignment before adding the result to a
party. The older `roll_abilities`, `roll_starting_age` and `finish_character`
interfaces remain lower-level helpers; they do not reproduce the whole sequence
when combined in arbitrary order. Do not call the legacy randomizing completion
helper on a prepared roll. Actual DOS reroll/cancellation/idle timing remains
unverified; engine reroll behavior is deterministic and tested separately.

### Starting hit points and money

Optional class `startingMoney` configures currency ID, count/sides and multiplier;
`hitPoints` configures count/sides, a minimum roll, modifier ability, first supported
ability score (`abilityMinimum`) and its ordered modifier array. Money rolls after
the abilities and before HP. Pool uses cleric 3d6, fighter 5d4, magic-user 2d4 and
thief 2d6, multiplied by ten gold. HP uses 1d8/1d10/1d4/1d6 with floors 5/6/2/4,
then Constitution modifiers and a final minimum of one. Fighter CON 17/18 grants
3/4; the other supported classes grant 2. These policies are data, without game
IDs or original record offsets in the engine.

The original full post-stat initialization matches 204 numeric cases, including
gold, base/current/maximum HP and final RNG. Another 72 original cases cover CON
3..20 for each class. The engine displays and saves HP (`base`, `current`,
`maximum`) and per-currency balances. Current HP may be zero; it cannot exceed
maximum. Loading preserves wounds and balances, never rerolling or recomputing
them. Legacy missing fields remain unknown. Rerolls regenerate money and HP while
retaining age. The original reroll branch confirms this ordering; interactive
input, idle timing and the entire native character record remain unverified.

### Starting spellbooks

Optional class `startingSpells` provides `known` spell IDs and `firstLevelSlots`.
An optional `ability`, `abilityMinimum` and ordered `modifiers` array adjust
slots by ability score. Spell IDs are opaque to the shared creator. A character
can have an explicitly empty book; absent legacy books remain unknown. Saves
preserve known IDs and slot counts, including mod-defined IDs, without deriving
them from current abilities. Duplicate/invalid IDs and counts outside 0..64 fail.

Original post-stat execution across 204 cases confirms eight available cleric
spells, four magic-user spells (Detect Magic, Read Magic, Shield, Sleep), and
empty noncaster books. Eighteen isolated cleric cases verify Wisdom 3..20:
one first-level slot through 12, two at 13, three thereafter. Magic-users have
one. Initialization consumes no additional RNG. The summary displays spell and
slot counts. This models starting availability/capacity; memorized spell lists,
casting, spell effects and progression remain unfinished.

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
