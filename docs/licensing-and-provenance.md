# Licensing and copyright guardrails

**Current project license: GPL-3.0-or-later** for project-owned code. The full GNU GPLv3 text is in [../LICENSE](../LICENSE); see [../LICENSING.md](../LICENSING.md) for historical MIT releases and third-party distinctions.

## Reusing open-source code

- Research existing libraries and game-compatible parsers *before* custom implementation.
- GPLv3-compatible source may be incorporated with its existing license notices and corresponding-source obligations. GPL-3.0-or-later, GPL-3.0-only, LGPL and permissive components each require their **specific terms** to be reviewed.
- Do not assume that every GPL-labeled project or file is compatible: GPL-2.0-only and GPLv3 are generally incompatible; 'GPL-3.0-only' source can constrain later-version relicensing of a combined distribution.
- Verify file-level exceptions, dual-license options, contributor rights, third-party assets and dependency obligations; review AGPL and other copyleft components explicitly.
- Preserve license texts/copyright notices when required, and log source URLs, revisions, modified paths and attribution in [../THIRD_PARTY.md](../THIRD_PARTY.md).
- GPL imposes obligations when **conveying/distributing** covered code or binaries; private modifications not conveyed ordinarily do not trigger source publication obligations.
- The engine's license does not automatically make independent user mods or campaigns GPL; whether a tightly coupled plugin forms a covered combined work is fact-specific.

## Original SSI intellectual property remains separate

- Do **not** distribute original SSI executables, game data, art, maps, journals, manual scans, game text, or proprietary derived fixtures in Git or released builds.
- Import original resources from user-selected, legally obtained local installations. Keep generated caches local by default.
- No special license is granted for SSI trademarks, names or proprietary content by this project's GPL license.

## Reuse acceptance checklist

1. Define feature and compare existing OSS alternatives.
2. Confirm exact license and 'or later' wording for the specific code and dependencies.
3. Confirm author/provenance, third-party portions, required notices, attribution and modification requirements.
4. Record incorporated source with URL, upstream SHA/tag, license, modifications and obligations in `THIRD_PARTY.md`.
5. Add or retain copyright and SPDX identifiers in new project-owned code where practical: `SPDX-License-Identifier: GPL-3.0-or-later`.
6. Verify license/source distribution compliance and no SSI proprietary assets in CI/release artifacts.
7. Test with synthetic fixtures; run owned-game data checks locally without committing assets.

This is a project engineering policy, not legal advice.
