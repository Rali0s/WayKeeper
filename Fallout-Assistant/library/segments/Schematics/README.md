# WayKeeper Schematics Segment

This directory is a self-contained ingestion segment for the Information Unlimited / Amazing1 preservation archive.

## Contents

- `segment.json` — machine-readable segment manifest
- `catalog.tsv` — 64 normalized schematic-related records
- `sources.tsv` — deduplicated provenance and external-source table
- `INDEX.md` — human-readable collection index
- `LOW-ENERGY-CANDIDATES.md` — restricted entries suitable only for clean-sheet, low-energy educational redesign
- `records/` — one offline-readable Markdown card per record
- `txt/catalog.tsv` — the small allow-list presented by the ANSI Schematics tab
- `txt/*.txt` — fixed-cell diagrams that can be rendered locally without reflow
- `checksums.sha256` — integrity hashes for every generated artifact

The 19 linked/traced records point to patents, a period article, an optics worksheet locator, or surviving file traces. The 45 restricted records preserve catalog identity and provenance only. Four are separately marked as possible clean-sheet, low-energy educational redesigns; their original covert circuits remain restricted. No operational EMP/HERF, weapon, incapacitation, covert-surveillance, dangerous pulsed-power, coil-launcher, or Class IV laser circuit is stored here.

The interactive Schematics tab does not enumerate the 64 historical metadata
cards. It loads only `txt/catalog.tsv`, so an item appears in the viewer only
when a local fixed-width text card actually exists. The current allow-list has
seven items: three WayKeeper field manuals (operator, UART Scout, and modular
self-repair) plus power-domain, console-stack, boot-flow, and radio-service
architecture maps. The maps are not hazardous construction plans, and the
self-repair card defers battery, charger, unknown-power, and board-level damage
to controlled module replacement or trained service.

Rebuild from the site catalog with:

```sh
node scripts/build_amazing1_schematics_segment.mjs
```
