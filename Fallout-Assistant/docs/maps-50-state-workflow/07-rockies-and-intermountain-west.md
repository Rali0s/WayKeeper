# Phase 07 — Rockies and Intermountain West

## Scope

States: **Colorado, Wyoming, Montana, Idaho, Utah**  
Nominal timebox: **75-120 minutes**  
Temporary workspace ceiling: **2 GB**

This phase is terrain- and trail-heavy. The build must keep mountain relief
readable and avoid turning dense recreation networks into broad yellow areas.

## State work cards

### Colorado — CO

```sh
./scripts/maps/build_state_pack.sh --state CO --edition full --road-scale 1m --resume
./scripts/maps/verify_state_pack.sh --state CO
```

- Treat this as the primary mountain hillshade benchmark.
- Inspect Front Range town/road density and western trail density independently.

### Wyoming — WY

```sh
./scripts/maps/build_state_pack.sh --state WY --edition full --road-scale 1m --resume
./scripts/maps/verify_state_pack.sh --state WY
```

- Verify sparse towns remain prioritized.
- Inspect Yellowstone/Teton trails without implying seasonal access.

### Montana — MT

```sh
./scripts/maps/build_state_pack.sh --state MT --edition full --road-scale 1m --resume
./scripts/maps/verify_state_pack.sh --state MT
```

- Use a wide raster and retain eastern plains plus western relief.
- Expect long route geometry; route points must be precomputed.

### Idaho — ID

```sh
./scripts/maps/build_state_pack.sh --state ID --edition full --road-scale 1m --resume
./scripts/maps/verify_state_pack.sh --state ID
```

- Handle the long narrow panhandle without losing southern Idaho.
- Check trail and river overlap in mountain corridors.

### Utah — UT

```sh
./scripts/maps/build_state_pack.sh --state UT --edition full --road-scale 1m --resume
./scripts/maps/verify_state_pack.sh --state UT
```

- Preserve desert plateau and mountain contrast.
- Check Great Salt Lake water geometry and Wasatch Front density.

## Phase gate

- [ ] All five states pass standard acceptance gates.
- [ ] Colorado passes the mountain hillshade benchmark.
- [ ] Trails remain single-cell at all standard zoom levels.
- [ ] Sparse town ranking works in Wyoming and Montana.
- [ ] Phase measurements are recorded below.

## Completion record

| State | Status | Full MB | Lite MB | Elapsed | Notes |
|---|---|---:|---:|---:|---|
| CO | Not started | — | — | — | — |
| WY | Not started | — | — | — | — |
| MT | Not started | — | — | — | — |
| ID | Not started | — | — | — | — |
| UT | Not started | — | — | — | — |

Phase status: **NOT STARTED**

