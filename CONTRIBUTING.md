# Contribution policy

1. **Open-source first:** before building a substantial component, evaluate reusable open-source projects and document the reuse-versus-reimplementation decision. Follow [the OSS reuse policy](docs/open-source-first.md).
2. The project-owned code is licensed **GPL-3.0-or-later**. Contributors must have the rights to submit their changes under GPL-3.0-or-later; clearly identify files that retain their own compatible third-party licenses.
3. You may propose **GPL-compatible code reuse** from OpenXcom, SSI Engine, Gold Box Explorer, and other projects, but do **not** copy anything until the precise file license, version/or-later status, author provenance, notices, and dependencies are reviewed. In particular, GPL-2.0-only code is not automatically compatible with GPLv3, and GPL-3.0-only code cannot automatically be offered under future GPL versions.
4. For any copied or adapted material, identify upstream URL and commit, preserve copyright/license notices, list source files and modifications in `THIRD_PARTY.md` **and `attribution/sources.json`**, add `Attribution-ID` markers to adopted code, and retain compatibility requirements. Run `python3 scripts/check_attribution.py` before submitting.
5. Cite independently observed file-format behavior and published specifications when doing interoperability work.
6. Never commit SSI executables, assets, game data, maps, proprietary journal/manual text, or materials from other games without redistribution permission. Use synthetic fixtures in CI.
7. Prefer deterministic tests, narrow interfaces and explicit failure handling. Run `cmake --build build && ctest --test-dir build` plus repository hygiene checks before contributing.
8. Distinguish functional implementations from planned or unvalidated compatibility, and record testing and provenance in the PR.
9. Distributing a covered binary entails GPL corresponding-source and notice obligations. See [LICENSING.md](LICENSING.md) and the [official GPLv3 text](LICENSE).

Contributions to project-owned files are intended to be available under **GPL-3.0-or-later**, unless clearly identified as separately licensed compatible code and accepted as such. Earlier publicly released MIT-licensed revisions remain subject to their original license grants.
