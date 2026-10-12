# Third-party licensing and provenance

## Actually used (M2)

| Library | License | Use | Distribution |
|---|---|---|---|
| Boost.JSON (Boost 1.75+, tested with 1.83) | Boost Software License 1.0 | JSON parsing/serialization | Supplied by package manager; not vendored |

Upstream: https://github.com/boostorg/json
License: https://www.boost.org/LICENSE_1_0.txt

Boost.JSON headers are compiled as part of this application using `boost/json/src.hpp` in exactly one translation unit. The Boost copyright/license notices remain with the installed library; package redistributors must comply with the Boost license.

## Research / prospective reuse (no source copied into this project)

- OpenXcom: https://github.com/OpenXcom/OpenXcom — GPL-3.0-or-later. Source reuse is potentially permitted under our new license after verifying individual files and preserving notices; none has been included.
- Gold Box Explorer: https://github.com/bsimser/Gold-Box-Explorer — MIT. Evaluate individual parser files and copyright notices before adapting.
- SSI Engine: https://github.com/TiCoKH/ssi-engine — GPLv3 license text at repository root; verify individual file terms and whether 'or later' permission exists before adopting. Research reference only thus far.
- QuickJS-NG: https://github.com/quickjs-ng/quickjs — MIT. Future script host candidate; not integrated.

## Reuse evaluation requirement

Before writing a new parser, event runtime, UI framework, renderer, or script host, consult [the open-source-first policy](docs/open-source-first.md). Record candidates and file-level GPL compatibility/licensing review before adopting or porting code. The candidate inventory in that policy is **not** a claim that the libraries are installed.

## Future optional dependencies (not installed by this project)

- SDL3 for windowing/input/rendering.
- RmlUi for interface rendering.
- QuickJS-NG for executing JS emitted by the TypeScript compiler.

No original SSI game files or code, and no copied OpenXcom or SSI Engine implementation, are currently distributed by this repository. Existing Boost.JSON remains independently licensed under BSL-1.0. See [LICENSING.md](LICENSING.md) for project licensing and historical MIT releases.
