#!/usr/bin/env python3
"""WAYKEEPER online manual discovery console.

This utility searches metadata and stages links. It intentionally has no bulk
download command: integration into the offline library remains a deliberate,
source-by-source operation with rights and checksum review.
"""

from __future__ import annotations

import argparse
import csv
import json
import os
from pathlib import Path
import sys
import urllib.error
import urllib.parse
import urllib.request

AMBER = "\033[38;5;214m"
GREEN = "\033[38;5;82m"
MUTED = "\033[38;5;245m"
RED = "\033[38;5;196m"
RESET = "\033[0m"

PROVIDERS = {
    "archive": {
        "name": "INTERNET ARCHIVE / MANUALS",
        "mode": "API METADATA SEARCH",
        "scope": "~3.04M collection items; item derivatives and rights vary",
    },
    "ifixit": {
        "name": "IFIXIT",
        "mode": "OFFICIAL API GUIDE SEARCH",
        "scope": "repair guides; CC BY-NC-SA licensing requires attribution and commercial review",
    },
    "manualslib": {
        "name": "MANUALSLIB",
        "mode": "PROVIDER SEARCH HANDOFF",
        "scope": "10,522,811 PDFs / 3.2 TB claimed; no bulk ingest",
    },
    "manualsonline": {
        "name": "MANUALSONLINE",
        "mode": "PROVIDER SEARCH HANDOFF",
        "scope": "700,000+ products claimed; no bulk ingest",
    },
}


def request_json(url: str) -> object:
    request = urllib.request.Request(
        url,
        headers={"User-Agent": "WAYKEEPER-WK01/0.1 manual metadata discovery"},
    )
    with urllib.request.urlopen(request, timeout=15) as response:
        return json.load(response)


def archive_search(query: str, limit: int) -> list[dict[str, str]]:
    expression = f'collection:manuals AND (title:({query}) OR description:({query}))'
    params = urllib.parse.urlencode(
        {
            "q": expression,
            "fl[]": ["identifier", "title", "creator", "date", "description"],
            "rows": limit,
            "page": 1,
            "output": "json",
        },
        doseq=True,
    )
    payload = request_json("https://archive.org/advancedsearch.php?" + params)
    docs = payload.get("response", {}).get("docs", []) if isinstance(payload, dict) else []
    return [
        {
            "provider": "Internet Archive",
            "title": str(doc.get("title") or doc.get("identifier") or "Untitled"),
            "url": "https://archive.org/details/" + str(doc.get("identifier", "")),
            "kind": "item metadata",
            "rights": "VERIFY ITEM RIGHTS + FILE LIST BEFORE FETCH",
        }
        for doc in docs
        if doc.get("identifier")
    ]


def ifixit_search(query: str, limit: int) -> list[dict[str, str]]:
    encoded = urllib.parse.quote(query, safe="")
    payload = request_json(
        f"https://www.ifixit.com/api/2.0/suggest/{encoded}?doctypes=guide"
    )
    results = payload.get("results", []) if isinstance(payload, dict) else []
    return [
        {
            "provider": "iFixit",
            "title": str(item.get("title") or "Untitled guide"),
            "url": str(item.get("url") or "https://www.ifixit.com/Guide"),
            "kind": str(item.get("type") or "guide"),
            "rights": "CC BY-NC-SA / ATTRIBUTION + NONCOMMERCIAL + SHAREALIKE REVIEW",
        }
        for item in results[:limit]
        if isinstance(item, dict)
    ]


def handoff_result(provider: str, query: str) -> list[dict[str, str]]:
    if provider == "manualslib":
        url = "https://www.manualslib.com/index.php?" + urllib.parse.urlencode(
            {"action": "search", "snippet": "1", "keyword": query}
        )
        name = "ManualsLib"
    else:
        url = "https://www.manualsonline.com/"
        name = "ManualsOnline"
    return [{
        "provider": name,
        "title": f'Search provider for: {query}',
        "url": url,
        "kind": "search handoff / no automated scraping",
        "rights": "VERIFY MANUAL COPYRIGHT + PROVIDER TERMS BEFORE FETCH",
    }]


def search(provider: str, query: str, limit: int) -> list[dict[str, str]]:
    if provider == "archive":
        return archive_search(query, limit)
    if provider == "ifixit":
        return ifixit_search(query, limit)
    return handoff_result(provider, query)


def queue_path(root: Path) -> Path:
    return root / "state" / "manual-resource-queue.tsv"


def queue_count(root: Path) -> int:
    path = queue_path(root)
    if not path.exists():
        return 0
    with path.open(newline="", encoding="utf-8") as handle:
        return max(0, sum(1 for _ in handle) - 1)


def stage(root: Path, results: list[dict[str, str]]) -> None:
    path = queue_path(root)
    path.parent.mkdir(parents=True, exist_ok=True)
    exists = path.exists()
    with path.open("a", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(
            handle, fieldnames=["provider", "title", "url", "kind", "rights"], delimiter="\t"
        )
        if not exists:
            writer.writeheader()
        writer.writerows(results)


def display(results: list[dict[str, str]], ansi: bool) -> None:
    a, g, m, r = (AMBER, GREEN, MUTED, RESET) if ansi else ("", "", "", "")
    if not results:
        print("NO MATCHING METADATA RETURNED")
        return
    for index, item in enumerate(results, 1):
        print(f"{a}[{index:02d}]{r} {g}{item['title']}{r}")
        print(f"     {item['provider']} // {item['kind']}")
        print(f"     {m}{item['url']}{r}")
        print(f"     RIGHTS: {item['rights']}")


def interactive(root: Path, limit: int) -> int:
    ansi = sys.stdout.isatty() and os.environ.get("TERM", "") != "dumb"
    a, g, m, red, r = (AMBER, GREEN, MUTED, RED, RESET) if ansi else ("", "", "", "", "")
    while True:
        print("\033[2J\033[H" if ansi else "")
        print(f"{a}WAYKEEPER // ONLINE MANUAL RESOURCE CONSOLE{r}")
        print("=" * 100)
        print(f"{red}DISCOVERY + METADATA STAGING ONLY // BULK DOWNLOAD DISABLED{r}")
        print(f"QUEUE {queue_count(root)} CANDIDATES // VERIFY MODEL, REVISION, RIGHTS, HASH, AND SAFETY BEFORE INTEGRATION")
        print("-" * 100)
        keys = list(PROVIDERS)
        for index, key in enumerate(keys, 1):
            provider = PROVIDERS[key]
            print(f"{a}[{index}]{r} {g}{provider['name']:<30}{r} {provider['mode']}")
            print(f"    {m}{provider['scope']}{r}")
        print(f"{a}[Q]{r} BACK")
        choice = input("RESOURCE> ").strip().lower()
        if choice in {"q", "quit", "back", "escape"}:
            return 0
        if not choice.isdigit() or not 1 <= int(choice) <= len(keys):
            continue
        provider = keys[int(choice) - 1]
        query = input("MODEL / PRODUCT / MANUAL QUERY> ").strip()
        if not query:
            continue
        try:
            results = search(provider, query, limit)
        except (urllib.error.URLError, TimeoutError, json.JSONDecodeError) as error:
            print(f"{red}OFFLINE OR PROVIDER ERROR: {error}{r}")
            input("[ RETURN ]")
            continue
        print("-" * 100)
        display(results, ansi)
        answer = input("STAGE THESE METADATA LINKS FOR LATER REVIEW? [y/N]> ").strip().lower()
        if answer in {"y", "yes"} and results:
            stage(root, results)
            print(f"{g}STAGED {len(results)} LINKS // NO PDF OR GUIDE CONTENT DOWNLOADED{r}")
        input("[ RETURN ]")


def main() -> int:
    parser = argparse.ArgumentParser(description="Search and stage online manual metadata")
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--interactive", action="store_true")
    parser.add_argument("--provider", choices=PROVIDERS, default="archive")
    parser.add_argument("--query")
    parser.add_argument("--limit", type=int, default=10)
    parser.add_argument("--stage", action="store_true", help="stage returned metadata links")
    args = parser.parse_args()
    limit = min(25, max(1, args.limit))
    if args.interactive or not args.query:
        return interactive(args.root.resolve(), limit)
    try:
        results = search(args.provider, args.query, limit)
    except (urllib.error.URLError, TimeoutError, json.JSONDecodeError) as error:
        print(f"OFFLINE OR PROVIDER ERROR: {error}", file=sys.stderr)
        return 2
    display(results, sys.stdout.isatty())
    if args.stage:
        stage(args.root.resolve(), results)
        print(f"STAGED {len(results)} LINKS // NO CONTENT DOWNLOADED")
    return 0 if results else 1


if __name__ == "__main__":
    raise SystemExit(main())
