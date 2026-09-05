#!/usr/bin/env python3
import sqlite3
import sys
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parent.parent
DATABASE = PROJECT_ROOT / "library" / "plants-herbs" / "plants-herbs.sqlite3"


def main() -> None:
    if len(sys.argv) < 2:
        raise SystemExit("Usage: search_plants_herbs_db.py <terms>")
    terms = " ".join(sys.argv[1:]).replace('"', ' ')
    query = ' AND '.join(f'"{term}"' for term in terms.split() if term)
    connection = sqlite3.connect(DATABASE)
    rows = connection.execute(
        """
        SELECT d.title, f.page_number,
               snippet(document_page_fts, 2, '[', ']', ' ... ', 18)
        FROM document_page_fts AS f
        JOIN source_document AS d ON d.id = f.document_id
        WHERE document_page_fts MATCH ?
        ORDER BY bm25(document_page_fts)
        LIMIT 12
        """,
        (query,),
    ).fetchall()
    connection.close()
    for title, page, excerpt in rows:
        print(f"{title} / page {page}\n  {' '.join(excerpt.split())}\n")


if __name__ == "__main__":
    main()

