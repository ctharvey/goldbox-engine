# Example mod: journal enhancer

`mod.json` is a **working** journal-content overlay loaded by the M1 engine.
The example text is invented; no SSI copyrighted prose or artwork is provided.

Try it (run from repository root after building):

```bash
./build/apps/goldbox_cli --content examples/content/demo-campaign/journals.json --mods examples/mods --journal 17 --mode enhanced
```

Switch `--mode classic` to see the unmodified synthetic base entry.
The engine does not yet load original SSI campaigns or run TypeScript.
