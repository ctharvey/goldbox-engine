# DAX v1 archive reader

The read-only decoder in `formats/src/dax.cpp` recognizes the documented little-endian archive header:

- Bytes 0–1: index byte count excluding the leading word, little-endian 16-bit.
- Record data starts at `2 + index byte count`. Earlier synthetic fixtures
  encoded the absolute offset instead; those remain accepted when the word
  modulo nine is two, rather than zero. The conventions are unambiguous.
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

- All 113 DAX archives (1,245 records) in one authorized local Pool of Radiance
  installation passed index and RLE checks. Public tests remain synthetic.
- GEO and ECL pilots exist; some formats may treat payloads differently.
- A few Gold Box titles use GLB, not DAX. The GLB plugin remains unimplemented.
- This does not prove source compatibility with other games/versions. See
  [local validation scope](../research/local-validation-2026-10-15.md).

Reference: https://forums.goldbox.games/index.php?topic=3148.0

Index and RLE behavior were compared against Gold Box Explorer's `DaxFile.cs`
and SSI Engine's `DAXFile.java` at the revisions in `attribution/sources.json`.
The existing bounds checks and RLE implementation were retained. Uncompressed
records with raw size zero seen in some Explorer paths remain unsupported.
