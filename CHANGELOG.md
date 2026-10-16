# Changelog

## 2026-10-16

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
