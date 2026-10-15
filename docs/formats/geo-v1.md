# GEO 16×16 decoder pilot (M2)

**Status:** implemented with synthetic tests and layout checks on 29 local Pool
of Radiance GEO records. Wall/door semantics and full game compatibility remain
unverified. A separate bounded resolver handles supported selector routes;
unknown game-state dependencies remain unresolved.

## Format source and provenance

Adapted to C++20 from the MIT-licensed **Gold Box Explorer**, by Bil Simser:

- [`GeoDaxFile.cs`](https://github.com/bsimser/Gold-Box-Explorer/blob/eac30abaa6ee66aea6f5d65ebe6d676b10015a8f/src/Common/Plugins/GeoDax/GeoDaxFile.cs), notably `ProcessBlocks()`;
- [`GeoWallRecord.cs`](https://github.com/bsimser/Gold-Box-Explorer/blob/eac30abaa6ee66aea6f5d65ebe6d676b10015a8f/src/Common/Plugins/GeoDax/GeoWallRecord.cs);
- [`GeoMapRecord.cs`](https://github.com/bsimser/Gold-Box-Explorer/blob/eac30abaa6ee66aea6f5d65ebe6d676b10015a8f/src/Common/Plugins/GeoDax/GeoMapRecord.cs).

Source revision: `eac30abaa6ee66aea6f5d65ebe6d676b10015a8f`. Original MIT license (copyright 2018 Bil Simser) retained in [`licenses/gold-box-explorer-MIT.txt`](../../licenses/gold-box-explorer-MIT.txt). See [`THIRD_PARTY.md`](../../THIRD_PARTY.md). No original game maps, map names, images or binaries are distributed.

## Layout of one **decompressed** GEO DAX record

The specialized 16×16 decoder requires **at least 1026 bytes**. It does not yet claim to support every Gold Box title or platform.

| Offset | Length | Interpretation |
|---|---:|---|
| `0..1` | 2 | Preserved opaque header bytes; semantics not yet verified |
| `2..257` | 256 | North wall nibble (`byte >> 4`) and east wall nibble (`byte & 15`) |
| `258..513` | 256 | South wall nibble and west wall nibble |
| `514..769` | 256 | Raw event bytes; low 7 bits are an event selector, high bit preserved without assumed meaning |
| `770..1025` | 256 | Raw door flags; orientation/locking semantics not yet interpreted |
| `1026+` | variable | Unknown trailing data, length recorded but not interpreted |

Each plane is indexed by `row * 16 + column`. `GeoMap::cell(row, col)` checks both coordinates and throws on invalid access. `GeoEventReference` reports `(GEO record ID, row, column, raw event, event selector)` for **every nonzero raw event byte**. This intentionally includes `0x80` (selector zero with a high bit) without claiming it is an executable event.

## GEO → ECL association

`bind_geo_events_to_ecl(map, eclArchive, recordId)` only binds a map to an **explicitly selected ECL DAX record**, after checking the ECL record exists and extracts. It does *not* guess matching IDs, scan arbitrary bytecode for patterns, or confuse the selector with an ECL byte offset. A future game-specific map/ECL dispatch decoder must resolve `(record ID, event selector)` to a **verified** `(bytecode offset, dialect)` before invoking the ECL instruction decoder.

This distinction matters: Gold Box Explorer's ECL discovery currently scans for byte patterns such as `0x21` without instruction-boundary validation. That is a useful research heuristic, **not** a reliable import contract. We explicitly avoid silently accepting false positives.

## Read-only CLI

Scan locally owned resource archives without printing or saving proprietary map contents:

```sh
./build/apps/goldbox_cli --scan-geo /path/to/GEO1.DAX \
  --game-profile games/pool-of-radiance/profile.json

# When a pairing has already been independently verified:
./build/apps/goldbox_cli --scan-geo /path/to/GEO1.DAX --geo-record 20 \
  --ecl-dax /path/to/ECL1.DAX --ecl-record 4
```

Outputs per-record map dimensions, count of nonzero raw event bytes, count of high-bit flags, trailing byte count, and—if provided—paired ECL ID explicitly annotated **entry addresses unresolved**. The paths, record IDs, and game variants above are *illustrative*, not validated source mappings.

## Tests and limits

- `tests/geo_test.cpp` synthesizes a complete 1026-byte 16×16 record and a separately encoded DAX archive; checks wall nibble order, four planes, every corner, high-bit semantics, out-of-range coordinates, trailing data, and truncations.
- Verifies GEO extraction from DAX, explicit ECL pairing, failure on missing ECL record, and failure when an ECL record's compressed bytes are invalid.
- No SSI assets in test fixtures. The tests enforce `assert` even in Release builds.
- Record layouts, variant trailers, doorway semantics, event dispatch addresses, and game compatibility remain unverified until local game-file checks.

## Next gates

The `ecl_dispatch.hpp` API decodes the five lifecycle instructions at known
boundaries and follows GOTO, WRITE_MEM and AND routing to a typed ON_GOTO or
ON_GOSUB table. It checks every table address and reports state-dependent,
unsupported, malformed or cyclic routes. `ResolvedGeoEvent` preserves GEO
coordinates, raw selector, paired ECL ID, lifecycle kind, dialect, table offset
and target offset. Offsets exclude an optional 5000 record prefix. The full raw
square-info byte is used until an actual AND masks it; high-bit semantics are
not assumed. `geo_event_id` namespaces coordinates by game and archive identity.

The CLI's explicit GEO/ECL pairing additionally reports how many **move** routes
resolve when a profile supplies `eventDispatch`. No record pairing is inferred.

1. Test against the user's local Pool of Radiance and Curse of the Azure Bonds archives, without committing source data.
2. Compare actual cells with independently viewed maps and discover which platform/title variants diverge from this fixed layout.
3. Identify confirmed ECL load-area and dispatch records at known instruction boundaries, then create a versioned event table.
4. Normalize map references for mods and exploration, preserving `(archive, record ID, row, column, ECL selector, origin)`.
