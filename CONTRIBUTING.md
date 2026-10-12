# Contribution policy

1. **Open-source first:** before building a substantial component, review existing open-source libraries and implementations, record candidates and reuse-vs-reimplement rationale. Follow [the OSS reuse policy](docs/open-source-first.md).
3. Keep the core MIT-compatible; every reused source file needs exact license/provenance verification and notices. Only review GPL projects for architecture and observable interoperability behavior unless licensing is explicitly reconsidered.
3. Cite reverse-engineered behavioral facts and original publicly available
   specifications in PRs; do not copy/translate GPL code implementations.
4. Record any third-party code and license in `THIRD_PARTY.md` before merging.
5. Never commit copyrighted SSI assets, executable files or journal/manual text.
6. Prefer synthetic tests, deterministic behavior, and narrow interfaces.
7. Use `cmake --build build && ctest --test-dir build` before submitting.
8. Distinguish working code from planned components in documentation.
