# Phase 04 — Deep South and Lower Mississippi

## Scope

States: **Alabama, Mississippi, Tennessee, Arkansas, Louisiana, Missouri**  
Nominal timebox: **60-105 minutes**  
Temporary workspace ceiling: **2 GB**

This phase emphasizes river corridors, wetlands, flat-terrain compression, and
long narrow states. Hydrography must aid orientation without turning the ANSI
map into a solid blue mask.

## State work cards

### Alabama — AL

```sh
./scripts/maps/build_state_pack.sh --state AL --edition full --road-scale 1m --resume
./scripts/maps/verify_state_pack.sh --state AL
```

- Inspect Tennessee Valley, central routes, and Gulf coast separately.
- Keep Mobile Bay polygons bounded at statewide zoom.

### Mississippi — MS

```sh
./scripts/maps/build_state_pack.sh --state MS --edition full --road-scale 1m --resume
./scripts/maps/verify_state_pack.sh --state MS
```

- Verify Mississippi River landmarks and Delta hydrography.
- Confirm flat terrain remains visually useful through hillshade.

### Tennessee — TN

```sh
./scripts/maps/build_state_pack.sh --state TN --edition full --road-scale 1m --resume
./scripts/maps/verify_state_pack.sh --state TN
```

- Use a wide, shallow terrain raster rather than a square default.
- Validate Memphis, Nashville, Knoxville, and eastern relief at 4x.

### Arkansas — AR

```sh
./scripts/maps/build_state_pack.sh --state AR --edition full --road-scale 1m --resume
./scripts/maps/verify_state_pack.sh --state AR
```

- Inspect Ozark/Ouachita relief and Mississippi lowlands.
- Verify trail density remains thin and route labels stay readable.

### Louisiana — LA

```sh
./scripts/maps/build_state_pack.sh --state LA --edition full --road-scale 1m --resume
./scripts/maps/verify_state_pack.sh --state LA
```

- Treat wetlands, distributaries, lakes, and coastline as the phase's hydro test.
- Reject any view where water masks the road/town orientation network.

### Missouri — MO

```sh
./scripts/maps/build_state_pack.sh --state MO --edition full --road-scale 1m --resume
./scripts/maps/verify_state_pack.sh --state MO
```

- Verify Missouri and Mississippi river references independently.
- Inspect Ozark relief and the St. Louis/Kansas City label clusters.

## Phase gate

- [ ] All six states pass standard acceptance gates.
- [ ] Louisiana hydrography remains legible and bounded.
- [ ] Tennessee uses an aspect-correct raster.
- [ ] Flat states retain useful terrain shading.
- [ ] Phase measurements are recorded below.

## Completion record

| State | Status | Full MB | Lite MB | Elapsed | Notes |
|---|---|---:|---:|---:|---|
| AL | Not started | — | — | — | — |
| MS | Not started | — | — | — | — |
| TN | Not started | — | — | — | — |
| AR | Not started | — | — | — | — |
| LA | Not started | — | — | — | — |
| MO | Not started | — | — | — | — |

Phase status: **NOT STARTED**

