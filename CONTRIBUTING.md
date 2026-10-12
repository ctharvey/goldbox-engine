# Contribution policy

1. MIT original work only, unless maintainers explicitly re-evaluate licensing.
2. Cite reverse-engineered behavioral facts and original publicly available
   specifications in PRs; do not copy/translate GPL code implementations.
3. Record any third-party code and license in `THIRD_PARTY.md` before merging.
4. Never commit copyrighted SSI assets, executable files or journal/manual text.
5. Prefer synthetic tests, deterministic behavior, and narrow interfaces.
6. Use `cmake --build build && ctest --test-dir build` before submitting.
7. Distinguish working code from planned components in documentation.
