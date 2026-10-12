# Upstream attribution inventory

This project reuses open-source code with explicit attribution. The source-of-truth registry is [`sources.json`](sources.json), and the human-oriented explanation is [`../THIRD_PARTY.md`](../THIRD_PARTY.md).

## Status categories

- **`adapted`** — third-party implementation actually incorporated or translated into our code. Requires immutable upstream commit, exact original paths, local destination files, copyright notice, preserved license text and a summary of changes.
- **`dependency`** — externally installed or linked library used at build/runtime. Record the integration point, license and version/pinning policy; no vendor attribution is implied.
- **`reference`** — consulted for architecture/observable behavior; **no upstream implementation copied**. This does not count as adopting source.

The registry records provenance; it is **not** a substitute for actual license permissions or a review of individual upstream copyrights.

## Required workflow

For every newly reused component, add an entry to `sources.json` with a stable `id`. For adapted source, place **`// Attribution-ID: <id>`**, original upstream copyright information and the applicable SPDX identifier in the top of every adapted local source file. Keep required upstream license texts under `licenses/` and record modifications in `THIRD_PARTY.md`. Use immutable Git revision identifiers rather than links to moving branches.

Run `python3 scripts/check_attribution.py` locally. CI runs the same check. It validates inventory shape, referenced local files, notices and coverage, but **does not perform a complete license/provenance audit**. Original SSI game data is never copied here.

Note: `upstreamRevision: null` is acceptable for a system-supplied library whose installed version is selected by the builder; the package/version policy must still be documented.
