# Proposed mod sandbox requirements

- Compile TS to JS at mod-development time; ship JS + metadata to players.
- Embed QuickJS-NG and explicitly expose a small versioned API.
- Do not expose Node `require`, fs, network, OS process, or native FFI.
- Per-mod memory limits, time/instruction budgets, and deterministic RNG.
- Module imports restricted to known packaged files, with canonicalized paths.
- Mods must declare capability requests and affected content namespaces.
- Persist compatibility fingerprints in savegames.
- Treat mods and imported SSI files as untrusted inputs.
- Add fuzzing for proprietary-format parsers before accepting arbitrary files.

These controls are **design requirements**, not implemented functionality.
