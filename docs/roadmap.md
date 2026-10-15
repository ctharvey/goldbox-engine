# Project roadmap and acceptance criteria

## M0 — Initial scaffold
- [x] Independent C++20 project builds
- [x] Synthetic journal resolver tests
- [x] TypeScript mod API draft
- [x] Licensing and provenance policy
- [x] Public GitHub repository initialized: https://github.com/ctharvey/goldbox-engine

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

## Research / design investigation (2026-10-12)
- [x] Inspect OpenXcom's actual master-mod / extension layout and declarative ruleset conventions
- [x] Identify DOS Pool of Radiance DAX resource families and ECL event responsibilities
- [x] Document master/mod architecture proposal (ADR-0002), initially written before GPL relicensing
- [ ] Decide whether to accept ADR-0002 after schema review
- [ ] Prototype modular category rulesets with M1 backwards compatibility
- [ ] Build read-only DAX header/index parser against synthetic files, then validate with user-owned original game resources

Research: [OpenXcom and Pool of Radiance](research/openxcom-mod-system-and-pool-of-radiance.md). Design: [ADR-0002](decisions/0002-master-campaigns-and-modular-rulesets.md).

## Open-source adoption track (2026-10-12)
- [x] Audit actual implementations, source-file licenses, provenance and maintenance across Gold Box Explorer, SSI Engine, OpenXcom, UAF/Dungeon Craft, OpenUA and others
- [x] Identify pilot reuse targets and avoid-code-copy licensing blockers: [source audit](research/oss-reuse-audit-2026-10-12.md)
- [x] Port and attribute bounded ECL lexical/opcode decoding from MIT Gold Box Explorer behavior; synthetic unit tests pass (unvalidated on SSI resources)
- [x] Adapt MIT Gold Box Explorer GEO 16×16 map/wall/event decoder with preserved upstream license and synthetic tests (not SSI validated)
- [x] Cross-check the existing DAX parser against other OSS implementations and owned-game archives
- [ ] Review SSI Engine ECL VM source and version-specific GPL terms before any translation
- [ ] Clarify OpenUA's GPLv2-root/MIT-design-document mismatch prior to any code reuse

## M2 — Shared formats and first original title (Pool of Radiance)
- [x] Shared DAX archive scanner + bounded record RLE extraction against synthetic fixtures
- [x] Data-driven, unverified DOS profiles for Pool of Radiance and Curse of the Azure Bonds
- [x] Read-only CLI scan mode (no original assets or resources committed)
- [ ] Implement GLB decoder for later variants; identify further title-specific ECL dialects
- [x] User-selected installation inventory, SHA-256 fingerprints and decoder diagnostics
- [ ] Complete original SSI decoder coverage (DAX/GEO layout and ECL lifecycle checks completed on one Pool installation; sprites and complete ECL dialect coverage pending)
- [ ] Complete selector-to-entry-point coverage across original records (bounded selector routes and typed runtime selections implemented; state-dependent static routes remain unresolved)
- [x] Expand synthetic tests for dispatch, bounded execution, exploration and persistence
- [x] Keep original assets and local derived content out of public code and tests

## M3 — Playable vertical slice
- [x] Navigate one imported map in the standalone terminal pilot
- [x] Execute a supported original menu event and persist its resulting state (narrow map-18 validation only)
- [ ] Display journal text sourced from user-provided materials
- [ ] Toggle external mod without corrupting imported content
- [x] Save and restore prototype position and event state; full campaign/mod save compatibility remains pending

Evidence: [local validation, 2026-10-15](research/local-validation-2026-10-15.md).
This standalone slice uses empty initial state and an explicit map/ECL pairing;
it does not establish original campaign startup or gameplay parity.

## M4 — Gameplay parity
- [ ] Party management, AD&D spell lists, combat, inventory, encounters
- [ ] End-to-end original campaign and deterministic regression tests

## M5 — Extensible modding and tools
- [ ] QuickJS-NG host + TypeScript SDK + versioned capabilities
- [ ] Explicit script resource budgets; no OS/network access by default
- [ ] Content inspector, map and event editors, mod diagnostics
- [ ] Additional Gold Box campaigns and other SSI game families
