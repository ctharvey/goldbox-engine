# Licensing and copyright guardrails

**Project intent: maintain an independent MIT-licensed codebase.**

## Allowable reference and reuse

- Ideas, user-visible functionality, public factual file-format specifications,
  interoperability behavior, and high-level project/module organization can
  inform an independently implemented engine. Do not copy protected expression.
- Reuse third-party source only after identifying its exact license, file-level
  exceptions, copyright holders, and transitive dependency obligations.
- MIT/BSD/Apache-2.0 candidates may be reused when their conditions are met.
  Preserve attribution and license notices in any actual copies.
- A GPL project can still be an architectural reference, but incorporating or
  adapting GPL implementation into the application is prohibited under the
  current MIT-only project policy unless licensing is revisited.

## Protected materials

- Do not publish, bundle, or upload any original SSI executables, image/audio
  assets, maps, character names/dialogue/journal passages copied from game
  material, or manuals.
- Future installers may locate game files in a user-selected owned install.
- Redistributable tests use synthetic fixtures only.
- Project branding must not imply authorization by rights holders.

## Provenance checklist for each contribution

1. Is the code independently authored? If not, identify source file and URL.
2. Identify exact license version and obligations, including modifications.
3. Record copied or adapted code in `THIRD_PARTY.md` and preserve notices.
4. Reject ambiguous/proprietary/GPL code for the MIT core pending review.
5. Ensure no original SSI art/text/code was committed, including test fixtures.
6. Run license/asset checks as part of CI as the project grows.

## Legal caveat

Separate original asset distribution avoids one major issue, but does not
automatically settle all intellectual-property questions. Compatibility
implementations and trademarks still deserve case-specific review.
This is a development policy, not legal advice.
