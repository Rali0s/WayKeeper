# Offline Maps

Maps are stored with their source, bounds, resolution, acquisition date, and checksum. The first sample is a downsampled statewide preview for validating the ANSI terrain renderer; it is not the final navigation-grade offline tile pack.

The staged nationwide build is defined in the
[50-state map workflow](../docs/maps-50-state-workflow/README.md). It assigns
every state exactly once across nine independently triggerable phases and does
not authorize publishing or uploading the resulting packs.

## New York preview

- File: `ny/USGS-3DEP-New-York-State-preview.tif`
- Visual preview: `ny/USGS-3DEP-New-York-State-preview.png`
- Source: USGS 3DEP Elevation ImageServer
- Requested bounds: west -79.7624, south 40.4961, east -71.8562, north 45.0159
- Returned bounds: west -79.7624, south 40.1206, east -71.8562, north 45.3914
- Raster: 1200 x 800 Float32, WGS84
- Elevation statistics: -43.622 m to 1445.096 m
- Downloaded: 2026-08-16
- SHA-256: `e4556e82377ca6b3074dc3bd38e09a2f25dcf098e81d30fe386d58256a268aa6`
- PNG SHA-256: `89d5f10d6c3e50c768fb85faa82cf0817997faf90b8648fae8c2135982cd86ec`

Reproducible request endpoint:

```text
https://elevation.nationalmap.gov/arcgis/rest/services/3DEPElevation/ImageServer/exportImage
```

Parameters: `bbox=-79.7624,40.4961,-71.8562,45.0159`, `bboxSR=4326`, `imageSR=4326`, `size=1200,800`, `format=tiff`, `pixelType=F32`, `interpolation=RSP_BilinearInterpolation`.

The PNG is a quick elevation-color reference generated from the GeoTIFF and
`terrain-colors.txt`. The C++ terminal reads the GeoTIFF itself and adds local
hillshade while rendering ANSI true color; it does not depend on the PNG.

## New York hiking trails overview

Run `scripts/download_ny_trails.sh` to retrieve pedestrian-accessible terra
trails from the USGS National Transportation Dataset, clip them to the official
New York State boundary, simplify them for statewide display, and store the
result as an indexed GeoPackage. This is an overview layer, not a substitute for
current trail closures, land-manager notices, signage, or a detailed navigation
map.

- File: `ny/USGS-National-Digital-Trails-New-York-overview.gpkg`
- State boundary: `ny/USGS-National-Map-New-York-boundary.geojson`
- Original trail preview: `ny/USGS-3DEP-New-York-State-hiking-trails-overview.png`
- High-contrast trail + railroad preview: `ny/USGS-3DEP-New-York-State-trails-railroads-overview.png`
- Contents: 17,392 pedestrian-accessible terra-trail segments after state clipping

## New York railroad overlay

Run `scripts/download_ny_railroads.sh` to acquire USGS National Map railroad
features and clip them to New York. The current layer contains 5,560 clipped
segments. F6 starts at 4x zoom for route-following detail, supports `+`/`-`
zoom through 32x, renders trails as single-cell gold/`*`, railroads as cyan/`#`, and
trail/rail crossings as `X`. These lines are orientation references, not proof
that a route is open, safe, public, maintained, or legally accessible.

Run `scripts/render_ny_routes_preview.py` with the bundled workspace Python to
regenerate the 1200 x 800 overview. It rasterizes both source GeoPackages
against the terrain bounds and writes an amber/cyan legend directly into the PNG.

- GeoPackage size: 5.8 MB
- Source service refresh: July 2026
- Downloaded: 2026-08-16
- Trail GeoPackage SHA-256: `b683633a23017cbc1c6fe1dd11428cea66a0254d775db4c20c547eb49fc93fdb`
- Railroad GeoPackage SHA-256: `0af8048868b2e58ec62b09bf621b3e8419fcb9bba3304662e14330a9ec97ef3a`
- Boundary SHA-256: `a174443dd7acdcb7457b0dd0bb46886e9b28b24d0f55df6db7c941947239207a`
- Original trail preview SHA-256: `99206147107fef3a614754c43fea0922d44f1150bb958fde467aededefe8b9c2`
- High-contrast routes preview SHA-256: `f762a9ff81229c4bd76d618f43fc890bf145e519bc61dd7d4a2d74284adc8bd2`

## New York offline orientation layers

Run `scripts/download_ny_orientation_layers.sh` to rebuild the compact USGS
orientation pack. The live ANSI map draws major roads as `=`, railroads as `#`,
water as `~`, and thin trails as `*`. Collision-managed `@` town, `O` lake, and
`~` river, and `=` numbered-route icons mark named features; `#` follows rail
tracks. Press `N` for visible names and map quadrants. This workflow uses
physical signs, water, roads, rails, and terrain
and does not require GPS.

- Primary roads: 90 clipped features plus 90 precomputed labels, 164 KB, SHA-256 `8fb37b808b3fe16618ec231d058ef2f778def2d09f1e00607f4569939c8ce467`
- Incorporated places: 616 features, 224 KB, SHA-256 `224678d7f46cac653e519144e85a9239e0eb4e65c6feb9f74495a065219cfef4`
- Major river segments: 1,026 clipped features (named icons omit blank source records)
- Large water polygons: 144 clipped features
- Hydrography pack: 1.8 MB, SHA-256 `ef5a8551889fe2bc6c072b8d84fbd3f51b2d51e89b1ff202bcd6952b2d4a1d23`

The compact road layer is intentionally a statewide orientation network, not a
street atlas. Feature presence does not establish current access, bridge
condition, legal entry, or safety.

## Florida core pack

Florida is installed as an independent F6 state pack using the same compact
terrain-plus-vector design. Run `scripts/download_fl_map_pack.sh` to rebuild
the official boundary, 3DEP terrain, pedestrian-accessible trails, railroad
overlay, and base preview. Run `scripts/render_state_routes_preview.py fl` with
a Python environment containing Pillow to rebuild the high-contrast overview.

- Terrain: `fl/USGS-3DEP-Florida-State-preview.tif`
- Requested bounds: west -87.6349, south 24.3963, east -80.0314, north 31.0009
- Returned bounds: west -87.79591, south 24.3963, east -79.87039, north 31.0009
- Raster: 1200 x 1000 Float32, WGS84
- Elevation statistics: -18.846 m to 101.397 m
- Trails: 9,490 state-clipped pedestrian-accessible terra-trail segments
- Railroads: 4,486 state-clipped segments
- High-contrast preview: `fl/USGS-3DEP-FL-State-trails-railroads-overview.png`
- Pack size: approximately 6.2 MB
- Downloaded: 2026-08-16
- Terrain SHA-256: `41c87d051529cf074879ea6d5d6b63290a1b40daa69e7ea8631e466a8060f3b0`
- Trail SHA-256: `41039b0b2be9c6e96099d4614c3a0a4d66f7f612361fe5b661de9b1fba27fe3f`
- Railroad SHA-256: `fd94803a42ce8f882f90a0176604780f51b15124e9c1edb215ab9f2b96d734ca`
- Boundary SHA-256: `ae0be76bab70ec3db5630299b276156a1dbaab1c61d9e34a42e2f726028b641c`
- Routes preview SHA-256: `eb2ad5bdd39ce124ede5fdbfbb33d943781038b680595bf2518d86fde1db0335`

The trail and railroad lines are orientation references. They do not prove
public access, bridge condition, storm clearance, legal entry, or current
route availability. Florida hurricane, flood, wildfire, wildlife-area, and
land-manager notices must be checked whenever communications are available.
