# Philosophy offline reader pack

The `F9 / SOCIETY / PHILOSOPHY` tree contains 197 reader entries arranged as
ten shelves. These are plain-text reader adaptations, not survival cards and
not generated PDFs.

The current verified build contains 125 public-domain full-text readers, 27
license-required bibliographic records, and 45 source-review records. All 197
entries are present in the menu; a review record never masquerades as a full
book.

| Shelf | Entries |
|---|---:|
| Ethics + the Good Life | 18 |
| Government + Power | 41 |
| War + Peace | 13 |
| Logic + Knowledge | 23 |
| Mind + Human Nature | 12 |
| Nature + Science | 13 |
| Meaning + Solitude | 13 |
| World Traditions | 21 |
| Philosophical Literature | 26 |
| Modern / License Required | 17 |

Run the idempotent importer with:

```sh
scripts/download_philosophy_library.py
```

Use `--reformat` to rebuild pagination from cached acquisition state without
performing network searches, or `--only <manifest-id>` to rebuild one entry.

The importer accepts a Project Gutenberg result only when its title and author
match the manifest and its API record identifies it as public domain. It stores
the full plain text, wraps it for the ANSI reader, inserts page boundaries, and
records a SHA-256 checksum and resolved edition metadata. Ambiguous or missing
results become `SOURCE REVIEW REQUIRED` acquisition records; no guessed
substitute is installed.

Modern copyrighted works remain visible in their recommended shelf as
`FULL TEXT NOT INCLUDED / LICENSE REQUIRED` bibliographic reader entries. Add a
lawfully acquired local edition before enabling its full text. These records do
not contain excerpts or substitute summaries.

For outreach and acquisition tracking, see `PHILOSOPHY-NOT-INCLUDED.md`. It
lists every entry without full text and separates definite license needs from
unresolved edition or translation review.

The source manifest is `philosophy-sources.tsv`; acquisition results and
checksums are in `philosophy-provenance.tsv`. Imported reader files live in
`readers/philosophy/` and are merged into the main catalog without changing the
existing PDF collection.
