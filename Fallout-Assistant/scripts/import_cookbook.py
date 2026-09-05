#!/usr/bin/env python3
"""Build a gated local-only text index for the quarantined Cookbook archive."""

from __future__ import annotations

import csv
import hashlib
import subprocess
from datetime import date
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CATALOG = ROOT / "library" / "catalog.tsv"
OUTPUT = ROOT / "library" / "readers" / "cookbook"
PROVENANCE = ROOT / "library" / "cookbook-provenance.tsv"
DOCUMENT_ID = "anarchists-cookbook-iv-4-14-quarantine"


def find_source() -> Path:
    candidates = list((ROOT.parent / "RES").glob("*Anarchist*Cookbook*.pdf"))
    candidates += list((ROOT / "RES").glob("*Anarchist*Cookbook*.pdf"))
    if not candidates:
        raise FileNotFoundError("No RES/*Anarchist*Cookbook*.pdf source was found")
    return sorted(candidates)[0]


def pdf_pages(source: Path) -> int:
    result = subprocess.check_output(["pdfinfo", str(source)], text=True, errors="replace")
    for line in result.splitlines():
        if line.startswith("Pages:"):
            return int(line.split(":", 1)[1].strip())
    raise RuntimeError("pdfinfo did not report a page count")


def load_catalog() -> list[dict[str, str]]:
    with CATALOG.open(encoding="utf-8", newline="") as stream:
        return list(csv.DictReader(stream, delimiter="\t"))


def write_tsv(path: Path, fields: list[str], rows: list[dict[str, object]]) -> None:
    with path.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields, delimiter="\t", lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


def main() -> int:
    source = find_source().resolve()
    pages = pdf_pages(source)
    OUTPUT.mkdir(parents=True, exist_ok=True)
    reader = OUTPUT / f"{DOCUMENT_ID}.txt"
    extracted = subprocess.run(
        ["pdftotext", "-layout", str(source), "-"],
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    ).stdout.decode("utf-8", errors="replace")
    extracted_pages = extracted.split("\f")
    if extracted_pages and not extracted_pages[-1].strip():
        extracted_pages.pop()
    page_texts = []
    for page in range(1, pages + 1):
        source_text = extracted_pages[page - 1].strip() if page <= len(extracted_pages) else ""
        page_texts.append("\n".join([
            "WAYKEEPER // COOKBOOK RESTRICTED ARCHIVE",
            "=" * 72,
            "AGE RESTRICTION: 18+ ONLY",
            "SOURCE CLASS: UNDERGROUND / HISTORICAL / UNVERIFIED",
            "RIGHTS: LOCAL PERSONAL ARCHIVE / REDISTRIBUTION NOT CLEARED",
            "ILLEGAL CONTENT - FOR REFERENCE PURPOSES ONLY - HISTORIC",
            "INDEX: LOCAL RESTRICTED SEARCH ONLY / EXCLUDED FROM GUIDE + DEEPSEARCH",
            "WARNING: INCLUDES FRAUD, INTRUSION, WEAPONS, EXPLOSIVES, DRUGS,",
            "VIOLENCE, AND OTHER ILLEGAL OR POTENTIALLY LETHAL MATERIAL.",
            f"SOURCE PAGE: {page} / {pages}",
            "",
            "Preservation is not endorsement, validation, instruction, or survival advice.",
            "",
            source_text,
        ]))
    reader.write_text("\f".join(page_texts), encoding="utf-8")

    relative_pdf = Path("../../") / source.relative_to(ROOT.parent)
    relative_reader = reader.relative_to(ROOT / "library")
    rows = [row for row in load_catalog() if row["id"] != DOCUMENT_ID]
    rows.append({
        "id": DOCUMENT_ID,
        "title": "Anarchist's Cookbook IV 4.14 - Restricted Archive",
        "category": "Cookbook-Underground-Restricted",
        "pages": str(pages),
        "pdf": relative_pdf.as_posix(),
        "text": relative_reader.as_posix(),
    })
    write_tsv(CATALOG, ["id", "title", "category", "pages", "pdf", "text"], rows)

    digest = hashlib.sha256(source.read_bytes()).hexdigest()
    write_tsv(PROVENANCE, [
        "id", "sha256", "pages", "registered_on", "local_source_path",
        "rights_status", "index_policy", "note",
    ], [{
        "id": DOCUMENT_ID,
        "sha256": digest,
        "pages": pages,
        "registered_on": date.today().isoformat(),
        "local_source_path": str(source),
        "rights_status": "LOCAL PERSONAL ARCHIVE / AUTHORSHIP AND REDISTRIBUTION RIGHTS UNCLEARED",
        "index_policy": "18+ GATED FULL-TEXT LOCAL INDEX / EXCLUDED FROM GUIDE AND DEEPSEARCH",
        "note": "Illegal content - for reference purposes only - historic. Not reviewed survival guidance.",
    }])
    print(f"Cookbook archive registered: {pages} pages / {source}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
