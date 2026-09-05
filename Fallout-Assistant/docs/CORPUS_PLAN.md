# Offline Corpus Plan

## Known local source roots

| Corpus | Local root | Phase-A treatment |
|---|---|---|
| Survival library | `../Survival-Library` | Ingest reviewed/open documents with checksums |
| SERE/manual material | `../RES` | Inventory provenance and license before ingestion |
| TEXTFILES mirror | `../Scraper/offline_site` | Index selected ham-radio and technical paths, not the entire mirror blindly |
| PortaPack/Mayhem | `../mayhem-firmware` | Index documentation and schematics with GPL/source metadata |
| PortaPack/Mayhem v2.4 | `../mayhem-firmware-v2.4.0-sivra` | Treat as a versioned second source; deduplicate by hash |
| TPMS Sonar | `../TPMS Sonar` | Index as concept documentation and label experimental |
| Positive psychology | `../../DISM/Postive Psychology` | Inventory only; ingest solely verified legal/open works |
| Plants and herbs | `library/plants-herbs` | Safety-ranked WHO/USDA PDFs, page FTS, and structured botanical/medical review schema |
| Fieldcraft | `library/pdfs/fieldcraft` | 20 checksum-verified agency/extension PDFs with acquisition manifest and law/safety/shelter-construction gates |
| Society | `library/pdfs/society` | 6 public-domain, official, institutional, or declassified PDFs with belief/civics/FOIA provenance gates |

No distinct “schematic sources” site clone was identified during the initial Documents scan. Its future path belongs in `config/source-roots.example.txt`; content remains quarantined until provenance is known.

The initial hardware drawings are already populated in `config/schematic-roots.tsv`. It registers PortaPack H1/H4M, HackRF One, and Opera Cake drawings from the local Mayhem tree. The second Mayhem tree contains many duplicates and is retained as a versioned source; ingestion must compare hashes and revisions before choosing the authoritative copy.

The Information Unlimited / Amazing1 preservation export is registered as the
`library/segments/Schematics/` segment. Its local Markdown records and TSV
catalog are offline-readable. External patent, periodical, and archive URLs are
provenance links rather than redistributed copies. Restricted historical product
records contain non-operational metadata only.

## Schematic ingestion

Register KiCad `.kicad_sch`, legacy `.sch`, PDFs, datasheets, READMEs, BOMs, and license files. For each design retain:

- project/device and hardware revision;
- exact local source and upstream URL;
- file hash and repository commit/tag;
- license and redistribution status;
- voltage domains, connector names, and warnings;
- whether the drawing is authoritative, community-derived, or unverified.

Rendered schematic images are navigation aids only. The original design file is the authority.

## Electronics collection

Prefer official and redistributable material: government training manuals, manufacturer datasheets/application notes, openly licensed textbooks, and project documentation. Keep commercial works such as current ARRL books as catalog entries or user-owned references unless redistribution and model-indexing rights are clear.

## Index record

Every page/chunk stores `document_id`, title, edition/revision, page, section, source URL, local hash, license status, trust tier, ingestion date, OCR confidence, and text. Safety cards additionally store reviewer and next-review date.

Trust tiers:

1. Current official label/regulator/manufacturer instruction.
2. Current government, standards body, or peer-reviewed/open textbook.
3. Established community project documentation.
4. Unverified mirror, forum, or unknown clone—searchable only when explicitly enabled and visibly marked.
