# Third-party licensing and provenance

## Actually used at M1

| Library | License | Use | Distribution |
|---|---|---|---|
| Boost.JSON (Boost 1.75+, tested with 1.83) | Boost Software License 1.0 | JSON parsing/serialization | Supplied by package manager; not vendored |

Upstream: https://github.com/boostorg/json
License: https://www.boost.org/LICENSE_1_0.txt

Boost.JSON headers are compiled as part of this application using `boost/json/src.hpp` in exactly one translation unit. The Boost copyright/license notices remain with the installed library; package redistributors must comply with the Boost license.

## Research references (none copied into this project)

- OpenXcom: https://github.com/OpenXcom/OpenXcom — GPL-3.0-or-later. Architectural concepts only.
- Gold Box Explorer: https://github.com/bsimser/Gold-Box-Explorer — MIT. Evaluate individual parser files and copyright notices before adapting.
- SSI Engine: https://github.com/TiCoKH/ssi-engine — GPL-3.0. Research/interoperability reference only; no code copied.
- QuickJS-NG: https://github.com/quickjs-ng/quickjs — MIT. Future script host candidate; not integrated.

## Future optional dependencies (not installed by this project)

- SDL3 for windowing/input/rendering.
- RmlUi for interface rendering.
- QuickJS-NG for executing JS emitted by the TypeScript compiler.

No original SSI or OpenXcom game files/code are distributed by this repository.
