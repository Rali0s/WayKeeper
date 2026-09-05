#!/usr/bin/env python3
"""Create a resumable offline mirror of TEXTFILES.COM's .txt collection.

The crawler begins at http://textfiles.com/directory.html, follows the site's
bold directory links, saves the directory pages for offline browsing, and
downloads every linked file whose path ends in .txt (case-insensitive).
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import os
import re
import threading
import time
from collections import deque
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import dataclass
from datetime import datetime, timezone
from html import escape
from html.parser import HTMLParser
from pathlib import Path
from typing import Iterable
from urllib.error import HTTPError, URLError
from urllib.parse import quote, quote_from_bytes, unquote, urljoin, urlsplit, urlunsplit
from urllib.request import Request, urlopen


START_URL = "http://textfiles.com/directory.html"
HOST = "textfiles.com"
USER_AGENT = "TXTFilesOfflineArchive/1.0 (personal archival mirror; resumable)"


class LinkParser(HTMLParser):
    def __init__(self) -> None:
        super().__init__(convert_charrefs=True)
        self.bold_depth = 0
        self.links: list[tuple[str, bool]] = []

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        tag = tag.lower()
        if tag in {"b", "strong"}:
            self.bold_depth += 1
        elif tag == "a":
            href = dict(attrs).get("href")
            if href:
                self.links.append((href, self.bold_depth > 0))

    def handle_endtag(self, tag: str) -> None:
        if tag.lower() in {"b", "strong"} and self.bold_depth:
            self.bold_depth -= 1


class RateLimiter:
    def __init__(self, interval: float) -> None:
        self.interval = max(0.0, interval)
        self.next_allowed = 0.0
        self.lock = threading.Lock()

    def wait(self) -> None:
        with self.lock:
            now = time.monotonic()
            pause = max(0.0, self.next_allowed - now)
            self.next_allowed = max(now, self.next_allowed) + self.interval
        if pause:
            time.sleep(pause)


@dataclass(frozen=True)
class DownloadResult:
    url: str
    relative_path: str
    size: int
    sha256: str
    status: str
    error: str = ""


def canonical_url(raw_url: str, base: str) -> str | None:
    joined = urljoin(base, raw_url)
    parts = urlsplit(joined)
    if parts.scheme not in {"http", "https"} or parts.hostname not in {HOST, f"www.{HOST}"}:
        return None
    path = re.sub(r"/{2,}", "/", parts.path or "/")
    return urlunsplit(("http", HOST, path, "", ""))


def first_path_part(url: str) -> str:
    parts = [part for part in urlsplit(url).path.split("/") if part]
    return parts[0] if parts else ""


def looks_like_directory_link(url: str, was_bold: bool) -> bool:
    if not was_bold:
        return False
    leaf = urlsplit(url).path.rstrip("/").rsplit("/", 1)[-1]
    return bool(leaf) and "." not in leaf


def request_bytes(url: str, limiter: RateLimiter, timeout: float, retries: int) -> tuple[bytes, str]:
    last_error: Exception | None = None
    for attempt in range(retries + 1):
        try:
            limiter.wait()
            # Much of this archive predates UTF-8. Some hrefs contain literal
            # ISO-8859-1 filename bytes, which HTTP clients require us to quote.
            parts = urlsplit(url)
            try:
                path_bytes = parts.path.encode("latin-1")
            except UnicodeEncodeError:
                path_bytes = parts.path.encode("utf-8")
            request_url = urlunsplit(
                (parts.scheme, parts.netloc, quote_from_bytes(path_bytes, safe="/%:@!$&'()*+,;=-._~"), parts.query, "")
            )
            request = Request(request_url, headers={"User-Agent": USER_AGENT, "Accept-Encoding": "identity"})
            with urlopen(request, timeout=timeout) as response:
                return response.read(), response.headers.get_content_type()
        except (HTTPError, URLError, TimeoutError, ConnectionError) as exc:
            last_error = exc
            if attempt < retries:
                time.sleep(min(8.0, 0.75 * (2**attempt)))
    raise RuntimeError(str(last_error) if last_error else "request failed")


def safe_segment(segment: str) -> str:
    decoded = unquote(segment).replace("/", "_").replace("\\", "_").replace(":", "_")
    decoded = "".join(char if ord(char) >= 32 else "_" for char in decoded)
    if decoded in {"", ".", ".."}:
        decoded = "_"
    if len(decoded.encode("utf-8")) > 180:
        digest = hashlib.sha1(decoded.encode("utf-8")).hexdigest()[:10]
        while len(decoded.encode("utf-8")) > 160:
            decoded = decoded[:-1]
        decoded = f"{decoded}__{digest}"
    return decoded


def url_to_relative_txt_path(url: str) -> Path:
    segments = [safe_segment(part) for part in urlsplit(url).path.split("/") if part]
    if not segments:
        raise ValueError(f"URL has no file path: {url}")
    return Path(*segments)


def page_output_path(site_root: Path, url: str) -> Path:
    path = urlsplit(url).path
    if path == "/directory.html":
        return site_root / "directory.html"
    segments = [safe_segment(part) for part in path.split("/") if part]
    return site_root.joinpath(*segments, "index.html")


def write_text_lines(path: Path, lines: Iterable[str]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temp = path.with_suffix(path.suffix + ".part")
    with temp.open("w", encoding="utf-8", newline="\n") as handle:
        for line in lines:
            handle.write(f"{line}\n")
    temp.replace(path)


def discover(site_root: Path, metadata_root: Path, limiter: RateLimiter, timeout: float, retries: int) -> tuple[list[str], list[str]]:
    print(f"Discovering directory tree from {START_URL}", flush=True)
    entry, _ = request_bytes(START_URL, limiter, timeout, retries)
    entry_parser = LinkParser()
    entry_parser.feed(entry.decode("latin-1", errors="replace"))
    # The main page later repeats a few navigation links in uppercase. Keep the
    # first spelling for each category so case-sensitive servers are not asked
    # for duplicate/nonexistent paths.
    categories_by_name: dict[str, str] = {}
    for href, bold in entry_parser.links:
        target = canonical_url(href, START_URL)
        if target is not None and bold and looks_like_directory_link(target, bold):
            categories_by_name.setdefault(first_path_part(target).casefold(), target)
    category_urls = set(categories_by_name.values())
    allowed_categories = {first_path_part(url).casefold() for url in category_urls}
    page_output_path(site_root, START_URL).parent.mkdir(parents=True, exist_ok=True)
    page_output_path(site_root, START_URL).write_bytes(entry)

    queue: deque[str] = deque(sorted(category_urls, key=str.casefold))
    queued = set(queue)
    visited: set[str] = set()
    txt_urls: set[str] = set()
    page_errors: list[str] = []

    while queue:
        page_url = queue.popleft()
        if page_url in visited:
            continue
        visited.add(page_url)
        try:
            body, content_type = request_bytes(page_url, limiter, timeout, retries)
            if content_type not in {"text/html", "application/xhtml+xml", "text/plain"}:
                continue
            parser = LinkParser()
            parser.feed(body.decode("latin-1", errors="replace"))
            # A non-HTML extensionless file will normally contain no useful anchors.
            if parser.links or body.lstrip().lower().startswith((b"<html", b"<!doctype")):
                output = page_output_path(site_root, page_url)
                output.parent.mkdir(parents=True, exist_ok=True)
                output.write_bytes(body)
            # The live site exposes directories as extensionless URLs such as
            # /magazines, but relative links on those pages are directory-based.
            link_base = page_url.rstrip("/") + "/"
            for href, bold in parser.links:
                target = canonical_url(href, link_base)
                if target is None:
                    continue
                if first_path_part(target).casefold() not in allowed_categories:
                    continue
                if urlsplit(target).path.casefold().endswith(".txt"):
                    txt_urls.add(target)
                elif looks_like_directory_link(target, bold) and target not in queued:
                    queued.add(target)
                    queue.append(target)
        except Exception as exc:  # Continue so a later resumable run can retry.
            page_errors.append(f"{page_url}\t{exc}")
        if len(visited) % 100 == 0:
            print(f"  pages={len(visited):,} queued={len(queue):,} .txt links={len(txt_urls):,}", flush=True)

    urls = sorted(txt_urls, key=str.casefold)
    pages = sorted(visited, key=str.casefold)
    write_text_lines(metadata_root / "discovered_urls.txt", urls)
    write_text_lines(metadata_root / "directory_pages.txt", pages)
    write_text_lines(metadata_root / "discovery_errors.tsv", page_errors)
    print(f"Discovery complete: {len(pages):,} directory pages, {len(urls):,} .txt URLs", flush=True)
    return urls, page_errors


def allocate_paths(urls: list[str]) -> dict[str, Path]:
    allocated: dict[str, Path] = {}
    used: dict[str, str] = {}
    for url in urls:
        relative = url_to_relative_txt_path(url)
        collision_key = str(relative).casefold()
        if collision_key in used and used[collision_key] != url:
            digest = hashlib.sha1(url.encode("utf-8")).hexdigest()[:10]
            relative = relative.with_name(f"{relative.stem}__{digest}{relative.suffix}")
            collision_key = str(relative).casefold()
        used[collision_key] = url
        allocated[url] = relative
    return allocated


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def download_one(url: str, relative: Path, site_root: Path, limiter: RateLimiter, timeout: float, retries: int) -> DownloadResult:
    target = site_root / relative
    # Atomic .part writes mean even a zero-byte target is a completed source
    # file, not an interrupted download.
    if target.is_file():
        return DownloadResult(url, relative.as_posix(), target.stat().st_size, sha256_file(target), "existing")
    target.parent.mkdir(parents=True, exist_ok=True)
    temp = target.with_name(f".{target.name}.{os.getpid()}.{threading.get_ident()}.part")
    try:
        body, _ = request_bytes(url, limiter, timeout, retries)
        with temp.open("wb") as handle:
            handle.write(body)
        temp.replace(target)
        digest = hashlib.sha256(body).hexdigest()
        return DownloadResult(url, relative.as_posix(), len(body), digest, "downloaded")
    except Exception as exc:
        temp.unlink(missing_ok=True)
        return DownloadResult(url, relative.as_posix(), 0, "", "error", str(exc).replace("\t", " ").replace("\n", " "))


def write_manifest(metadata_root: Path, results: list[DownloadResult]) -> None:
    path = metadata_root / "manifest.csv"
    temp = path.with_suffix(".csv.part")
    with temp.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.writer(handle)
        writer.writerow(["relative_path", "bytes", "sha256", "status", "source_url"])
        for result in sorted(results, key=lambda item: item.relative_path.casefold()):
            writer.writerow([result.relative_path, result.size, result.sha256, result.status, result.url])
    temp.replace(path)


def build_sorted_indexes(site_root: Path, metadata_root: Path) -> list[tuple[str, int]]:
    rows = [
        (path.relative_to(site_root).as_posix(), path.stat().st_size)
        for path in site_root.rglob("*")
        if path.is_file() and path.suffix.casefold() == ".txt"
    ]
    by_path = sorted(rows, key=lambda row: row[0].casefold())
    by_name = sorted(rows, key=lambda row: (Path(row[0]).name.casefold(), row[0].casefold()))
    by_size = sorted(rows, key=lambda row: (-row[1], row[0].casefold()))
    write_text_lines(metadata_root / "SORTED_BY_PATH.txt", (path for path, _ in by_path))
    write_text_lines(metadata_root / "SORTED_BY_NAME.txt", (path for path, _ in by_name))
    write_text_lines(metadata_root / "SORTED_BY_SIZE.tsv", (f"{size}\t{path}" for path, size in by_size))
    html_path = metadata_root / "SORTED_FILES.html"
    html_temp = html_path.with_suffix(".html.part")
    with html_temp.open("w", encoding="utf-8", newline="\n") as handle:
        handle.write("""<!doctype html>
<meta charset="utf-8">
<title>TXTFILES Archive — Sorted Files</title>
<style>
body{background:#000;color:#0f0;font:14px ui-monospace,SFMono-Regular,Menlo,monospace;margin:2rem}
a{color:#0f0} table{border-collapse:collapse;width:100%} th,td{padding:.25rem .5rem;border-bottom:1px solid #063;text-align:left}
th{position:sticky;top:0;background:#020} td:first-child{text-align:right;white-space:nowrap}
</style>
<h1>TXTFILES ARCHIVE — Sorted by Path</h1>
<p>Files: """)
        handle.write(f"{len(by_path):,} &nbsp; Bytes: {sum(size for _, size in by_path):,}</p>\n")
        handle.write("<table><thead><tr><th>Bytes</th><th>Path</th></tr></thead><tbody>\n")
        for relative_path, size in by_path:
            href = "../offline_site/" + quote(relative_path, safe="/")
            handle.write(f'<tr><td>{size}</td><td><a href="{href}">{escape(relative_path)}</a></td></tr>\n')
        handle.write("</tbody></table>\n")
    html_temp.replace(html_path)
    return rows


def load_discovered_urls(metadata_root: Path) -> list[str]:
    path = metadata_root / "discovered_urls.txt"
    if not path.is_file():
        raise SystemExit(f"Missing {path}; run with --mode discover or --mode all first")
    return sorted({line.strip() for line in path.read_text(encoding="utf-8").splitlines() if line.strip()}, key=str.casefold)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mode", choices=("all", "discover", "download"), default="all")
    parser.add_argument("--site-root", type=Path, default=Path(__file__).resolve().parent / "offline_site")
    parser.add_argument("--metadata-root", type=Path, default=Path(__file__).resolve().parent / "metadata")
    parser.add_argument("--workers", type=int, default=6)
    parser.add_argument("--delay", type=float, default=0.06, help="Minimum seconds between all HTTP requests")
    parser.add_argument("--timeout", type=float, default=30.0)
    parser.add_argument("--retries", type=int, default=3)
    parser.add_argument("--limit", type=int, default=0, help="Download only the first N URLs (0 means all)")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.workers < 1 or args.workers > 32:
        raise SystemExit("--workers must be between 1 and 32")
    args.site_root.mkdir(parents=True, exist_ok=True)
    args.metadata_root.mkdir(parents=True, exist_ok=True)
    limiter = RateLimiter(args.delay)
    started = time.monotonic()
    discovery_errors: list[str] = []

    if args.mode in {"all", "discover"}:
        urls, discovery_errors = discover(args.site_root, args.metadata_root, limiter, args.timeout, args.retries)
    else:
        urls = load_discovered_urls(args.metadata_root)

    results: list[DownloadResult] = []
    if args.mode in {"all", "download"}:
        if args.limit:
            urls = urls[: args.limit]
        paths = allocate_paths(urls)
        print(f"Downloading {len(urls):,} .txt files with {args.workers} workers", flush=True)
        with ThreadPoolExecutor(max_workers=args.workers) as pool:
            futures = {
                pool.submit(download_one, url, paths[url], args.site_root, limiter, args.timeout, args.retries): url
                for url in urls
            }
            for completed, future in enumerate(as_completed(futures), start=1):
                results.append(future.result())
                if completed % 250 == 0 or completed == len(futures):
                    errors = sum(result.status == "error" for result in results)
                    downloaded = sum(result.status == "downloaded" for result in results)
                    print(f"  complete={completed:,}/{len(futures):,} downloaded={downloaded:,} errors={errors:,}", flush=True)
        write_manifest(args.metadata_root, results)
        write_text_lines(
            args.metadata_root / "download_errors.tsv",
            (f"{result.url}\t{result.error}" for result in results if result.status == "error"),
        )

    indexed = build_sorted_indexes(args.site_root, args.metadata_root)
    summary = {
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "start_url": START_URL,
        "mode": args.mode,
        "discovered_txt_urls": len(urls),
        "downloaded_txt_files": len(indexed),
        "downloaded_txt_bytes": sum(size for _, size in indexed),
        "download_errors": sum(result.status == "error" for result in results),
        "discovery_warnings": len(discovery_errors),
        "elapsed_seconds": round(time.monotonic() - started, 3),
    }
    (args.metadata_root / "run_summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(summary, indent=2), flush=True)
    return 1 if summary["download_errors"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
