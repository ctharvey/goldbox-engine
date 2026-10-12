# Goldbox Engine (working title)

An independent, MIT-licensed, OpenXcom-inspired framework for playing and enhancing classic SSI Gold Box RPGs **using original resources supplied by the player**.

**Status: M1 — working JSON content importer and external journal mod loader. Not a playable Gold Box game.** Original SSI file decoders, adventure event processing, combat, renderer, and embedded JS runtime are not implemented.

## What actually works today

- Buildable, testable C++20 engine and console demonstration.
- JSON importer for **independently authored synthetic sample content only**.
- Load real `mod.json` files from disk, validate schemas and field types, and patch journal titles or text.
- Apply mod dependencies deterministically, with explicit conflict warnings and cycle/missing-dependency detection.
- Select **Classic Mode** (original unpatched content) or **Enhanced Mode** (overlay mods).
- Output session metadata with game identifier, mode, and applied mod IDs/versions; **not a full savegame**.
- TypeScript types for mod authoring; **compiled JS execution is not implemented**.
- Reject symlinked mod directories/manifests, invalid IDs, oversized JSON files, and unknown manifest fields.

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
engine/src/          Core, Boost.JSON adapter, import/mod implementations
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
- Refer to OpenXcom for **architecture**, not copied GPL implementation.
- Only incorporate external code once its file-level licenses, provenance, notices, and dependency obligations are reviewed.
- This project uses Boost.JSON via the user's Boost installation (Boost Software License 1.0); it does not vendor the Boost source.
- Original game compatibility requires separate importer implementations and independently validated behavior.

See [OpenXcom architectural comparison](docs/openxcom-architecture-notes.md), [mod format](docs/mod-format.md), [architecture](docs/architecture.md), [roadmap](docs/roadmap.md), [licensing policy](docs/licensing-and-provenance.md), and [dependency inventory](THIRD_PARTY.md).

## License

Original code in this project is MIT-licensed (see [LICENSE](LICENSE)). Dependencies remain subject to their respective licenses.
