# Project roadmap and acceptance criteria

## M0 — Initial scaffold
- [x] Independent C++20 project builds
- [x] Synthetic journal resolver tests
- [x] TypeScript mod API draft
- [x] Licensing and provenance policy
- [ ] Remote GitHub repo initialized (GitHub connector has no create-repository action)

## M1 — Real content overlays (2026-10-12)
- [x] JSON synthetic content importer and independent `ContentImporter` interface
- [x] `mod.json` loader and strict schema checks
- [x] Reject symlinked mod packages, invalid/oversized files, bad IDs and unknown keys
- [x] Dependency ordering, collision warnings, missing dependency/cycle detection
- [x] Classic/enhanced mode switch and external mod overlay demo
- [x] Session metadata records mod IDs/versions and classic/enhanced mode
- [x] Unit and CLI integration tests
- [ ] Version range constraints for dependency resolution
- [ ] Proper gameplay state and savegame persistence
- [ ] Migration/version validation for existing saves
- [ ] Race-hardened mod filesystem loading and broader fuzzing

## M2 — Import first original title (Pool of Radiance)
- [ ] User-selected original installation and format version fingerprinting
- [ ] Implement independent decoders (maps, sprites, journals/events)
- [ ] Normalize ECL event semantics and link journal references
- [ ] Expand test suite with synthetic/reverse-engineered behavioral fixtures
- [ ] Never include original assets in repository, build, or tests

## M3 — Playable vertical slice
- [ ] Navigate one imported map
- [ ] Execute its original events and state transitions
- [ ] Display journal text sourced from user-provided materials
- [ ] Toggle external mod without corrupting imported content
- [ ] Save and restore deterministic progression

## M4 — Gameplay parity
- [ ] Party management, AD&D spell lists, combat, inventory, encounters
- [ ] End-to-end original campaign and deterministic regression tests

## M5 — Extensible modding and tools
- [ ] QuickJS-NG host + TypeScript SDK + versioned capabilities
- [ ] Explicit script resource budgets; no OS/network access by default
- [ ] Content inspector, map and event editors, mod diagnostics
- [ ] Additional Gold Box campaigns and other SSI game families
