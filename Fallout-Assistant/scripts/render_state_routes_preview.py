#!/usr/bin/env python3
"""Render a state terrain preview with high-contrast trail and rail overlays."""

from __future__ import annotations

import argparse
import json
import subprocess
import tempfile
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parents[1]


def one(directory: Path, pattern: str) -> Path:
    matches = sorted(directory.glob(pattern))
    if len(matches) != 1:
        raise SystemExit(f"Expected one {pattern} under {directory}; found {len(matches)}")
    return matches[0]


def mask(vector: Path, layer: str, destination: Path,
         bounds: tuple[float, ...], size: tuple[int, int]) -> None:
    west, south, east, north = bounds
    subprocess.run([
        "gdal_rasterize", "-q", "-burn", "255", "-ot", "Byte", "-of", "GTiff", "-at",
        "-te", str(west), str(south), str(east), str(north),
        "-ts", str(size[0]), str(size[1]), "-l", layer, str(vector), str(destination),
    ], check=True)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("state", help="installed two-letter state directory, for example fl")
    args = parser.parse_args()
    state = args.state.lower()
    directory = ROOT / "maps" / state
    terrain = one(directory, "*State-preview.tif")
    base_path = one(directory, "*State-preview.png")
    trails = one(directory, "*Trail*overview.gpkg")
    rails = one(directory, "*railroad*.gpkg")
    output = directory / f"USGS-3DEP-{state.upper()}-State-trails-railroads-overview.png"

    info = json.loads(subprocess.check_output(["gdalinfo", "-json", str(terrain)]))
    corners = info["cornerCoordinates"]
    bounds = (
        corners["lowerLeft"][0], corners["lowerLeft"][1],
        corners["upperRight"][0], corners["upperRight"][1],
    )
    base = Image.open(base_path).convert("RGB")
    with tempfile.TemporaryDirectory(prefix=f"waykeeper-{state}-routes-") as temporary:
        temporary_path = Path(temporary)
        trail_path = temporary_path / "trails.tif"
        rail_path = temporary_path / "rails.tif"
        mask(trails, "hiking_trails", trail_path, bounds, base.size)
        mask(rails, "railroads", rail_path, bounds, base.size)
        trail = Image.open(trail_path).convert("L")
        rail = Image.open(rail_path).convert("L").filter(ImageFilter.MaxFilter(3))
        base.paste(Image.new("RGB", base.size, (255, 210, 30)), mask=trail)
        base.paste(Image.new("RGB", base.size, (30, 225, 255)), mask=rail)

    draw = ImageDraw.Draw(base)
    draw.rounded_rectangle(
        (18, 18, 410, 82), radius=8, fill=(8, 18, 24), outline=(235, 235, 220), width=2)
    draw.line((34, 40, 94, 40), fill=(255, 210, 30), width=5)
    draw.text((108, 31), "HIKING TRAIL", fill=(255, 235, 150))
    draw.line((34, 64, 94, 64), fill=(30, 225, 255), width=3)
    draw.text((108, 55), "RAILROAD - ACCESS NOT IMPLIED", fill=(160, 245, 255))
    base.save(output, optimize=True)
    print(output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
