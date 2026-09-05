# Plants and Herbs Evidence Database

This collection is designed for offline retrieval, botanical cross-referencing,
and future reviewed survival cards. It is not a self-treatment database.

Current build: 10 verified source PDFs, 2,266 page-aligned text records, and a
full-text SQLite index. The structured plant and evidence tables intentionally
start empty until monograph extraction and human review are performed.

The initial corpus contains WHO monographs and herbal-safety/quality guidance,
the USDA Forest Service guide to Appalachian medicinal plants, and current
Upstate New York Poison Center material. Source PDFs, page-separated text,
checksums, and provenance are retained together. The SQLite schema separates
plant identity, names, parts, claims, evidence level, hazards, interactions,
lookalikes, and geographic presence.

Safety defaults:

- imported text is evidence to review, not an instruction;
- all plant and claim records default to `emergency_use_allowed = 0`;
- traditional use is never silently promoted to clinical evidence;
- identification requires botanical review and toxic-lookalike data;
- doses require source-page citation and medical review;
- pregnancy, pediatric use, drug interactions, allergies, toxicity, and
  contamination are first-class hazard records;
- uncertainty must stop a recommendation rather than invite experimentation.

Build or refresh the collection:

```sh
scripts/download_plants_herbs_library.sh
```

Search the page-level offline index:

```sh
scripts/search_plants_herbs_db.py "willow contraindications"
```

The next ingestion stage should extract monograph headings into structured plant
records with reviewer checkpoints. Automated extraction may propose records, but
must not set `emergency_use_allowed`.
