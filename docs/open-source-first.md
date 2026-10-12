# Open-source-first engineering policy

Status: Active project policy (2026-10-12)

## Default rule: research before implementation

Before developing a substantial format parser, rendering/UI subsystem, compression decoder, scripting runtime, mod-management tool, editor, or other general-purpose component:

1. Search existing open-source implementations and libraries, including specialized Gold Box tools.
2. Identify at least one candidate where available, and record version/revision, upstream URL, exact license, feature coverage, maintenance, language and dependency implications.
3. Prefer adopting, linking, adapting, or porting **compatible permissively licensed** existing work over writing a new implementation when this saves effort and improves reliability.
4. Assess quality: correctness against real/synthetic fixtures, bounds checks, portability, test coverage, runtime/security considerations, and integration cost. A dead/unsafe dependency is not automatically better than a small independent implementation.
5. Record a short **reuse vs. reimplement** decision in the issue/PR (and a design note for larger components). Explain when existing software is not suitable.
6. If reusing source: audit file-level licensing and contributor provenance, retain notices, and record exact path, upstream URL and commit/release, changed files, attribution and license obligations in `THIRD_PARTY.md`. Audit transitive dependencies and bundled resources independently.
7. Verify the dependency's license obligations against the project's MIT distribution policy. Escalate copyleft, unknown, incompatible, or asset-licensing concerns before copying. Preserve the separate-asset model: users supply SSI game files.
8. Build a compatibility test for adopted or independently reimplemented code, with reproducible synthetic fixtures; never commit proprietary original game data.

**Important:** Permissive OSS licenses often permit copying/adapting code with attribution, but this is not the same as unrestricted reuse. GPL-covered OpenXcom and SSI Engine code must not be copied, mechanically translated, or derived into the MIT-licensed core under the current policy. Their high-level architecture and independently verified file-format behavior may inform design.

## Known candidates for early evaluation

These are **candidates, not installed dependencies**, unless listed separately as used in `THIRD_PARTY.md`.

| Need | Project | License checked | Integration notes / decision |
| --- | --- | --- | --- |
| Gold Box archive, bitmap and character file knowledge | [Gold Box Explorer](https://github.com/bsimser/Gold-Box-Explorer) | MIT at repository root; audit source files and history before reuse | Existing C# parsing resources may inform or provide attributable, adapted C++ routines; evaluate first instead of implementing every format ourselves |
| JSON content and rulesets | [Boost.JSON](https://github.com/boostorg/json) | Boost Software License 1.0 | **Already used**, package-managed and not vendored |
| JavaScript runtime for TypeScript-authored mods | [QuickJS-NG](https://github.com/quickjs-ng/quickjs) | MIT | Prefer embedded library to custom JS runtime; type-check/compile TypeScript outside the engine |
| 2D rendering, audio and input | [SDL3](https://github.com/libsdl-org/SDL) | zlib | Use library rather than implementing platform backends |
| Declarative game UI | [RmlUi](https://github.com/mikke89/RmlUi) | MIT for core; bundled fonts/assets have additional licenses | Review dependencies/assets before integration; avoid custom layout engine |
| Game inspired by Gold Box | [goldbox-rpg](https://github.com/opd-ai/goldbox-rpg) | README claims MIT; exact file provenance not yet audited | Research candidate only; Go implementation and scope may not be useful for original-game compatibility |
| Architectural inspiration | [OpenXcom](https://github.com/OpenXcom/OpenXcom) | GPL (copyleft) | **No code reuse** in the MIT project; conceptual mod/package organization only |
| Original-game behavioral compatibility | [SSI Engine](https://github.com/TiCoKH/ssi-engine) | GPL (copyleft) | **No code reuse** in the MIT project; interoperability research only |

License source references:
- Gold Box Explorer: https://github.com/bsimser/Gold-Box-Explorer/blob/master/LICENSE
- QuickJS-NG: https://github.com/quickjs-ng/quickjs/blob/master/LICENSE
- SDL3: https://github.com/libsdl-org/SDL/blob/main/LICENSE.txt
- RmlUi: https://github.com/mikke89/RmlUi/blob/master/LICENSE.txt
- Boost Software License: https://www.boost.org/LICENSE_1_0.txt

## Every new feature PR answers

- **Existing OSS reviewed:** links and version/commit.
- **Choice:** reuse as dependency, adapt/port, or implement independently; why.
- **License/provenance:** reviewed files, transitive assets, notice obligations, additions to `THIRD_PARTY.md`.
- **Quality/testing:** functionality validated by synthetic fixtures and/or private locally owned game assets.
- **Security/maintenance:** who patches vulnerabilities and how dependency versions are pinned.

## Keep public repository clean

Do not commit game executables, SSI artwork, manuals/journal passages, converted original campaigns, proprietary save files, or game-derived fixture payloads. Keep local game-data imports outside the repository.
