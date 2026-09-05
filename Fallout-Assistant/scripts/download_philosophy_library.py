#!/usr/bin/env python3
"""Build WAYKEEPER's rights-aware offline philosophy reader library."""

from __future__ import annotations

import argparse
import csv
import difflib
import hashlib
import json
import re
import sys
import textwrap
import time
import unicodedata
import urllib.error
import urllib.parse
import urllib.request
from datetime import date
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "library" / "philosophy-sources.tsv"
CATALOG = ROOT / "library" / "catalog.tsv"
READER_DIR = ROOT / "library" / "readers" / "philosophy"
RAW_DIR = ROOT / "tmp" / "philosophy" / "raw"
META_DIR = ROOT / "tmp" / "philosophy" / "metadata"
PROVENANCE = ROOT / "library" / "philosophy-provenance.tsv"
USER_AGENT = "WAYKEEPER/1.0 (personal offline public-domain library)"
STOP_WORDS = {
    "a", "an", "and", "book", "books", "of", "on", "or", "selected", "the", "to", "works"
}
GUTENBERG_OVERRIDES = {
    # Canonical editions requested by name; pinning avoids search ambiguity.
    "plato-republic": 1497,
    "lucretius-nature": 785,
    "tolstoy-war-peace": 2600,
}


def normalized(value: str) -> str:
    value = unicodedata.normalize("NFKD", value).encode("ascii", "ignore").decode("ascii")
    return re.sub(r"[^a-z0-9]+", " ", value.lower()).strip()


def meaningful_words(value: str) -> set[str]:
    return {word for word in normalized(value).split() if word not in STOP_WORDS and len(word) > 1}


def request_bytes(url: str, retries: int = 3) -> bytes:
    request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    for attempt in range(retries):
        try:
            with urllib.request.urlopen(request, timeout=45) as response:
                return response.read()
        except (urllib.error.URLError, TimeoutError):
            if attempt + 1 == retries:
                raise
            time.sleep(1.5 * (attempt + 1))
    raise RuntimeError("unreachable")


def result_score(row: dict[str, str], result: dict) -> float:
    if result.get("copyright") is not False or "en" not in result.get("languages", []):
        return -1
    expected_title = meaningful_words(row["title"])
    actual_title = meaningful_words(result.get("title", ""))
    overlap = len(expected_title & actual_title) / max(1, len(expected_title))
    expected_author = normalized(row["author"])
    actual_authors = " ".join(author.get("name", "") for author in result.get("authors", []))
    actual_author = normalized(actual_authors)
    generic_author = expected_author in {"anonymous", "multiple authors", "traditional"}
    surnames = [part for part in expected_author.split() if len(part) > 2]
    actual_author_words = actual_author.split()
    author_match = generic_author or any(
        part in actual_author or any(
            difflib.SequenceMatcher(None, part, actual).ratio() >= 0.78
            for actual in actual_author_words
        )
        for part in surnames[-2:]
    )
    if not author_match or overlap < 0.50:
        return -1
    exact_bonus = 4 if normalized(row["title"]) == normalized(result.get("title", "")) else 0
    return overlap * 10 + exact_bonus + min(float(result.get("download_count", 0)) / 1_000_000, 1)


def text_url(result: dict) -> str | None:
    formats = result.get("formats", {})
    preferences = (
        "text/plain; charset=utf-8",
        "text/plain; charset=us-ascii",
        "text/plain",
    )
    for preferred in preferences:
        for mime, url in formats.items():
            if url and mime.lower() == preferred:
                return url
    for mime, url in formats.items():
        if url and mime.lower().startswith("text/plain"):
            return url
    return None


def resolve_gutenberg(row: dict[str, str]) -> tuple[str, str, str, str]:
    if row["id"] in GUTENBERG_OVERRIDES:
        query_urls = [f"https://gutendex.com/books/{GUTENBERG_OVERRIDES[row['id']]}" ]
    else:
        queries = [row["query"], f"{row['title']} {row['author']}", row["title"]]
        query_urls = [
            "https://gutendex.com/books/?" + urllib.parse.urlencode({"search": query})
            for query in dict.fromkeys(queries)
        ]
    results: list[dict] = []
    for query_url in query_urls:
        payload = json.loads(request_bytes(query_url).decode("utf-8"))
        if "results" in payload:
            results.extend(payload.get("results", []))
        else:
            results.append(payload)
        if any(result_score(row, result) >= 0 for result in results):
            break
    ranked = sorted(
        (((100.0 if row["id"] in GUTENBERG_OVERRIDES else result_score(row, result)), result)
         for result in results),
        key=lambda pair: pair[0], reverse=True,
    )
    if not ranked or ranked[0][0] < 0:
        raise LookupError("no sufficiently close public-domain edition")
    result = ranked[0][1]
    download = text_url(result)
    if not download:
        raise LookupError("matched edition has no plain-text download")
    raw = request_bytes(download).decode("utf-8-sig", errors="replace")
    if len(raw.strip()) < 800:
        raise LookupError("downloaded text is unexpectedly short")
    authors = "; ".join(author.get("name", "") for author in result.get("authors", [])) or row["author"]
    source = f"https://www.gutenberg.org/ebooks/{result['id']}"
    return raw, source, result.get("title", row["title"]), authors


def header(row: dict[str, str], source: str, status: str) -> str:
    return "\n".join([
        "WAYKEEPER // OFFLINE READER ADAPTATION",
        "=" * 72,
        f"TITLE: {row['title']}",
        f"AUTHOR: {row['author']}",
        f"SHELF: {row['category']}",
        f"STATUS: {status}",
        f"SOURCE: {source}",
        "",
        "EDITORIAL NOTE",
        row["note"],
        "",
        "This source is presented as a paginated plain-text reader, not a survival card. "
        "Its historical claims and assumptions should be evaluated critically.",
        "=" * 72,
        "",
        "",
    ])


def licensed_entry(row: dict[str, str]) -> str:
    return header(row, "LICENSED COPY NOT YET SUPPLIED", "FULL TEXT NOT INCLUDED") + (
        "RIGHTS GATE\n\n"
        "WAYKEEPER includes this bibliographic reader entry so the recommended Society tree remains "
        "complete. The work itself is not copied here. Add a lawfully acquired, locally owned edition "
        "before enabling full-text reading and search.\n\n"
        f"AUTHOR: {row['author']}\nTITLE: {row['title']}\n"
        f"LIBRARY NOTE: {row['note']}\n\n"
        "This is a rights-and-acquisition record, not a summary, excerpt, or treatment card.\n"
    )


def review_entry(row: dict[str, str], search_url: str, reason: str) -> str:
    return header(row, search_url, "SOURCE REVIEW REQUIRED") + (
        "ACQUISITION RECORD\n\n"
        "An automated search did not identify a sufficiently exact public-domain plain-text edition. "
        "No substitute or uncertain text was downloaded. Review the source search, confirm the title, "
        "author, edition, and rights, then rerun or install the verified text manually.\n\n"
        f"REASON: {reason}\nQUERY: {row['query']}\n"
    )


def paginate(text: str, width: int = 96, lines_per_page: int = 50) -> tuple[str, int]:
    text = text.replace("\r\n", "\n").replace("\r", "\n").replace("\x00", "")
    output: list[str] = []
    for raw_line in text.split("\n"):
        if not raw_line.strip():
            output.append("")
            continue
        indent = raw_line[: len(raw_line) - len(raw_line.lstrip())][:8]
        wrapped = textwrap.wrap(
            raw_line.strip(), width=max(24, width - len(indent)), replace_whitespace=False,
            drop_whitespace=True, break_long_words=True, break_on_hyphens=False,
        )
        output.extend(indent + line for line in (wrapped or [""]))
    while output and not output[-1]:
        output.pop()
    pages = ["\n".join(output[index:index + lines_per_page]).rstrip() + "\n"
             for index in range(0, len(output), lines_per_page)] or ["\n"]
    return "\f".join(pages), len(pages)


def load_rows(path: Path) -> list[dict[str, str]]:
    with path.open(encoding="utf-8", newline="") as stream:
        return list(csv.DictReader(stream, delimiter="\t"))


def write_tsv(path: Path, fieldnames: list[str], rows: list[dict[str, object]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fieldnames, delimiter="\t", lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--refresh", action="store_true", help="redownload already cached Gutenberg sources")
    parser.add_argument("--only", help="rebuild one manifest ID while preserving every other entry")
    parser.add_argument("--reformat", action="store_true", help="rebuild readers from cached source state without network searches")
    args = parser.parse_args()
    manifest_rows = load_rows(MANIFEST)
    manifest_ids = [row["id"] for row in manifest_rows]
    if len(manifest_ids) != len(set(manifest_ids)):
        raise SystemExit("manifest contains duplicate IDs")
    rows = [row for row in manifest_rows if not args.only or row["id"] == args.only]
    if not rows:
        raise SystemExit(f"manifest ID not found: {args.only}")
    ids = [row["id"] for row in rows]
    READER_DIR.mkdir(parents=True, exist_ok=True)
    RAW_DIR.mkdir(parents=True, exist_ok=True)
    META_DIR.mkdir(parents=True, exist_ok=True)

    catalog_rows: list[dict[str, object]] = []
    provenance_rows: list[dict[str, object]] = []
    counts = {"full-text": 0, "license-required": 0, "source-review": 0}
    prior_rows = load_rows(PROVENANCE) if PROVENANCE.exists() else []
    prior_by_id = {row["id"]: row for row in prior_rows}
    for number, row in enumerate(rows, 1):
        source = ""
        resolved_title = row["title"]
        resolved_author = row["author"]
        if row["mode"] == "licensed":
            content = licensed_entry(row)
            status = "license-required"
            rights = "FULL TEXT NOT INCLUDED / LICENSE REQUIRED"
            source = "local bibliographic record"
        else:
            raw_path = RAW_DIR / f"{row['id']}.txt"
            meta_path = META_DIR / f"{row['id']}.json"
            try:
                if args.reformat and not (raw_path.exists() and meta_path.exists()):
                    previous = prior_by_id.get(row["id"], {})
                    source = previous.get("source_url") or (
                        "https://www.gutenberg.org/ebooks/search/?" +
                        urllib.parse.urlencode({"query": row["query"]})
                    )
                    content = review_entry(row, source, "Previously unresolved; source review still required")
                    status = "source-review"
                    rights = "SOURCE REVIEW REQUIRED / NO FULL TEXT INSTALLED"
                    raise StopIteration
                if raw_path.exists() and meta_path.exists() and not args.refresh:
                    raw = raw_path.read_text(encoding="utf-8")
                    metadata = json.loads(meta_path.read_text(encoding="utf-8"))
                    source = metadata["source"]
                    resolved_title = metadata["title"]
                    resolved_author = metadata["author"]
                else:
                    raw, source, resolved_title, resolved_author = resolve_gutenberg(row)
                    raw_path.write_text(raw, encoding="utf-8")
                    meta_path.write_text(json.dumps({
                        "source": source, "title": resolved_title, "author": resolved_author,
                    }, indent=2) + "\n", encoding="utf-8")
                    time.sleep(0.15)
                content = header(row, source, "PUBLIC-DOMAIN FULL TEXT") + raw
                status = "full-text"
                rights = "PROJECT GUTENBERG PUBLIC-DOMAIN EDITION"
            except StopIteration:
                pass
            except Exception as exc:  # preserve the shelf without installing an uncertain source
                source = "https://www.gutenberg.org/ebooks/search/?" + urllib.parse.urlencode({"query": row["query"]})
                content = review_entry(row, source, str(exc))
                status = "source-review"
                rights = "SOURCE REVIEW REQUIRED / NO FULL TEXT INSTALLED"

        paginated, pages = paginate(content)
        output_path = READER_DIR / f"{row['id']}.txt"
        output_path.write_text(paginated, encoding="utf-8")
        digest = hashlib.sha256(output_path.read_bytes()).hexdigest()
        relative = output_path.relative_to(ROOT / "library").as_posix()
        catalog_rows.append({
            "id": row["id"], "title": row["title"], "category": row["category"],
            "pages": pages, "pdf": relative, "text": relative,
        })
        provenance_rows.append({
            "id": row["id"], "sha256": digest, "pages": pages, "acquired_on": date.today().isoformat(),
            "source_url": source, "publisher": "Project Gutenberg" if status == "full-text" else "WAYKEEPER",
            "rights_status": rights, "acquisition_status": status,
            "resolved_title": resolved_title, "resolved_author": resolved_author,
        })
        counts[status] += 1
        print(f"[{number:03}/{len(rows)}] {status:16} {row['title']}", flush=True)

    existing = load_rows(CATALOG)
    philosophy_ids = set(ids)
    existing = [row for row in existing if row["id"] not in philosophy_ids]
    write_tsv(CATALOG, ["id", "title", "category", "pages", "pdf", "text"], existing + catalog_rows)
    prior_provenance = prior_rows
    prior_provenance = [row for row in prior_provenance if row["id"] not in philosophy_ids]
    write_tsv(PROVENANCE, [
        "id", "sha256", "pages", "acquired_on", "source_url", "publisher", "rights_status",
        "acquisition_status", "resolved_title", "resolved_author",
    ], prior_provenance + provenance_rows)
    print("Imported " + ", ".join(f"{value} {key}" for key, value in counts.items()))
    return 0


if __name__ == "__main__":
    sys.exit(main())
