#!/usr/bin/env python3
"""Import a curated, local-only survival subset from the existing TEXTFILES archive."""

from __future__ import annotations

import csv
import hashlib
import json
import os
import textwrap
from datetime import date
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "library" / "textfiles-survival-sources.tsv"
CATALOG = ROOT / "library" / "catalog.tsv"
OUTPUT = ROOT / "library" / "readers" / "textfiles"
PROVENANCE = ROOT / "library" / "textfiles-survival-provenance.tsv"


def archive_candidates() -> list[Path]:
    configured = os.environ.get("WAYKEEPER_TEXTFILES_ARCHIVE")
    candidates = [Path(configured)] if configured else []
    candidates += [
        ROOT.parent / "Counter-Intel" / "output" / "sdcard" / "COUNTER" / "TEXTFILES",
        Path.home() / "Documents" / "HRF-H2" / "DEPLOY_TO_H2" /
        "COUNTER_INTEL_v2.4.0s_NOT_HARDWARE_TESTED" / "SDCARD" / "COUNTER" / "TEXTFILES",
    ]
    return candidates


def find_archive() -> Path:
    for candidate in archive_candidates():
        if (candidate / "FILES").is_dir() and (candidate / "INDEX" / "INDEX_META.JSON").is_file():
            return candidate
    raise SystemExit("TEXTFILES archive not found; set WAYKEEPER_TEXTFILES_ARCHIVE")


def load_tsv(path: Path) -> list[dict[str, str]]:
    with path.open(encoding="utf-8", newline="") as stream:
        return list(csv.DictReader(stream, delimiter="\t"))


def paginate(text: str) -> tuple[str, int]:
    lines: list[str] = []
    for raw in text.replace("\r\n", "\n").replace("\r", "\n").replace("\x00", "").split("\n"):
        if not raw.strip():
            lines.append("")
        else:
            lines.extend(textwrap.wrap(raw, width=96, replace_whitespace=False,
                                       drop_whitespace=True, break_long_words=True,
                                       break_on_hyphens=False) or [""])
    pages = ["\n".join(lines[index:index + 50]).rstrip() + "\n"
             for index in range(0, len(lines), 50)] or ["\n"]
    return "\f".join(pages), len(pages)


def write_tsv(path: Path, fields: list[str], rows: list[dict[str, object]]) -> None:
    with path.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields, delimiter="\t", lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


def main() -> int:
    archive = find_archive()
    rows = load_tsv(MANIFEST)
    OUTPUT.mkdir(parents=True, exist_ok=True)
    catalog_additions = []
    provenance = []
    for row in rows:
        source = archive / "FILES" / row["relative_path"]
        raw = source.read_bytes()
        decoded = raw.decode("cp437", errors="replace")
        warning = "\n".join([
            "WAYKEEPER // TEXTFILES UNDERGROUND ARCHIVE READER",
            "=" * 72,
            f"TITLE: {row['title']}",
            f"ORIGINAL PATH: {row['relative_path']}",
            "SOURCE CLASS: UNDERGROUND / HISTORICAL / UNVERIFIED",
            "RIGHTS: LOCAL PERSONAL ARCHIVE ONLY / REDISTRIBUTION NOT CLEARED",
            "WARNING: OLD TEXT CAN BE WRONG, UNSAFE, ILLEGAL, OR OFFENSIVE.",
            row["note"],
            "=" * 72,
            "",
        ])
        formatted, pages = paginate(warning + decoded)
        destination = OUTPUT / f"{row['id']}.txt"
        destination.write_text(formatted, encoding="utf-8")
        digest = hashlib.sha256(destination.read_bytes()).hexdigest()
        relative = destination.relative_to(ROOT / "library").as_posix()
        catalog_additions.append({"id": row["id"], "title": row["title"],
            "category": row["category"], "pages": pages, "pdf": relative, "text": relative})
        provenance.append({"id": row["id"], "sha256": digest, "pages": pages,
            "acquired_on": date.today().isoformat(),
            "source_url": "http://textfiles.com/" + row["relative_path"],
            "local_archive_path": str(source), "rights_status": "LOCAL PERSONAL ARCHIVE / RIGHTS UNCLEARED",
            "note": row["note"]})
        print(f"Imported {row['title']}: {pages} reader pages")

    archive_meta = json.loads((archive / "INDEX" / "INDEX_META.JSON").read_text(encoding="utf-8"))
    archive_id = "textfiles-complete-local-archive"
    archive_text = "\n".join([
        "WAYKEEPER // COMPLETE TEXTFILES.COM LOCAL ARCHIVE",
        "=" * 72,
        "SOURCE CLASS: UNDERGROUND / HISTORICAL / UNVERIFIED",
        "RIGHTS: LOCAL PERSONAL ARCHIVE ONLY / REDISTRIBUTION NOT CLEARED",
        f"FILES: {archive_meta.get('files', 0):,}",
        f"SOURCE BYTES: {archive_meta.get('source_bytes', 0):,}",
        f"INDEX BYTES: {archive_meta.get('index_bytes', 0):,}",
        f"LOCAL ROOT: {archive}",
        "",
        "The complete archive is already installed beside WAYKEEPER's source tree and remains",
        "available through its COUNTER-SVX-1 search index. The F9 Underground branch imports",
        "only the survival-relevant selection into the main evidence reader, preventing 33,000",
        "unreviewed texts from contaminating emergency search results.",
        "",
        "WARNING: THIS ARCHIVE CONTAINS MATERIAL THAT MAY BE FALSE, ILLEGAL, DANGEROUS,",
        "OFFENSIVE, MALICIOUS, OR COPYRIGHTED. PRESERVATION IS NOT ENDORSEMENT.",
    ])
    formatted, pages = paginate(archive_text)
    destination = OUTPUT / f"{archive_id}.txt"
    destination.write_text(formatted, encoding="utf-8")
    digest = hashlib.sha256(destination.read_bytes()).hexdigest()
    relative = destination.relative_to(ROOT / "library").as_posix()
    catalog_additions.append({"id": archive_id, "title": "Complete TEXTFILES.COM Local Archive Index",
        "category": "Underground-TEXTFILES-Archive", "pages": pages, "pdf": relative, "text": relative})
    provenance.append({"id": archive_id, "sha256": digest, "pages": pages,
        "acquired_on": date.today().isoformat(), "source_url": "http://textfiles.com/directory.html",
        "local_archive_path": str(archive), "rights_status": "LOCAL PERSONAL ARCHIVE / RIGHTS UNCLEARED",
        "note": "Pointer to the complete 33,009-file archive and index; only curated sources enter evidence search."})

    ids = {row["id"] for row in rows} | {archive_id}
    catalog = [row for row in load_tsv(CATALOG) if row["id"] not in ids]
    write_tsv(CATALOG, ["id", "title", "category", "pages", "pdf", "text"], catalog + catalog_additions)
    write_tsv(PROVENANCE, ["id", "sha256", "pages", "acquired_on", "source_url",
        "local_archive_path", "rights_status", "note"], provenance)
    print(f"TEXTFILES library ready: {len(rows)} curated readers plus complete archive index / {archive}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
