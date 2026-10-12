# Original-game importers (not implemented)

The only currently functional importer is `SyntheticJsonImporter` in `engine/src/content.cpp`.
It exclusively parses invented JSON fixture content and deliberately rejects original-game identifiers.

Next: independent original SSI file-format readers that convert user-supplied data into `ImportedContent` and future map/event/encounter records. Keep parsers separated by format/version, with tests that do not distribute copyrighted materials.
