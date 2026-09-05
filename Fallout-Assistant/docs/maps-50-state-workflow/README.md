# WayKeeper 50-State Map Build Workflow

## Purpose

This directory divides the fifty-state offline map build into nine resumable
phases. Each phase is a separate trigger sheet that can be run weeks apart. A
phase is complete only when every state in its file passes the map-pack gates;
downloading files alone is not completion.

The workflow builds statewide field-orientation maps for a 640x480 ANSI
terminal. It does not create a street atlas, promise route access, or require
GPS. The field map uses terrain, named towns, major lakes and rivers, numbered
roads, rail lines, and pedestrian trails as physical orientation references.

## Scope and phase order

| Phase | States | Nominal timebox | Purpose |
|---|---|---:|---|
| [01](01-calibration-and-northeast-core.md) | NY, FL, DE, RI, CT, NJ | 3-5 hr first run; 60-90 min rerun | Builder contract and calibration |
| [02](02-northern-new-england-and-mid-atlantic.md) | ME, NH, VT, MA, PA, MD | 75-120 min | Coast, mountains, dense corridors |
| [03](03-appalachia-and-southeast-atlantic.md) | VA, WV, NC, SC, GA, KY | 75-120 min | Long states and Appalachian relief |
| [04](04-deep-south-and-lower-mississippi.md) | AL, MS, TN, AR, LA, MO | 60-105 min | Wetlands, rivers, narrow aspect ratios |
| [05](05-great-lakes-and-upper-midwest.md) | OH, IN, IL, MI, WI, MN | 75-120 min | Dense transport and Great Lakes geometry |
| [06](06-central-and-northern-plains.md) | IA, KS, NE, ND, SD | 45-75 min | Wide, relatively sparse states |
| [07](07-rockies-and-intermountain-west.md) | CO, WY, MT, ID, UT | 75-120 min | Mountain relief and trail density |
| [08](08-southwest-and-south-central.md) | TX, OK, NM, AZ, NV | 75-135 min | Large extents and desert terrain |
| [09](09-pacific-alaska-and-hawaii.md) | CA, OR, WA, AK, HI | 2-4 hr | Mandatory tiling and non-contiguous states |

Except for Phase 01's first run, the timeboxes assume the generic builder
exists, USGS services are responsive, and raw data do not need manual repair.
They are planning targets, not reasons to skip validation. Stop cleanly and
resume later if a service throttles.

## User trigger contract

When the user says `Run map phase NN`, the implementing agent must:

1. Read this file and the selected phase file completely.
2. Inspect existing state directories and preserve valid completed artifacts.
3. Build only the states named in that phase.
4. Keep incomplete downloads in temporary workspace, never in `maps/<state>/`.
5. Run the acceptance gates for each state.
6. Update that phase's completion record with measured results.
7. Stop before the next phase unless the user explicitly authorizes it.
8. Do not publish, upload, or create a release without separate authorization.

## Planned command contract

Phase 01 owns implementation of these commands if they do not yet exist:

```sh
./scripts/maps/build_state_pack.sh --state NY --edition full --road-scale 1m
./scripts/maps/verify_state_pack.sh --state NY
./scripts/maps/package_state_pack.sh --state NY --edition full
```

Later phases must use the same commands rather than creating state-specific
download scripts. The builder must support `--resume`, a bounded temporary
directory, retries with backoff, and a dry-run mode. It must not overwrite a
previously verified pack until the replacement passes validation.

## Full state-pack contract

Every completed `maps/<code>/` directory must contain:

| Artifact | Required content |
|---|---|
| Boundary | Official state boundary in WGS84 |
| Terrain | Compressed, tiled GeoTIFF with sensible aspect ratio |
| Roads | USGS 1M-scale roads, clipped and spatially indexed |
| Road markers | Precomputed named/numbered route points |
| Towns | Incorporated-place points with short display names |
| Hydrography | Major river lines and large water polygons |
| Rail | Clipped railroad geometry |
| Trails | Pedestrian-accessible terra trails |
| Manifest | Sources, query filters, bounds, counts, dates, and versions |
| Checksums | SHA-256 for every shipped artifact |

Full packs use USGS 1M-scale roads because that layer includes Interstate, US,
state, and other highways. The 10M layer used in the first New York prototype
is too sparse for nationwide field orientation. Local streets and secondary
road atlases remain outside this workflow.

## Lite state-pack contract

Lite packaging is derived from a verified Full pack; it is never downloaded as
an unrelated dataset. Lite retains terrain, boundary, towns, major hydrography,
and a sparse numbered-road overview. Trails, rail geometry, and decorative
previews may be omitted. The manifest must identify every omitted layer and
provide the Full-pack download name and checksum.

## Standard build sequence for each state

1. Resolve the official boundary and calculate a safe query envelope.
2. Choose one terrain raster or an approved multi-tile layout.
3. Download 3DEP terrain and write a compressed/tiled GeoTIFF.
4. Page through 1M roads, trails, and rail queries at no more than the service
   record limit.
5. Retrieve towns and major hydrography.
6. Clip vector layers to the state boundary and simplify only to the documented
   ANSI overview tolerances.
7. Precompute route-label points; never calculate long-road centroids during an
   Orange Pi render.
8. Create spatial indexes and normalize layer names expected by WayKeeper.
9. Write the manifest and checksums.
10. Render and inspect the standard LCD views.
11. Package Full and Lite archives only after validation.

## Required layer names

The runtime contract is intentionally stable:

```text
hiking_trails
railroads
major_roads
road_markers
towns
rivers
waterbodies
water_areas
```

Filename discovery tokens must remain `trail`, `railroad`, `primary-roads`,
`towns`, and `hydrography` unless the C++ map inspector is changed and tested in
the same phase.

## Per-state acceptance gates

- [ ] Every required Full layer opens through `gdalinfo` or `ogrinfo`.
- [ ] The state boundary has exactly the expected state identity.
- [ ] Vector extents intersect the boundary and do not contain obvious national
      or neighboring-state leakage.
- [ ] Terrain contains valid elevation statistics and no unexpected empty band.
- [ ] Road markers use recognizable route names or numbers.
- [ ] Blank and duplicate landmark labels are suppressed.
- [ ] Spatial indexes exist for every GeoPackage vector layer.
- [ ] The 80x24 and 96x28 ANSI maps fit without scrolling.
- [ ] Trails are single-cell gold with no dilation halo.
- [ ] `N` lists only landmarks visible in the current viewport.
- [ ] Pan, zoom, recenter, manual marks, and Escape behavior still work.
- [ ] The core test suite passes after installing the state.
- [ ] Manifest counts match the final clipped files.
- [ ] SHA-256 values match the shipped files.
- [ ] Full and Lite archive extraction is tested in a clean temporary directory.
- [ ] Total bytes and elapsed time are written into the phase record.

## Stop conditions

Pause the phase and report rather than silently lowering quality when:

- an official service changes fields or layer identifiers;
- a state returns zero towns, roads, or hydrography unexpectedly;
- the Full state pack exceeds 80 MB;
- temporary usage exceeds the phase's stated workspace ceiling;
- a query repeatedly returns incomplete pagination;
- Alaska crosses the antimeridian incorrectly;
- island or peninsula geometry becomes unreadable at 80x24;
- a source license or attribution is unclear;
- the production build or tests fail.

## Storage controls

- Normal phases: reserve 2 GB temporary workspace.
- Phase 08: reserve 3 GB because of Texas.
- Phase 09: reserve 6 GB because of California and Alaska tiling.
- Target ordinary Full state: 10-25 MB.
- Warning threshold: 48 MB.
- Hard review threshold: 80 MB.
- Expected complete Full collection: approximately 700 MB-1.1 GB.

GeoTIFF and GeoPackage artifacts are already compressed. Do not assume ZIP will
reduce the final collection substantially. Report both archive and extracted
sizes.

## Program completion gate

After Phase 09, generate a national catalog but do not publish it. Confirm that
all fifty state abbreviations appear exactly once, then calculate measured Full
and Lite totals. District of Columbia and U.S. territories are explicitly
outside the fifty-state run and may be proposed as a later optional phase.
