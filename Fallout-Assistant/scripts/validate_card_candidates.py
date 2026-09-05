#!/usr/bin/env python3
"""Validate WayKeeper candidate-card structure and local PDF provenance."""

from __future__ import annotations

import hashlib
import re
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CANDIDATE_ROOT = ROOT / "cards" / "candidates"
REQUIRED = {
    "schema",
    "id",
    "title",
    "status",
    "reviewed",
    "risk",
    "trust",
    "source_doc_id",
    "source_pdf",
    "source_pages",
    "source_sha256",
    "source_published",
    "cross_check_url",
    "cross_checked",
    "tags",
    "aliases",
}
ID_PATTERN = re.compile(r"^[a-z0-9]+(?:[.-][a-z0-9]+)*$")
SHA_PATTERN = re.compile(r"^[0-9a-f]{64}$")
PAGE_PATTERN = re.compile(r"^\d+(?:-\d+)?(?:,\s*\d+(?:-\d+)?)*$")


def parse_card(path: Path) -> tuple[dict[str, str], str]:
    text = path.read_text(encoding="utf-8")
    if not text.startswith("---\n"):
        raise ValueError("missing opening front-matter delimiter")
    end = text.find("\n---\n", 4)
    if end < 0:
        raise ValueError("missing closing front-matter delimiter")
    metadata: dict[str, str] = {}
    for line in text[4:end].splitlines():
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        key, separator, value = line.partition(":")
        if not separator:
            raise ValueError(f"invalid metadata line: {line!r}")
        metadata[key.strip()] = value.strip()
    return metadata, text[end + 5 :]


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def pdf_page_count(path: Path) -> int:
    result = subprocess.run(
        ["pdfinfo", str(path)], check=True, capture_output=True, text=True
    )
    match = re.search(r"^Pages:\s+(\d+)\s*$", result.stdout, re.MULTILINE)
    if not match:
        raise ValueError("pdfinfo did not report a page count")
    return int(match.group(1))


def referenced_pages(value: str) -> set[int]:
    pages: set[int] = set()
    for item in value.split(","):
        bounds = [int(part) for part in item.strip().split("-")]
        if len(bounds) == 1:
            pages.add(bounds[0])
        elif len(bounds) == 2 and bounds[0] <= bounds[1]:
            pages.update(range(bounds[0], bounds[1] + 1))
        else:
            raise ValueError(f"invalid page expression: {item!r}")
    return pages


def validate(path: Path, seen_ids: set[str]) -> list[str]:
    errors: list[str] = []
    try:
        metadata, body = parse_card(path)
    except (OSError, ValueError) as exc:
        return [str(exc)]

    missing = sorted(REQUIRED - metadata.keys())
    if missing:
        errors.append("missing metadata: " + ", ".join(missing))
        return errors

    card_id = metadata["id"]
    if not ID_PATTERN.fullmatch(card_id):
        errors.append("invalid id")
    if card_id in seen_ids:
        errors.append("duplicate id")
    seen_ids.add(card_id)

    if metadata["schema"] != "waykeeper-card-v1":
        errors.append("unsupported schema")
    if metadata["status"] not in {"candidate", "reviewed", "rejected"}:
        errors.append("invalid status")
    if metadata["status"] == "candidate" and metadata["reviewed"] != "null":
        errors.append("candidate must have reviewed: null")
    if metadata["risk"] not in {"low", "moderate", "high"}:
        errors.append("invalid risk")
    if not SHA_PATTERN.fullmatch(metadata["source_sha256"]):
        errors.append("invalid SHA-256")
    if not PAGE_PATTERN.fullmatch(metadata["source_pages"]):
        errors.append("invalid source_pages")

    source = (ROOT / metadata["source_pdf"]).resolve()
    try:
        source.relative_to(ROOT)
    except ValueError:
        errors.append("source_pdf escapes repository")
        source = ROOT / "__invalid__"
    if not source.is_file():
        errors.append("source PDF does not exist")
    elif source.suffix.lower() != ".pdf":
        errors.append("source is not a PDF")
    else:
        if sha256(source) != metadata["source_sha256"]:
            errors.append("source SHA-256 mismatch")
        try:
            pages = referenced_pages(metadata["source_pages"])
            page_count = pdf_page_count(source)
            if not pages or min(pages) < 1 or max(pages) > page_count:
                errors.append(f"source page outside 1-{page_count}")
        except (OSError, ValueError, subprocess.SubprocessError) as exc:
            errors.append(f"could not validate source pages: {exc}")

    if not metadata["cross_check_url"].startswith("https://"):
        errors.append("cross_check_url must use HTTPS")
    if not re.fullmatch(r"\d{4}-\d{2}-\d{2}", metadata["cross_checked"]):
        errors.append("cross_checked must be YYYY-MM-DD")
    if not re.search(r"^# .+", body, re.MULTILINE):
        errors.append("missing card title heading")
    if "\n## LIMITS\n" not in body:
        errors.append("missing LIMITS section")
    if "\n## SOURCE NOTE\n" not in body:
        errors.append("missing SOURCE NOTE section")
    if any(len(line) > 100 for line in body.splitlines()):
        errors.append("body contains a line longer than 100 characters")
    return errors


def main() -> int:
    paths = sorted(CANDIDATE_ROOT.glob("**/*.md"))
    if not paths:
        print(f"No candidate cards found under {CANDIDATE_ROOT}", file=sys.stderr)
        return 1

    failures = 0
    seen_ids: set[str] = set()
    for path in paths:
        errors = validate(path, seen_ids)
        relative = path.relative_to(ROOT)
        if errors:
            failures += 1
            for error in errors:
                print(f"FAIL {relative}: {error}")
        else:
            print(f"PASS {relative}")
    print(f"Checked {len(paths)} candidate cards; {failures} failed.")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
