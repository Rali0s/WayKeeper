# Society offline pack

The nested `F9` Society segment preserves belief, civic, philosophical,
institutional, and declassified historical records as distinct source classes. Run
`scripts/download_society_library.sh` to acquire, generate, extract, checksum,
and merge the six PDFs into the main page-searchable library.

## Collections

| Segment | Offline sources |
|---|---|
| Religion - Christian | King James Version, generated as a searchable PDF from Project Gutenberg public-domain text |
| Religion - Catholic | Complete Douay-Rheims Bible, Challoner revision, generated from Project Gutenberg public-domain text |
| Religion - occult history | Eliphas Levi, *Transcendental Magic: Its Doctrine and Ritual*, A. E. Waite translation, Library of Congress scan at Internet Archive |
| Founding papers | GPO Senate Manual historical-documents section containing the Declaration of Independence, Articles of Confederation, and Constitution |
| Government structure | 2025 *United States Government Manual*, the official organizational handbook of the Federal Government |
| FOIA and declassified records | CIA's 1963 KUBARK manual, preserved through the George Washington University National Security Archive |
| Philosophy | 197 rights-aware plain-text reader entries across ten shelves; see `PHILOSOPHY.md` |
| Survival economy | 3 open NIST references covering fair weights and measures, pricing/method-of-sale rules, and community resilience economic decisions, plus an expressly reusable Federal Reserve barter/money curriculum |
| Rebuilding society | 6 open FEMA/NIST references covering recovery doctrine, whole-community planning, lifelines, continuity, and long-term resilience planning |
| TEXTFILES underground archive | 21 curated local-only survival/communications/repair/fermentation readers plus an index pointer to the complete 33,009-file archive; see `TEXTFILES.md` |

## Corrections and scope

- The King James Version is historically Protestant and does not contain the
  modern Catholic canon. The Douay-Rheims is the separate traditional English
  Catholic edition in this corpus.
- Eliphas Levi wrote *Transcendental Magic: Its Doctrine and Ritual*. Anton
  LaVey wrote the copyrighted *The Satanic Bible*; no unauthorized copy of
  LaVey's book is included.
- Title 10 of the United States Code concerns the Armed Forces. It is not a
  general manual for running a country. WAYKEEPER therefore uses the current
  official U.S. Government Manual for branches, agencies, roles, and programs.
- Government documents are not emergency succession instructions and do not
  independently establish current legal authority. Verify current law.
- KUBARK is retained as evidence of historical coercive doctrine and abuse. It
  is not operational guidance; torture and cruel, inhuman, or degrading
  treatment are prohibited and harmful.

Every reader page carries a class-specific banner. Belief sources retain their
identity without endorsement, civic sources require current-law verification,
FOIA abuse records are visibly separated from operational guidance, and the
TEXTFILES shelf remains marked historical, unverified, and redistribution-rights
uncleared. Alcohol production material is additionally marked for fire,
pressure, toxic-fraction, and current-law hazards.

Acquisition inputs are in `society-sources.tsv`. Verified SHA-256 values, page
counts, acquisition dates, publishers, source URLs, and cautions are in
`society-provenance.tsv`.

The added economy and reconstruction inputs are recorded in
`recovery-economy-sources.tsv`; their checksums, page counts, and source URLs are
in `recovery-economy-provenance.tsv`.
