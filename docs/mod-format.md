# Mod manifest v1 — implemented journal overlay subset

Each mod is a separate direct subdirectory of the chosen mods directory, containing a non-symlinked `mod.json`.

```json
{
  "schemaVersion": 1,
  "id": "example.journal-enhancer",
  "version": "0.1.0",
  "priority": 100,
  "requires": [],
  "patches": [
    { "target": "journal", "id": 17, "title": "Old Tower (Enhanced)", "body": "Invented replacement text" }
  ]
}
```

- `schemaVersion`, `id`, and `version` are required; unknown fields are errors.
- `id`: lower-case letters, digits, periods, underscores and hyphens (no `..`, starts with letter or digit).
- `version`: `major.minor.patch`, optionally `-prerelease`. Exact versions are recorded but dependency version ranges are not yet implemented.
- `priority`: optional signed integer from -10000 through 10000; default 0.
- `requires`: optional list of installed mod IDs. Missing dependencies and dependency cycles abort loading.
- `patches`: optional array; only `journal` overrides are supported currently, with positive integer IDs and optional `title` or `body` strings. Each patch must change at least one field.
- Patches to unknown journal IDs fail validation. Duplicate journal patch IDs within a single mod are errors.
- The load order is topological (dependencies first), then ascending priority and lexicographic ID when multiple mods are eligible. Thus explicit dependencies take precedence over numeric priority.
- When multiple mods override the same journal field, a warning identifies both mods; the later mod in resolved order wins.
- Classic Mode ignores mod overlays without mutating imported base content.

**Security at M1:** JSON source files must be regular files of at most 4 MiB. The loader scans immediate mod subdirectories only, disallows symlinked subdirectories and manifest files, and does not execute scripts. It is not a complete untrusted-content sandbox or filesystem race-proof loader.

**Future:** TypeScript mods compiled to JavaScript run in a capability-limited embedded engine, not Node.js. No script host is implemented at M1.
