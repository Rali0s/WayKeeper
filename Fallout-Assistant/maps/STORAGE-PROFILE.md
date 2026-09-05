# WayKeeper State Map Pack Profile

WayKeeper installs map data one state at a time. F6 discovers independent
two-letter state directories under `maps/` and shows only installed packs.
Removing or adding one state directory does not alter any other state.

## Core field pack budget

Target per state: **48 MB or less** for the default survival overview.

| Layer | Preferred format | Per-state target |
|---|---|---:|
| Terrain overview | compressed GeoTIFF, WGS84 | 16 MB |
| Hiking trails | spatially indexed GeoPackage | 12 MB |
| Railroads | spatially indexed GeoPackage | 8 MB |
| State boundary | simplified GeoJSON/GeoPackage | 2 MB |
| Towns and named settlements | point-only GeoPackage | 4 MB |
| Previews, manifest, checksums | PNG/TSV/text | 6 MB |

At the full 48 MB ceiling, all 50 state overview packs occupy about 2.4 GB.
That leaves substantial room on a 256 GB microSD card for manuals, indexes,
regional detail packs, and redundancy. Navigation-grade topographic tiles are
optional regional packs and are not duplicated into the core state overview.

## Overlay rules

- Trails, rails, towns, and settlements remain vector features and are drawn
  over terrain only for the current viewport.
- F6 allocates small byte masks at terminal resolution; it does not load a
  statewide image overlay into memory.
- Trail geometry receives a dark visibility halo and gold core at render time,
  so the source data remains compact.
- Towns and settlements must be points with a short label and class. Do not ship
  satellite imagery, building footprints, or decorative tiles in the core pack.
- Each state pack carries source URLs, acquisition date, bounds, feature counts,
  license/public-domain notes, and SHA-256 checksums.
- Safety status such as closures, legal access, washouts, or bridge condition is
  never inferred from an offline line on the map.

## Current installed packs

New York occupies approximately **14.4 MB** and contains a 4.2 MB terrain
preview, a 5.8 MB USGS trail GeoPackage with 17,392 clipped pedestrian trail
segments, a 1.7 MB railroad GeoPackage with 5,560 clipped segments, the state
boundary, and visual previews. This is comfortably inside the core field-pack
budget.

Florida occupies approximately **6.2 MB** and contains a compressed 1200 x 1000
terrain preview, 9,490 clipped pedestrian trail segments, 4,486 clipped
railroad segments, the official state boundary, and visual previews. Its flat
terrain compresses especially well and remains far below the 48 MB ceiling.
