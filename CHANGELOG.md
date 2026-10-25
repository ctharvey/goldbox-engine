# Changelog

## 2026-10-24

- Add profile-selected source-cell movement events, exit/block/previous-position
  registers, bounded movement/search execution and transfer continuations.
  Connect SDL, terminal and JSONL controls; retain the unconfigured prototype.
- Support a profile-bound forced-step call and temporary no-resource map state.
  Record the original-resource boundary probe's explicit unsupported stop at
  Pool's PARTY_STRENGTH opcode 0x1D; no successful-transition parity claim.

- Follow numeric COMPARE/IF routes in the static map-event scanner, including
  AND comparison flags; identify unknown memory dependencies by address. Add
  synthetic condition, malformed-instruction and dialect-conflict regressions.
- Capture original movement-control words in the private startup harness.
  Verify the native boundary flag in 32 bounded cases and reproduce the original
  opening's westward area transition; document the remaining engine lifecycle gap.

- Resolve supported static ECL routes using profile-bound GEO coordinates as
  well as selectors. Keep arrival facing unknown and reject overlapping context
  bindings. Test location-only events on zero-selector cells through real CLI
  movement, blocked input, re-entry and save/load.
- Pass profile coordinate bindings through the read-only GEO/ECL scanners;
  verify coordinate-based routes through the actual scanner executable.

- Use SDL's built-in readable font for enhanced menus and labels while retaining
  imported SSI glyphs in original mode. Add rendered-pixel checks across F11
  round trips for font selection and original glyph orientation.

## 2026-10-18

- Support enhanced character portraits keyed by both original head/body archives
  and record IDs. Preserve composition, picker selections, saves and original-mode
  artwork; carry portrait identities through timed sprite replacements. Validate
  paired bindings and test picker cycling, conflicts, fallback and F11 pixels.

- Add an optional mod image library keyed by game, archive, kind, record and frame.
  Connect picture and sprite identities to full-color BMP replacements in enhanced
  mode, retaining original artwork and F11 cutover. Reuse mod dependency ordering
  and report binding conflicts; validate paths and bound BMP allocations. Add
  synthetic library/pixel/cutover regressions. No production artwork is bundled.

- Add F11 original/enhanced display cutover without restarting the game or VM.
  Retain artwork, dialogue, menu selection and game/RNG state; preserve the
  selected layout across F10 and campaign startup. Prepare the original adapter
  for enhanced launches when its resources are available. Add cutover/state,
  pixel, menu and startup/restart regressions; ignore held-key repeats.

- Dismiss stale event artwork on navigation and clear old dialogue in original
  mode. Make original-mode Down/S turn around in place; enhanced mode retains
  backward movement. Add synthetic pixel/state regressions for turning, blocked
  movement, ordinary movement and fresh artwork produced by a new event.

- Fix GEO16 ordinary-door traversal: honor directional state 1 on both sides
  of an edge while preserving solid walls, closed states and map boundaries.
  Avoid redrawing exploration/menus for irrelevant events, and render same-color
  dialogue in runs instead of changing font color per glyph. Add Linux/WSL SDL
  call-count regressions for input floods and synthetic four-direction door tests.

- Honor ECL menu orientation in original mode: horizontal choices occupy the
  command row; vertical choices and their prompt use the dialogue region, keeping
  portraits and the party panel visible. Add native-axis arrows, Space/Enter
  confirmation, first-letter selection and bounded selected-item viewports.
  Synthetic SDL tests cover choice indices, wraparound and unchanged party position;
  complete recorded-original menu styling and timing remain unverified.

- Render SSI inline dialogue colors in original mode without counting control
  codes as visible text. Preserve color across PRINT fragments and page clears,
  support escaped percent signs, and keep malformed controls literal. Add
  synthetic color/wrapping/page regressions and an ECL-to-SDL integration check;
  recorded-original color and reveal-timing parity remain unverified.

- Wrap original-mode dialogue at word boundaries, preserve the cursor across
  consecutive PRINT fragments, and pause before clearing a full page. Keep
  engine notices separate from ECL dialogue. Highlight the selected party member
  from the configured memory binding (white selected, cyan otherwise), with green
  numeric fields. Add synthetic continuation, paging and selection regressions.

- Add selectable original/enhanced display modes over the shared game state.
  Pool defaults to a profile-configured native canvas, imported symbol borders,
  scene/party/dialogue regions and paged text. Preserve the enhanced overview;
  exact startup/creation screens, armor, clock and timing parity remain unfinished.

- Move choice menus into the overview panel and wrap their labels so character
  portraits and opening cutscene artwork remain visible during input prompts.

- Add profile-selected campaign Begin with created-party/RNG retention, initial
  map/program resolution, ADD, host DELAY, position refresh and timed sprite-to-
  portrait presentation. Bind active ECL programs in engine saves and bound
  program transfers. Add optional installed-resource opening/save replay and a
  code-driven DOS checkpoint comparison; full gameplay and timing parity remain
  unverified.

- Connect edition-matched Tandy music to startup through the existing --audio
  option. Keep edition fingerprints and audio tables in the game profile.
  Stop music at the menu, replace queued playback on F10 restart, and preserve
  the installed v1.3's silent startup. Verify both paths with bounded scripted
  engine runs and captured audio; recorded-original parity remains unverified.

## 2026-10-17

- Add a bounded SSI Tandy tone-score decoder and deterministic three-voice PCM
  preview, with caller-supplied tables, notes, shared instrument calls, envelopes,
  sweeps and loops. Reject noise and malformed/nonterminating scores explicitly.
  Startup integration and recorded-audio parity remain unfinished; no music
  payload or original instrument table is distributed.

- Harden the comparison controller after a second audit: tolerate WSL clock
  drift in lease renewal, retry an interrupted kill and keep the session lock
  held until cleanup completes, stop reaping the POSIX leader before killpg,
  handle SIGQUIT and let POSIX children inherit the lock, reject duplicate
  scenario JSON keys, and watch the engine while a menu waits. Nested
  privateRoots can still run two sessions on one game copy (deferred).

- Add profile-controlled five-second startup titles, key skipping, a separate
  menu and C/B/L/E shortcuts. Preserve legacy presentation when timing is absent.
  Verify the installed Steam startup's observed silence with a calibrated
  private recording and trace title waits against the original routine.

- Route verified ECL audio calls through a profile-configured virtual selector
  and effect IDs. Add bounded speaker sweeps, loops and modulation; preserve explicit
  errors for unsupported native calls, sound controls and music.

- Add opt-in original speaker-effect decoding and an SDL3 F8 audition in startup
  and exploration. Keep executable offsets in the game profile, use bounded
  read-only EXEPACK extraction, and reject unsupported sound commands explicitly.
  Automatic game sound triggers and startup music remain unfinished.

- Clarify the harness's operator checkpoint and RNG limits. Regress that
  reference-only code-wheel/startup inputs never become engine actions, consume
  action IDs, or bypass the shared-input staging guard.

- Add a read-only combat-icon part probe. Verify all 56 CHEAD and 128 CBODY
  records with the existing decoder, and inspect the original customization
  screen. Icon composition/recoloring still needs deterministic reference data.

- Add rules-configured character portraits with H/B cycling and K to keep,
  imported artwork preview, saved selections and roster viewing. Verify native
  selector behavior and original HEAD3/BODY3 compositions; regress cancellation,
  restart, persistence, invalid choices and unchanged RNG.

- Add configurable multi-class creation: component dice, averaged starting gold
  and HP, fixed starting ages and separate spellbooks. Match the original race
  availability table and regress complete original multi-class numeric results.

- Preserve configurable starting spellbooks and first-level slots. Verify all
  supported class initializations and 18 original Wisdom boundary cases without
  extra random draws. Show spell/slot counts and validate saved spellbooks.

- Generate, display and preserve configurable starting gold and hit points after
  ability rolls. Verify 204 original post-stat initializations and 72 HP/CON
  boundary cases. Preserve wounded HP and unknown legacy values; regress exact
  RNG continuation, rerolls, save validation and scripted SDL creation.

- Match the tested original character-generation sequence: initial random draw,
  age, racial/age modifiers, clamps, class floors and in-order exceptional strength.
  Add configurable ordered age adjustments and 353 original numeric snapshots
  with exact state checks. Select alignment before rolls and avoid confirmation
  redrawing percentiles. Full interactive parity and remaining character fields
  are still unfinished.

- Generate, display and save optional starting age from configurable race/class
  dice using the shared RNG. Verify 102 isolated original cases across all 17
  supported pairs. Select class before age/abilities for configured creators;
  preserve unknown ages in legacy saves. Age-based ability modifiers and full
  creation draw alignment remain unfinished.

- Apply optional class-specific creation floors and offer supported classes that
  can meet their requirements through those floors. Verify 216 isolated original
  class-floor cases without RNG consumption; cover completion, incompatible caps
  and unchanged saved parties. Native creation ordering and age remain unfinished.

- Select rules-driven racial/gender clamping instead of whole-set rerolls.
  Roll exceptional strength on 1..100, then clamp to its configured cap.
  Verify 504 original bound cases and 72 percentile cases; preserve legacy
  policies when the optional settings are absent. Age and native class-floor
  adjustments, class-choice ordering and percentile draw timing remain unfinished.

- Apply optional rules-defined racial ability modifiers during party creation.
  Verify dwarf, elf and halfling adjustments plus unmodified controls against
  isolated original code; regress seeded scores, unchanged draw consumption,
  legal ability values of 19 and save roundtrips. Full creation parity remains
  unfinished.

- Use profile-selected, saved RNG state for character creation and ECL RANDOM.
  Pool rolls the best of six 3d6 trials per ability; five human raw-score vectors
  and final states match 540 isolated original-runtime draws. Add explicit startup
  seeds and deterministic fresh-process save/reload coverage. Racial adjustments,
  age caps and complete original creation-sequence parity remain unfinished.

- Add deterministic scripted input and exact ordered engine-event assertions to
  the existing comparison harness, usable from the CLI or Python without an LLM.
  Exercise the real engine's movement, menus, memory and save/load in CI; stop on
  mismatches, unexpected events or invalid choices. Preserve manual control and
  existing limits, and close drained pipes for repeated programmatic sessions.

- Automate RNG regression coverage with 280 fixed original-runtime numeric
  observations, exact state checks and synthetic capture/cleanup failure tests
  in public CI. Add a bounded opt-in native integration test for 280 original
  draws plus 160 draws from a freshly captured DOS seed; missing private inputs
  explicitly skip, while partial configuration and mismatches fail.

- Add a seeded RNG debug screen with original-code versus C++ values and state
  comparisons, mismatch highlighting, and read-only seed capture from an owned
  private DOSBox copy. Add a reusable parameterized generator and bounded probe.
  This tests the compatibility primitive; gameplay RANDOM/party RNG integration
  and live game draw tracing remain separate work.

- Verify the original DOS ECL RANDOM handler with 2,080 isolated executions.
  Document inclusive bounds below 255, the 255 edge case, word-to-byte narrowing
  and one draw even for a zero result. Engine RANDOM remains unsupported; live
  save/reload parity remains unverified.

- Identify the installed DOS executable's time-seeded 32-bit random generator.
  Isolated execution of its original routines passes 1,120 draw checks and three
  clock-seeding checks. Record startup and save-format evidence separately from
  still-unverified live replay; no engine RNG implementation added.

- Complete an original character-name event branch with declared authored party
  and flag state, then verify exact save/restore in a fresh engine process.
  The unset-flag route needs RANDOM; an alternate response needs DAMAGE.
  A matching original-game checkpoint remains unverified.

- Fix ECL AND to update the comparison flag from its zero/nonzero result;
  previously subsequent conditions used stale comparison state.

- Trace an original map-7 search event through its menus and WHO to a missing
  character-status read. Add optional symbolic status in normalized parties and
  saves, with profile-defined read-only script encoding. New characters start
  okay; legacy status remains unknown. Original-event parity is unverified.

- Add WHO party selection through existing SDL, terminal and automation menus.
  Update loaded-character and profile-configured selected-PC state only after
  a valid choice; preserve LOAD_CHAR's separate semantics and adjust selection
  on roster removal. Original WHO interaction/event parity remains unverified.

- Establish a private original-save/character-sheet baseline and inventory
  candidate character-dependent events. Document WHO selection and matching
  world state as remaining checkpoint requirements; no event parity claimed.

- Connect normalized parties to ECL through an optional profile-selected,
  read-only memory adapter: LOAD_CHAR, names, abilities, exceptional scores and
  party count. Persist loaded-character selection; reject unsupported fields,
  writes and conflicting layouts. Original character-access parity is unverified.

## 2026-10-16

- Add startup character creation, rerolling, party review/removal and party-aware
  engine saves. Separate data-driven creation rules from normalized party state;
  Pool opts into a six-race, four-single-class pilot with a six-member limit.
  Preserve roster state when initialization fails and reset it on F10. Original
  character byte layouts, combat statistics and creation parity remain unfinished.

- Add F10 restart from exploration, event menus and startup. Restore launch
  state and recreate presentation resources, return to titles when enabled,
  and leave save files untouched.

- Add opt-in profile-selected title screens and an exploration startup menu.
  Initialize the paired map before movement, load engine saves without rerunning
  initialization, and return to the menu with rolled-back state on failures.
  Party creation and full original campaign startup remain unfinished.

- Animate decoded pictures through an explicit profile interval during idle and
  menu waits. Preserve static sprites/portraits, bound timer waits, and keep
  original timing unverified (Pool uses a provisional 400 ms research policy).

- Execute profile-selected three-operand LOAD_AREA_MAP through a reusable,
  read-only map resolver. Validate geometry/positions before switching, preserve
  the current ECL program, bind map-aware saves to the supplied resource identity,
  and restore old single-map saves without inventing archive-group selection.

- Execute opt-in LOAD_AREA_DECO through a typed host callback, replacing wall
  banks only after resource/map validation. Persist selected IDs as optional
  save state and rebind before restoring the window's game state. Preserve the
  old display/state on resource failures; keep archive/map switching explicit.

- Connect opt-in ECL PICTURE portrait selection to profile-configured head memory
  and layout, with explicitly supplied HEAD/BODY archives. Preserve normal-picture
  sentinel handling, clear behavior and the previous image after resource errors;
  test typed memory writes, portrait upload and fallback in the SDL host.

- Add a reusable EGA body/head portrait composition adapter with explicit canvas
  and fallback coordinates, embedded-offset handling, opaque layer order and
  strict bounds. Keep composition separate from decoding, ECL and rendering;
  add synthetic tests and opt-in local structural validation. Resource pairing
  and original-event portrait selection remain unfinished.

- Add explicit original-font selection for the exploration window. Decode SSI
  monochrome tiles into a transparent, tinted atlas; map printable characters,
  fold UI lowercase to uppercase, and use a question mark for unsupported bytes.
  Labels, menus and event text share the atlas. Keep the debug-font default and
  use synthetic tests; extended glyphs and original layout parity remain unverified.

- Add an opt-in SSI color dungeon background behind original wall artwork.
  Pool's profile supplies sky-color addresses; GEO context selects indoors or
  outdoors. Render native sky/horizon/floor bands, reflect ECL memory changes,
  reject invalid selectors, and cover color selection, wall layering and saved
  memory with synthetic tests. Original-scene parity remains unverified.

- Controller Low-finding fixes: bounded config size/duplicate keys/NUL bytes, menu and
  completion render validation, reader OSErrors reported as failures, lock opened
  `O_NOFOLLOW` and taken before the marker is read, Windows job accounting via
  `ActiveProcesses` and a single-resume check, WSL adapter cleanup/reap bounded
  below the controller wait, engine no longer inherits `SIGHUP=SIG_IGN`, hostile
  control frames end with a diagnostic, and abandoned reference-input dispatches
  are cancelled before the next key. The session lock now stays held when a kill fails
  (ownership ambiguous). Deferred: F30, F33, reference mapping/disarm failures staying
  fatal, and two interrupt windows around launch.

- Controller cleanup hardening: SIGTERM/SIGHUP (POSIX) and CTRL_BREAK (Windows) now run
  the same cleanup path as Ctrl-C; POSIX `poll()` no longer reaps the owned group
  leader before `killpg`; a second interrupt during cleanup no longer skips the
  other tree or the lock release; a cleanup failure is appended to, not a
  replacement for, the session failure.

- Harden the comparison controller: reject WSL launchers (bash, wsl, distro
  launchers) by name and by the OS-resolved image of the suspended child, and
  compare `privateRoot`/`reference.cwd` by file identity so UNC, `\\?\` aliases
  and ancestors of the repository no longer pass. Split `scripts/compare_game.py`
  into `compare_*` modules (config, lock, process, WSL, streams, session) with the
  CLI entry point and names unchanged. Replace the README-only `importers/` and
  `gameplay/` stubs with a documented target layout in `docs/architecture.md`.
  Remaining controller findings are in the 2026-10-16 controller review.

- Connect an opt-in three-operand ECL image dialect to typed host callbacks and
  SDL picture/sprite display from explicitly selected archives. Add transient
  presentation state, wall-clamped sprite distances, clearing and resource-error
  handling; preserve headless diagnostics and clear images on save restore.
  Add VM/state/SDL tests and read-only local resource checks. Animation, timed
  replacement pictures, four-operand sprite variants and scene parity remain
  unsupported or unverified.

- Add bounded monochrome 8x8 strips and per-frame EGA picture/sprite decoding
  with explicit independent/XOR-first modes and standard/sprite/combat palettes.
  Preserve raw timing and offsets without mutating input; reject malformed or
  mismatched frames. Validate local picture, sprite, combat-image and mono
  resources with public-safe counts/hashes and synthetic regression tests.
  ECL image selection, UI font mapping and scene parity remain unfinished.

- Decode bounded SSI EGA images and 156-byte wall definitions, shared tiles,
  split/continuation banks and transparency. Add explicit original-artwork
  selection to the SDL view with a native 88x88 perspective compositor.
  Add synthetic malformed-input, bank and pixel/occlusion tests, preserve MIT
  attribution, and record read-only original-resource validation. Automatic
  ECL wall-bank selection, backdrops and scene parity remain unfinished.

- Add an optional SDL3 exploration renderer: procedural first-person walls,
  revealed map overview, movement/turning, ECL text and menus, save/load, and
  event-driven redraw. Keep headless builds and the JSONL protocol available.
  Add synthetic geometry and SDL event-loop tests and dependency attribution.
  Original artwork, full campaign startup and graphical parity remain unverified.

- Verify and document the inspected Steam build's startup path: Enter reaches
  the original party menu without a manual/code-wheel lookup; Load Saved Game,
  slot A reaches 3D exploration. Repeat through harness staging on a fresh
  private copy, record build fingerprints, and keep startup evidence separate
  from matching engine state or gameplay parity. No game binary is patched.
- Add opt-in shared input for the comparison controller: one ordered action
  stream concurrently drives the engine and a directly launched owned SDL 1.2
  reference window. Add explicit action/menu mappings, relative-facing movement
  translation, disarmed reference staging, bounded key delivery and failure
  handling without global keyboard injection. Add synthetic mapping/Windows
  integration tests; synchronized input does not claim equivalent game state.
- Add an explicit Windows-to-WSL engine backend to the comparison controller.
  A bounded Linux supervisor forwards the existing protocol and enforces a
  renewable controller lease, independent deadline, reserved process-group
  ownership, supervisor-loss cleanup and verified shutdown acknowledgment.
  Existing native configurations and raw WSL-launcher rejection remain intact.
  Add synthetic Linux and actual Windows/WSL transport/failure tests; no
  original-game compatibility or parity is claimed.
- Add the bounded, python-stdlib `scripts/compare_game.py` preview controller
  for one supervised engine JSONL session beside one manually driven reference
  process: version-1 validated session config, OS-backed session lock,
  owned child trees (Windows Job Object with suspended-start/resume; POSIX
  `setsid`+`killpg`), strict engine-event rendering with exact request/menu
  ids, bounded streams and deadlines, cleanup on quit/EOF/Ctrl-C, and an
  explicit WSL-launcher rejection on Windows. The session is deliberately
  unpaired/inconclusive; no parity result is produced.
- Register `goldbox_compare_game_tests` (Python-only synthetic CLI/child-process
  tests) covering the valid flow and id order, malformed configs and protocol,
  failures, deadline and flood cancellation, owned-descendant cleanup with an
  unrelated sentinel surviving, lock contention/release, spaced paths without
  shell interpretation, and an absence of any parity-MATCH claim. CTest also
  verifies synthetic event execution and persisted state through the compiled
  engine/controller pair; standalone controller tests need no engine build.
  No original-game parity is claimed.
- Add an opt-in `--harness` prefix to the exploration pilot that serves the
  same session over flushed JSONL events (ready/text/menu/complete/error)
  with bounded request lines, menu-response validation, and explicit
  unsupported-status completion for unknown VM failures.
- Preserve the human terminal pilot unchanged; harness mode reuses the shared
  session, move/trigger paths and save/restore safeguards without a second VM.
- Register synthetic protocol tests for state persistence, ordered menu choices,
  unsupported instructions, malformed input, and bounded child-process cleanup.

## 2026-10-15

- Integrate terminal movement, original-event output and local state persistence.
- Verify a standalone original Pool of Radiance menu event using two choices;
  both paths exit and restore position, and two saved memory bytes differ.
- Add program-memory reads, indexed COPY_MEM and typed runtime dispatch traces.
- Accept Windows CRLF input in the WSL terminal pilot, with regression coverage.
- Record local validation separately from synthetic CI and retain unverified
  game compatibility status. Follow the serialized WSL build/timeout rules.

## 2026-10-12

- Add read-only installation inventory with SHA-256, resource-family recognition
  and DAX/GEO/ECL decoder diagnostics without extracted assets.
- Correct DAX index-word interpretation after comparing two OSS readers;
  retain compatibility with existing synthetic fixtures.
- Decode lifecycle entries and bounded selector routes to explicit ECL tables,
  with typed GEO/ECL links and unresolved-state diagnostics.
- Add a deterministic headless ECL VM with execution/stack limits, typed memory,
  comparison/branching, subroutines, text, menus and combat host callbacks.
- Keep game profiles unverified and original-game validation separate from CI.
