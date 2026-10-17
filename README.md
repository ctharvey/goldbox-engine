# Goldbox Engine (working title)

An open-source, GPL-3.0-or-later-licensed, OpenXcom-inspired framework for playing and enhancing classic SSI Gold Box RPGs **using original resources supplied by the player**.

**Status: bounded exploration pilot.** M1 journal mods, installation scanning,
DAX/GEO/ECL decoding, selector routing, a minimal ECL VM, and terminal exploration
with state persistence are implemented. A standalone Pool of Radiance map/event
has been exercised against user-owned Steam resources. Full campaign startup,
combat, complete original-asset rendering, and embedded JS remain unimplemented; game compatibility is
still unverified. See the [local validation scope](docs/research/local-validation-2026-10-15.md).

## What actually works today

- Buildable, testable C++20 engine and console demonstration.
- JSON importer for **independently authored synthetic sample content only**.
- Load real `mod.json` files from disk, validate schemas and field types, and patch journal titles or text.
- Apply mod dependencies deterministically, with explicit conflict warnings and cycle/missing-dependency detection.
- Select **Classic Mode** (original unpatched content) or **Enhanced Mode** (overlay mods).
- Output session metadata with game identifier, mode, and applied mod IDs/versions; **not a full savegame**.
- TypeScript types for mod authoring; **compiled JS execution is not implemented**.
- Reject symlinked mod directories/manifests, invalid IDs, oversized JSON files, and unknown manifest fields.
- Read-only installation inventory with filenames, sizes, SHA-256, resource families and decoder diagnostics.
- Shared DAX index/RLE and GEO16 decoders exercised against one local Pool of Radiance installation; public tests remain synthetic.
- ECL typed operands, five lifecycle entries and bounded selector-table routing; unsupported/state-dependent static routes remain unresolved.
- Minimal deterministic ECL VM with typed memory, conditional branches, subroutines, text, menus, indexed memory reads and combat host callbacks.
- Terminal map movement and local save/restore, including original-event output and persisted menu state in one standalone map/ECL pair.
- Optional SDL3 exploration window with procedural walls, map overview, event text/menus and save/load; see [renderer controls and limits](runtime/README.md).
- SSI EGA tile and wall-definition decoding with explicit wall-bank rendering; original archive checks pass for one Pool installation, while automatic visual bindings and scene parity remain unverified.
- Game profiles for Pool of Radiance and Curse of the Azure Bonds (**both unverified**, not playable compatibility claims).

## Build and run

Requires a **C++20 compiler**, **CMake 3.20+**, and **Boost headers 1.75+** (including Boost.JSON).

On Ubuntu/Debian systems supporting Boost.JSON:

```bash
sudo apt-get install cmake ninja-build g++ libboost-dev
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

On Windows, install Boost through vcpkg or another supported CMake environment, then configure with your chosen generator and toolchain. Headless builds have no SDL/UI dependencies; enable `GOLDBOX_BUILD_RENDERER` and supply SDL3 >=3.2 for the exploration window.

For WSL builds, use `bash scripts/wsl-build.sh --no-tests=error` after configuring.
The wrapper serializes builds, uses `-j2`, and bounds build/test duration. Put a
wall-clock timeout on interactive or piped binaries; see [CONTRIBUTING.md](CONTRIBUTING.md).

From the repository root, after building:

```bash
./build/apps/goldbox_cli \
  --content examples/content/demo-campaign/journals.json \
  --mods examples/mods --journal 17 --mode enhanced

./build/apps/goldbox_cli \
  --content examples/content/demo-campaign/journals.json \
  --mods examples/mods --journal 17 --mode classic
```

The executable's location varies with the Windows generator; e.g. `build/apps/Debug/goldbox_cli.exe`.

Inspect a user-supplied DOS archive locally without modifying it:

```bash
./build/apps/goldbox_cli --scan-dax /path/to/GEO0.DAX \
  --game-profile games/pool-of-radiance/profile.json
```

For a read-only summary of map records and raw event counts, without printing copyrighted map geometry:

```bash
./build/apps/goldbox_cli --scan-geo /path/to/GEO1.DAX \
  --game-profile games/pool-of-radiance/profile.json

# Only when the exact GEO/ECL record pairing has been verified:
./build/apps/goldbox_cli --scan-geo /path/to/GEO1.DAX --geo-record 20 \
  --ecl-dax /path/to/ECL1.DAX --ecl-record 4
```

The identifiers above are illustrative. Scans do not alter source assets.
With a dispatch profile, explicit GEO/ECL pairing reports resolved move routes
and diagnostics for unresolved routes. Full original-game compatibility has not
been established.

Inventory an installation without writing extracted assets:

```bash
python scripts/scan_installation.py /path/to/POOLRAD --cli build/apps/goldbox_cli \
  --profile games/pool-of-radiance/profile.json > /outside/repository/inventory.json
```

Start the terminal pilot with a verified, explicitly selected map/ECL pair:

```bash
timeout 60 ./build/apps/goldbox_explore games/pool-of-radiance/profile.json \
  /path/to/GEO1.DAX 18 /path/to/ECL1.DAX 18 /outside/repository/state.json
```

Commands are `north`, `east`, `south`, `west`, `search`, `rest`, `init`, `save`,
`load`, and `quit`; menus accept their displayed numeric index. The pilot starts
at `(0,0)` with empty state and does not simulate full campaign initialization.
Nonzero walls block movement; doors and map transitions are not modeled. Saves
refuse overwrite and reject changed source pairs on load. Choose a fresh save
path for each run and keep saves, reports and captured game text outside this
repository. `timeout` is a WSL/Linux example; native Windows callers must also
bound the process lifetime in their launcher.

### Experimental JSONL parity-harness protocol

`goldbox_explore --harness <profile.json> <GEO.DAX> <geo-id> <ECL.DAX> <ecl-id>
<local-save.json>` runs the same exploration session behind a flushed JSONL
protocol instead of the terminal prompt:

- Startup emits a `ready` event with `protocol=1`, the source identity, initial
  position, and `baseline=unverified-empty`.
- Requests use a positive, strictly increasing integer ID, for example
  `{"id":1,"action":"search"}`. Actions are `north`, `east`, `south`, `west`,
  `search`, `rest`, `init`, `save`, `load`, and `quit`. Each line is bounded to
  64 KiB, and CRLF terminators are tolerated.
- VM callbacks surface as `text` and `menu` events; a `menu` expects one
  response such as `{"id":1,"menu_id":1,"choice":0}` with the pending action
  and menu IDs and a zero-based choice. Invalid responses end the session.
- Every accepted action answers with one `complete` event carrying
  `ok`/`blocked`/`unsupported` status, steps and bytecode telemetry, dispatch
  metadata, and the parsed save JSON as `state`; unknown VM failures are
  `unsupported`, never success.
- Protocol violations (malformed JSON, wrong id/action types or values, EOF
  without quitting, overlong lines) emit an `error` event and exit nonzero;
  `quit` completes and exits 0.
- Save/load keep every terminal-pilot safeguard (overwrite refusal, symlink/
  size checks, source-identity mismatch). No controller/emulator integration
  or original-parity claim exists yet; the startup state is an empty,
  unverified baseline. Stdout carries JSONL only, including protocol errors;
  callers must enforce a wall-clock deadline even while a menu awaits input.

Optional: add `--save-meta session.json` to create non-overwriting session metadata. This records only **the startup configuration**, not gameplay state.

### Bounded side-by-side preview controller (experimental, inconclusive)

`python scripts/compare_game.py <session-config.json>` drives **one** engine
harness session (the child engine command already carries `--harness` and its
explicit resource/save arguments) next to **one** reference process, renders
engine events for a human operator, and **never claims parity**: the session is
unpaired/inconclusive by design and no MATCH result is produced.

Version-1 session configuration (trusted local execution input; the controller
spawns with `shell=False` and never interpolates shell text; validated fully
before anything is spawned):

```json
{
  "schemaVersion": 1,
  "privateRoot": "C:/outside/the/repository/compare-work",
  "engine": {"command": ["build/apps/goldbox_explore.exe", "--harness",
                         "games/pool-of-radiance/profile.json", "..."],
             "cwd": "C:/abs/engine/dir"},
  "reference": {"command": ["C:/abs/reference/launcher.exe"],
                "cwd": "C:/outside/the/repository/compare-work/reference-copy"},
  "timeoutSeconds": 60,
  "checkpointSeconds": 10
}
```

Operational prerequisites and rules:

- Prepare a **private reference working copy** yourself; the controller never
  copies installed games or edits their config files. Point the reference
  command at that prepared copy and its graphical game/emulator window.
  Keep saves, captures and reports outside the repository. Any
  runnable reference command can still escape its `cwd` on its own; the
  controller does not claim sandboxing of arbitrary reference programs.
- `privateRoot` must be an existing absolute directory outside the repository;
  `reference.cwd` is required inside `privateRoot`. `timeoutSeconds` (default
  60, range 1..600) bounds the whole session; `checkpointSeconds` (default 10,
  positive, at most the total) prints known session state periodically and
  bounds inactivity while waiting for engine startup or an action response.
  Operator/menu input may wait until the total deadline.
- The controller holds an OS-backed single-session lock in `privateRoot`
  (`goldbox-compare.lock`; released on exit, no stale-lock deletion), then
  launches **both children as fully owned trees**:
  - Windows: a session Job Object with kill-on-close; the child is started
    suspended, assigned to the job, then resumed (no spawn-before-assignment
    escape race). Job Objects support killing every member on close or on
    explicit termination (see
    [Microsoft Job Objects](https://learn.microsoft.com/en-us/windows/win32/procthread/job-objects)).
  - POSIX: a dedicated session (`setsid`), so the descendant process group is
    killed via `killpg` at cleanup, even when the leader already exited.
    Trusted children must remain in that group; this is not containment for
    programs that deliberately detach into new sessions.
  - Windows callers can select the explicit WSL engine adapter described below.
    Raw `wsl.exe`/`wslg.exe` and the `System32\bash.exe` redirector remain
    rejected as native commands: a Windows Job Object alone cannot own their
    Linux children.
- The operator console renders engine `text`, ordered numbered menu choices,
  position/facing and unsupported diagnostics; raw protocol JSON stays out of
  the normal display. Requests carry strict positive increasing ids; menu
  responses reuse the exact pending action/menu ids. Reference input is manual
  by default. The optional shared-input mode below sends mapped keys only to
  the reference process's owned SDL window.
- Engine `error` events, malformed/out-of-order events, unexpected engine EOF,
  early reference exit, stdout flood (>64 MiB), overlong lines (>8 MiB) and the
  session deadline are all explicit failures that terminate the owned trees;
  controller input waiting never blocks past the deadline. Graceful success
  exists only through an acknowledged `quit`, clean engine exit, drained
  output and verified cleanup. EOF/Ctrl-C/deadline exits fail inconclusively.
  Original-side/reference output is drained
  with hard byte bounds and never rendered or logged.
- Synthetic tests run through the real CLI entry point (and are ctest-bound as
  `goldbox_compare_game_tests`); they cover the valid
  flow and exact id order, malformed configs/protocol, failures, deadline and
  flood, owned-descendant cleanup with an unrelated sentinel surviving, lock
  contention and release, spaced/metacharacter paths with no shell
  interpretation, and an explicit absence of parity-MATCH claims. CTest also
  runs the controller against the compiled engine with synthetic DAX/GEO/ECL
  resources and verifies event state persistence. The controller-only suite
  can run without a build: `python tests/compare_game_test.py`. Research on
  the underlying APIs used [the Python subprocess docs](https://docs.python.org/3.12/library/subprocess.html)
  and the Job Objects page above; no third-party code was adopted.

#### Engine inside WSL, reference window on Windows

Run the controller with Windows Python and replace only the `engine` object:

```json
"engine": {
  "backend": "wsl",
  "distribution": "Ubuntu",
  "cwd": "/mnt/c/absolute/path/to/goldbox-engine",
  "command": ["/mnt/c/absolute/path/to/goldbox-engine/build/apps/goldbox_explore",
              "--harness", "games/pool-of-radiance/profile.json",
              "/mnt/c/private/GEO.DAX", "7", "/mnt/c/private/ECL.DAX", "7",
              "/mnt/c/private/state.json"]
}
```

The record IDs above are placeholders for your independently verified local
mapping. `privateRoot`, reference command and reference working directory remain
Windows paths. Engine arguments and `cwd` are Linux paths; no automatic path
translation is attempted. The selected distribution must already have
`/usr/bin/python3` and `/proc`. No systemd user service, installation or network
listener is needed. Existing configurations default to the native backend.

The Linux supervisor relays the unchanged engine JSONL protocol over bounded
pipes. A five-second renewable controller lease and an independent total
deadline stop the session on disconnection or controller failure. Buffered stale
heartbeats cannot extend that lease. An unreaped session anchor reserves the
process-group ID until cleanup; it also kills its group if the supervisor dies.
As with the native POSIX backend, trusted engine children must not deliberately
detach into new sessions. Successful shutdown requires a session-specific
Linux cleanup acknowledgment, drained output and clean launcher exit. A lost
acknowledgment is an explicit failure even when the Linux deadline performs
cleanup. No distribution-wide shutdown or process-name killing is used.

Linux CI runs `goldbox_wsl_supervisor_tests`. On a Windows machine with Ubuntu,
`python tests/wsl_engine_test.py` also checks actual WSL bridging, menu responses,
operator idle lease renewal, controller crashes and unrelated Linux-session
survival. Optionally pass the absolute Linux engine executable and profile paths
as two arguments to include compiled-engine synthetic event/save validation.
These tests launch synthetic children only. The WSL adapter does not establish
parity; original-game input is handled by the separate opt-in mode below.
The implementation uses the Python standard
library and documented [Linux process-group behavior](https://man7.org/linux/man-pages/man2/setsid.2.html);
no third-party source was copied or adapted.

#### Shared input for both sides (Windows, SDL 1.2 reference)

Add `input` to the existing `reference` object. Launch the reference DOSBox
executable directly, using a private working copy and configuration that mounts
only that copy. Batch/Steam launchers are not supported for shared input: the
SDL window must belong to the exact launched reference PID.

```json
"input": {
  "backend": "windows-messages",
  "relativeMovement": {
    "forward": ["UP"], "left": ["LEFT"], "right": ["RIGHT"]
  },
  "actions": {"search": ["S"]},
  "menus": {"1": [["Y"], ["N"]]}
}
```

This is a mapping example, not a verified Pool event/menu profile. Supply the
actual original-game hotkeys for each supported action and exact engine menu
ID/choice ordering. Mapping `1` above is usable only for an independently
identified two-choice menu. Missing mappings or mismatched choice counts stop
before either side is dispatched. `save`, `load` and `init` also need explicit
reference mappings; engine saves are not SSI saves. `quit` is controller shutdown
and closes both owned process trees without needing an in-game quit mapping.

The controller launches both sides disarmed. The same stdin command stream can
be written by a person or an LLM driver:

```text
reference ENTER
arm
east
search
1
quit
```

Use `reference KEY ...` only while disarmed to stage the original from observed
screens. Supported keys include letters, digits, arrows, `ENTER`, `ESC`, `SPACE`,
`TAB`, `HOME`, `END`, `BACKSPACE`, and `KP_0`..`KP_9`; sequences are bounded to
16 keys. `arm` asserts that both sides are ready with matching facing; it does
not verify their state. `disarm` returns to staging. Relative movement computes
turns from the last engine facing before sending forward; alternatively, omit
`relativeMovement` and provide explicit absolute direction action mappings.
Unsupported engine actions stop shared input before another action is sent.

Each mapped action or menu response starts engine and reference delivery from
one dispatch gate and waits for both deliveries before accepting the next.
This is concurrent ordered delivery, not atomic execution or a shared emulator
clock. Failure can occur after one side receives input; the controller stops
both sides, reports possible partial delivery, and never retries automatically.
Win32 acknowledgment means the window processed keyboard messages, not that
the original game completed an action or reached an equivalent state. Reference
observation and gameplay parity remain separate work.

The backend forces `SDL_VIDEODRIVER=windib` only for the reference child. It uses
bounded key-down/up messages to exactly one visible `SDL_app` window of that
owned PID, rechecks ownership for each key, and never broadcasts, changes
foreground focus, or injects global keyboard input. Ambiguous or missing windows
fail closed. This is an SDL 1.2 backend, not generic support for SDL 2 or arbitrary
emulators. Research used documented
[Win32 messages](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendmessagetimeoutw)
and the upstream [SDL 1.2 message handler](https://github.com/libsdl-org/SDL-1.2/blob/main/src/video/windib/SDL_dibevents.c);
no upstream implementation was copied.

`python tests/reference_input_test.py` checks mapping on every platform and,
on Windows, actual message delivery to synthetic reference windows, shared
movement/menu input, disarmed/missing-map rejection, hung-window cancellation,
and an unrelated SDL window receiving no keys. CTest registers the same suite.

#### Verified Steam startup without a manual lookup

Local runtime validation on 2026-10-16 reached the original Pool of Radiance
party menu with Enter and loaded the supplied save into 3D exploration. The
code-wheel screen in this installed build did **not** require looking up an
answer. No original executable or installed configuration was modified.
Community reports of the same behavior were a research lead, not the validation:
[Steam startup discussion](https://steamcommunity.com/app/1882280/discussions/0/3185741160902745033/).

With the reference launched from a private copy, leave shared input disarmed
and send each command only after observing the indicated screen:

1. At the startup/credits screen, send `reference ENTER`. If the wheel prompt
   remains, send Enter at that prompt and verify the party menu before continuing.
2. At the party menu, send `reference L` for Load Saved Game.
3. At the save-slot prompt, send `reference A`. Slot A in the inspected private
   copy loaded a party into 3D exploration at the displayed `(15,4)`, facing west.

The three-command route was repeated through the harness on a fresh private
copy. This is a recipe with observed checkpoints, not a blind startup macro or
a claim that every edition accepts Enter. Save availability and content can
vary. Keep the supplied digital wheel/manuals for editions or in-game content
that need them; do not add proprietary answers or assets to this repository.

Build fingerprints for the local observation (SHA-256):

- `START.EXE`, 65,231 bytes:
  `d825df2b174675c9088ba1489488bdeebe66ad2a22943f17d3a198e60b6a07bd`
- `DOSBox.exe` 0.74-3, 3,745,792 bytes:
  `dcfd46fa521f5ce89dce3bf026056f3a1d15533f80321ee887403e30d7949f5e`

Successful original startup does not establish an equivalent engine baseline.
The prototype engine still starts with independently initialized state; do not
arm shared gameplay merely because the original save has loaded. Map identity,
coordinates, facing, party and event state still need independent alignment.

For TypeScript mod SDK authoring (requires Node.js and npm):

```bash
cd packages/mod-sdk
npm install
npm run check
```

## Repository shape

```text
apps/                Console integration and CLI
engine/include/      Public engine/content/mod interfaces
engine/src/          Core, Boost.JSON adapter, game profiles, import/mod implementations
formats/             Shared DAX archive, ECL lexer, GEO map decoders (still unverified)
games/               Declarative per-game compatibility profiles (unverified)
examples/content/    Entirely fictional original sample campaign
examples/mods/       External sample data mod
packages/mod-sdk/     Proposed TypeScript mod manifest API
importers/           Future original-game binary importers
runtime/             Future SDL3, UI and JS scripting runtime
gameplay/            Future events/ECL, rules and tactical combat
docs/                Decisions, architecture, OpenXcom comparison, licenses
tests/               Unit and integration tests
scripts/             Hygiene checks
```

## Design and legal safeguards

- User supplies legally obtained SSI resources. No proprietary original assets, binaries, journal passages, or manuals are distributed.
- OpenXcom and SSI Engine can now be evaluated for **GPL-compatible source reuse**, with file-level licensing, provenance and attribution review; none of their code has been incorporated so far.
- **Open-source first:** evaluate existing libraries and Gold Box tooling before custom implementations. See [reuse policy and candidate inventory](docs/open-source-first.md).
- Only incorporate external code once its file-level licenses, provenance, notices, and dependency obligations are reviewed.
- This project uses Boost.JSON via the user's Boost installation (Boost Software License 1.0); it does not vendor the Boost source.
- Original game compatibility requires separate importer implementations and independently validated behavior.

See the [upstream attribution registry](attribution/README.md) for exactly which projects and source files we've adapted or linked, and [THIRD_PARTY.md](THIRD_PARTY.md) for notices and modifications.

See the [shared importer architecture](docs/universal-importer.md), [DAX format documentation](docs/formats/dax-v1.md), and [ECL decoding pilot](docs/formats/ecl-v1.md), and [GEO decoding pilot](docs/formats/geo-v1.md). See also the [OpenXcom mod architecture + Pool of Radiance resource investigation](docs/research/openxcom-mod-system-and-pool-of-radiance.md) and [proposed master/ruleset design](docs/decisions/0002-master-campaigns-and-modular-rulesets.md).

For the current, working M1 implementation, see [mod format](docs/mod-format.md), [architecture](docs/architecture.md), [roadmap](docs/roadmap.md), [licensing policy](docs/licensing-and-provenance.md), and [dependency inventory](THIRD_PARTY.md).

## License

Project-owned code is offered under **GNU GPL version 3 or any later version** (SPDX: `GPL-3.0-or-later`). See [LICENSE](LICENSE) for the full GPLv3 text, [LICENSING.md](LICENSING.md) for relicensing details, and [THIRD_PARTY.md](THIRD_PARTY.md) for separately licensed dependencies. Earlier published MIT-licensed revisions remain available under their original MIT terms.
