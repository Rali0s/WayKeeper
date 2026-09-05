#!/usr/bin/env python3
import csv
import hashlib
import sqlite3
from datetime import date
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parent.parent
COLLECTION_ROOT = PROJECT_ROOT / "library" / "plants-herbs"
DATABASE = COLLECTION_ROOT / "plants-herbs.sqlite3"


def read_tsv(path: Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8", newline="") as stream:
        return list(csv.DictReader(stream, delimiter="\t"))


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def pages_from_text(path: Path) -> list[str]:
    pages = path.read_text(encoding="utf-8", errors="replace").split("\f")
    # Poppler terminates the document with one form feed. Remove only the
    # synthetic element after it; a genuinely blank final PDF page must remain
    # so every stored page number still matches the source PDF.
    if pages and pages[-1] == "":
        pages.pop()
    return pages


def main() -> None:
    sources = read_tsv(COLLECTION_ROOT / "sources.tsv")
    checksums = {row["id"]: row for row in read_tsv(COLLECTION_ROOT / "checksums.tsv")}
    connection = sqlite3.connect(DATABASE)
    connection.execute("PRAGMA foreign_keys = ON")
    connection.executescript((COLLECTION_ROOT / "schema.sql").read_text(encoding="utf-8"))

    for source in sources:
        pdf_path = COLLECTION_ROOT / "pdfs" / source["filename"]
        text_path = COLLECTION_ROOT / "text" / f"{source['id']}.txt"
        if not pdf_path.is_file() or not text_path.is_file():
            raise FileNotFoundError(f"Missing downloaded source for {source['id']}")
        pages = pages_from_text(text_path)
        recorded = checksums[source["id"]]
        actual_hash = sha256(pdf_path)
        if actual_hash != recorded["sha256"]:
            raise ValueError(f"Checksum mismatch for {source['id']}")
        if len(pages) != int(recorded["pages"]):
            raise ValueError(
                f"Page extraction mismatch for {source['id']}: "
                f"{len(pages)} text pages vs {recorded['pages']} PDF pages"
            )

        connection.execute(
            """
            INSERT INTO source_document(
                id, title, publisher, publication_year, trust_tier, category, scope,
                source_url, license_note, safety_role, local_pdf, local_text, sha256,
                page_count, accessed_on
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            ON CONFLICT(id) DO UPDATE SET
                title=excluded.title, publisher=excluded.publisher,
                publication_year=excluded.publication_year, trust_tier=excluded.trust_tier,
                category=excluded.category, scope=excluded.scope,
                source_url=excluded.source_url, license_note=excluded.license_note,
                safety_role=excluded.safety_role, local_pdf=excluded.local_pdf,
                local_text=excluded.local_text, sha256=excluded.sha256,
                page_count=excluded.page_count, accessed_on=excluded.accessed_on
            """,
            (
                source["id"], source["title"], source["publisher"], int(source["year"]),
                int(source["trust_tier"]), source["category"], source["scope"],
                source["source_url"], source["license_note"], source["safety_role"],
                str(pdf_path.relative_to(PROJECT_ROOT)), str(text_path.relative_to(PROJECT_ROOT)),
                actual_hash, len(pages), recorded.get("downloaded_on") or date.today().isoformat(),
            ),
        )
        connection.execute("DELETE FROM document_page WHERE document_id = ?", (source["id"],))
        connection.execute("DELETE FROM document_page_fts WHERE document_id = ?", (source["id"],))
        page_rows = [(source["id"], number, text) for number, text in enumerate(pages, 1)]
        connection.executemany(
            "INSERT INTO document_page(document_id, page_number, page_text) VALUES (?, ?, ?)",
            page_rows,
        )
        connection.executemany(
            "INSERT INTO document_page_fts(document_id, page_number, page_text) VALUES (?, ?, ?)",
            page_rows,
        )

    connection.commit()
    document_count, page_count = connection.execute(
        "SELECT COUNT(*), COALESCE(SUM(page_count), 0) FROM source_document"
    ).fetchone()
    connection.close()
    print(f"Plant and herb database ready: {DATABASE}")
    print(f"Indexed {document_count} documents / {page_count} pages")


if __name__ == "__main__":
    main()
