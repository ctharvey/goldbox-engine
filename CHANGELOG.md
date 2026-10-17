# Changelog

## 2026-10-17

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
