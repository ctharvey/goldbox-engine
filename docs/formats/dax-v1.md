# DAX v1 archive reader

The read-only decoder in `formats/src/dax.cpp` recognizes the documented little-endian archive header:

- Bytes 0–1: absolute start of record data, little-endian 16-bit.
- Bytes 2 to data-offset: zero or more 9-byte index records.
- Each record: 1-byte ID; 4-byte little-endian relative data offset; 2-byte unpacked size; 2-byte stored size.
- Record starts at `data_offset + relative_offset`.

Record extraction applies a signed-byte RLE stream. Nonnegative control values copy the next `control + 1` bytes; negative values repeat the next byte `-control` times. Stored records may be larger than the decoded output. An exact unpacked length is required, and trailing/invalid encoded segments are rejected.

### Current safety checks

- Bound archive size to 64 MiB; reject symlinks/non-regular files for file loading.
- Require an index length divisible by nine and index wholly within file.
- Reject duplicate record IDs, index/body boundary violations, out-of-range and overlapping data slices.
- Reject truncated, oversized and inconsistent RLE streams; all sizes are checked prior to allocation.
- Do not write to original files.

### Known limitations

- No SSI file validation yet; tests use generated fictional bytes only.
- Game-specific geometry decoding and ECL are not implemented; some formats may treat payloads differently.
- A few Gold Box titles use GLB, not DAX. The GLB plugin remains unimplemented.
- The synthetic index reader does not prove source compatibility with every game/version. A follow-up must inspect legally obtained installations locally.

Reference: https://forums.goldbox.games/index.php?topic=3148.0
