# Goldbox Engine (working title)

An open-source, GPL-3.0-or-later-licensed, OpenXcom-inspired framework for playing and enhancing classic SSI Gold Box RPGs **using original resources supplied by the player**.

**Status: bounded exploration pilot.** M1 journal mods, installation scanning,
DAX/GEO/ECL decoding, selector routing, a minimal ECL VM, and terminal exploration
with state persistence are implemented. A standalone Pool of Radiance map/event
has been exercised against user-owned Steam resources. Full campaign startup,
combat, rendering, and embedded JS remain unimplemented; game compatibility is
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

On Windows, install Boost through vcpkg or another supported CMake environment, then configure with your chosen generator and toolchain. The program presently has no SDL/UI dependencies.

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
