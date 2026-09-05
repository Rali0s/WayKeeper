# Phase 06 — Central and Northern Plains

## Scope

States: **Iowa, Kansas, Nebraska, North Dakota, South Dakota**  
Nominal timebox: **45-75 minutes**  
Temporary workspace ceiling: **2 GB**

This is the lightest expected phase and the best checkpoint for confirming that
the generic builder handles wide, sparse states without unnecessary raster or
vector inflation.

## State work cards

### Iowa — IA

```sh
./scripts/maps/build_state_pack.sh --state IA --edition full --road-scale 1m --resume
./scripts/maps/verify_state_pack.sh --state IA
```

- Emphasize river, numbered-road, rail, and town references.
- Confirm subdued relief remains visible beneath transport lines.

### Kansas — KS

```sh
./scripts/maps/build_state_pack.sh --state KS --edition full --road-scale 1m --resume
./scripts/maps/verify_state_pack.sh --state KS
```

- Use a wide aspect ratio and avoid excessive vertical padding.
- Verify sparse landmarks distribute across all map quadrants.

### Nebraska — NE

```sh
./scripts/maps/build_state_pack.sh --state NE --edition full --road-scale 1m --resume
./scripts/maps/verify_state_pack.sh --state NE
```

- Preserve the western panhandle in the terrain viewport.
- Verify Platte River, rail, and highway corridors remain distinguishable.

### North Dakota — ND

```sh
./scripts/maps/build_state_pack.sh --state ND --edition full --road-scale 1m --resume
./scripts/maps/verify_state_pack.sh --state ND
```

- Confirm sparse incorporated-place data still yields usable orientation.
- Inspect western relief separately from the Red River valley.

### South Dakota — SD

```sh
./scripts/maps/build_state_pack.sh --state SD --edition full --road-scale 1m --resume
./scripts/maps/verify_state_pack.sh --state SD
```

- Verify Black Hills and Badlands relief at 4x zoom.
- Keep Missouri River water geometry from dividing the map visually.

## Phase gate

- [ ] All five states pass standard acceptance gates.
- [ ] Wide rasters use documented aspect ratios.
- [ ] Sparse states still display useful named landmarks.
- [ ] No state exceeds 48 MB without an explained source-density reason.
- [ ] Phase measurements are recorded below.

## Completion record

| State | Status | Full MB | Lite MB | Elapsed | Notes |
|---|---|---:|---:|---:|---|
| IA | Not started | — | — | — | — |
| KS | Not started | — | — | — | — |
| NE | Not started | — | — | — | — |
| ND | Not started | — | — | — | — |
| SD | Not started | — | — | — | — |

Phase status: **NOT STARTED**

