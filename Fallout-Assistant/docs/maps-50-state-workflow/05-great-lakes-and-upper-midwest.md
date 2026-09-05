# Phase 05 — Great Lakes and Upper Midwest

## Scope

States: **Ohio, Indiana, Illinois, Michigan, Wisconsin, Minnesota**  
Nominal timebox: **75-120 minutes**  
Temporary workspace ceiling: **2 GB**

This phase combines dense transportation networks with major water bodies.
Michigan is the first required multi-part state decision after the contiguous
single-raster phases.

## State work cards

### Ohio — OH

```sh
./scripts/maps/build_state_pack.sh --state OH --edition full --road-scale 1m --resume
./scripts/maps/verify_state_pack.sh --state OH
```

- Check dense towns, highways, and rail corridors.
- Keep Lake Erie blue edge cells from masking northern routes.

### Indiana — IN

```sh
./scripts/maps/build_state_pack.sh --state IN --edition full --road-scale 1m --resume
./scripts/maps/verify_state_pack.sh --state IN
```

- Validate flat-terrain contrast and dense road/rail intersections.
- Confirm town priority at 80x24.

### Illinois — IL

```sh
./scripts/maps/build_state_pack.sh --state IL --edition full --road-scale 1m --resume
./scripts/maps/verify_state_pack.sh --state IL
```

- Expect high rail and road density around Chicago.
- Verify the Mississippi and Illinois rivers remain distinct.

### Michigan — MI

```sh
./scripts/maps/build_state_pack.sh --state MI --edition full --road-scale 1m --resume
./scripts/maps/verify_state_pack.sh --state MI
```

- Build Lower and Upper Peninsula terrain tiles under one state manifest.
- Do not use one ocean-sized bounding rectangle for both peninsulas.
- Verify Great Lakes polygons do not overwhelm either peninsula view.

### Wisconsin — WI

```sh
./scripts/maps/build_state_pack.sh --state WI --edition full --road-scale 1m --resume
./scripts/maps/verify_state_pack.sh --state WI
```

- Check Lake Michigan/Superior edges and inland water density.
- Validate town and rail references in the southeast.

### Minnesota — MN

```sh
./scripts/maps/build_state_pack.sh --state MN --edition full --road-scale 1m --resume
./scripts/maps/verify_state_pack.sh --state MN
```

- Limit lake landmark icons through ranking and collision handling.
- Check the Northwest Angle does not distort the useful state view.

## Phase gate

- [ ] All six states pass standard acceptance gates.
- [ ] Michigan uses a documented two-region layout.
- [ ] Great Lakes cells do not dominate land navigation.
- [ ] Dense Chicago/Ohio transport networks remain readable.
- [ ] Phase measurements are recorded below.

## Completion record

| State | Status | Full MB | Lite MB | Elapsed | Notes |
|---|---|---:|---:|---:|---|
| OH | Not started | — | — | — | — |
| IN | Not started | — | — | — | — |
| IL | Not started | — | — | — | — |
| MI | Not started | — | — | — | — |
| WI | Not started | — | — | — | — |
| MN | Not started | — | — | — | — |

Phase status: **NOT STARTED**

