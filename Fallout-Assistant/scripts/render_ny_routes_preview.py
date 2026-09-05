#!/usr/bin/env python3
"""Render a NY terrain preview with thin trails and railroads."""

from __future__ import annotations

import json
import subprocess
import tempfile
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parents[1]
NY = ROOT / "maps" / "ny"
TERRAIN = NY / "USGS-3DEP-New-York-State-preview.tif"
BASE = NY / "USGS-3DEP-New-York-State-preview.png"
TRAILS = NY / "USGS-National-Digital-Trails-New-York-overview.gpkg"
RAILS = NY / "USGS-National-Map-New-York-railroads.gpkg"
OUTPUT = NY / "USGS-3DEP-New-York-State-trails-railroads-overview.png"


def mask(vector: Path, layer: str, destination: Path, bounds: tuple[float, ...], size: tuple[int, int]) -> None:
    west, south, east, north = bounds
    subprocess.run([
        "gdal_rasterize", "-q", "-burn", "255", "-ot", "Byte", "-of", "GTiff", "-at",
        "-te", str(west), str(south), str(east), str(north),
        "-ts", str(size[0]), str(size[1]), "-l", layer, str(vector), str(destination),
    ], check=True)


def main() -> int:
    info = json.loads(subprocess.check_output(["gdalinfo", "-json", str(TERRAIN)]))
    corners = info["cornerCoordinates"]
    bounds = (
        corners["lowerLeft"][0], corners["lowerLeft"][1],
        corners["upperRight"][0], corners["upperRight"][1],
    )
    base = Image.open(BASE).convert("RGB")
    with tempfile.TemporaryDirectory(prefix="waykeeper-routes-") as temporary:
        temporary = Path(temporary)
        trail_path = temporary / "trails.tif"
        rail_path = temporary / "rails.tif"
        mask(TRAILS, "hiking_trails", trail_path, bounds, base.size)
        mask(RAILS, "railroads", rail_path, bounds, base.size)
        trail = Image.open(trail_path).convert("L")
        rail = Image.open(rail_path).convert("L").filter(ImageFilter.MaxFilter(3))
        trail_color = Image.new("RGB", base.size, (255, 210, 30))
        rail_color = Image.new("RGB", base.size, (30, 225, 255))
        base.paste(trail_color, mask=trail)
        base.paste(rail_color, mask=rail)

    draw = ImageDraw.Draw(base)
    draw.rounded_rectangle((18, 18, 410, 82), radius=8, fill=(8, 18, 24), outline=(235, 235, 220), width=2)
    draw.line((34, 40, 94, 40), fill=(255, 210, 30), width=5)
    draw.text((108, 31), "HIKING TRAIL", fill=(255, 235, 150))
    draw.line((34, 64, 94, 64), fill=(30, 225, 255), width=3)
    draw.text((108, 55), "RAILROAD - ACCESS NOT IMPLIED", fill=(160, 245, 255))
    base.save(OUTPUT, optimize=True)
    print(OUTPUT)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
