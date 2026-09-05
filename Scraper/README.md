# TXTFILES.COM Offline Scraper

This directory contains a resumable, standard-library-only Python scraper for
the `.txt` files linked from [TEXTFILES.COM's directory](http://textfiles.com/directory.html).

## Run

```bash
python3 scrape_textfiles.py
```

The default run discovers the live directory tree, saves its HTML directory
pages, downloads every `.txt` file, and creates sorted indexes.

To refresh only the downloads from the last discovery:

```bash
python3 scrape_textfiles.py --mode download
```

To test with a limited number of downloads:

```bash
python3 scrape_textfiles.py --mode download --limit 100
```

## Output

- `offline_site/` — offline directory pages and `.txt` files, preserving the
  source hierarchy.
- `metadata/discovered_urls.txt` — sorted source URLs.
- `metadata/manifest.csv` — path, byte size, SHA-256, status, and source URL.
- `metadata/SORTED_BY_PATH.txt` — downloaded files sorted by full path.
- `metadata/SORTED_BY_NAME.txt` — downloaded files sorted by filename.
- `metadata/SORTED_BY_SIZE.tsv` — downloaded files sorted largest first.
- `metadata/SORTED_FILES.html` — clickable browser view sorted by full path.
- `metadata/*_errors.tsv` — retryable discovery or download failures.
- `metadata/run_summary.json` — counts, bytes, errors, and elapsed time.

For local browsing, run this command from `Scraper/` and open the displayed URL
ending in `/offline_site/directory.html`:

```bash
python3 -m http.server 8000
```

The archive includes historical material that may be offensive, unsafe, or
incorrect. Treat it as preservation material, not instructions. The source
site's copyright and disclaimer terms still apply.

## Future PDF archive

The sorted indexes and `manifest.csv` provide stable input for a later PDF
conversion pass. Converting should be done as many bounded PDFs (for example,
one PDF per top-level category with a table of contents), rather than one huge
PDF, so the result remains searchable and manageable.
