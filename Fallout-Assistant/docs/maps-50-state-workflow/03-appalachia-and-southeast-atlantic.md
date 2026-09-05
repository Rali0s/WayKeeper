# Phase 03 — Appalachia and Southeast Atlantic

## Scope

States: **Virginia, West Virginia, North Carolina, South Carolina, Georgia, Kentucky**  
Nominal timebox: **75-120 minutes**  
Temporary workspace ceiling: **2 GB**

This phase tests long east-west terrain, Appalachian trail density, coastal
geometry, and river networks. Follow the common command and validation contract
in `README.md`.

## State work cards

### Virginia — VA

```sh
./scripts/maps/build_state_pack.sh --state VA --edition full --road-scale 1m --resume
./scripts/maps/verify_state_pack.sh --state VA
```

- Preserve both Appalachian west and Tidewater east in an aspect-correct raster.
- Verify the long state does not compress towns into one horizontal band.

### West Virginia — WV

```sh
./scripts/maps/build_state_pack.sh --state WV --edition full --road-scale 1m --resume
./scripts/maps/verify_state_pack.sh --state WV
```

- Expect dense relief, rivers, and trails.
- Confirm road/rail valleys remain distinguishable from blue river lines.

### North Carolina — NC

```sh
./scripts/maps/build_state_pack.sh --state NC --edition full --road-scale 1m --resume
./scripts/maps/verify_state_pack.sh --state NC
```

- Inspect mountain, Piedmont, and Outer Banks views independently.
- Prevent barrier islands from forcing excessive empty-ocean raster space.

### South Carolina — SC

```sh
./scripts/maps/build_state_pack.sh --state SC --edition full --road-scale 1m --resume
./scripts/maps/verify_state_pack.sh --state SC
```

- Check coastal water polygons and inland river references.
- Validate Charleston-area density without sacrificing statewide clarity.

### Georgia — GA

```sh
./scripts/maps/build_state_pack.sh --state GA --edition full --road-scale 1m --resume
./scripts/maps/verify_state_pack.sh --state GA
```

- Inspect the transition from northern relief to coastal plain.
- Confirm Atlanta label density remains collision-managed.

### Kentucky — KY

```sh
./scripts/maps/build_state_pack.sh --state KY --edition full --road-scale 1m --resume
./scripts/maps/verify_state_pack.sh --state KY
```

- Use a wide raster suited to the state's east-west shape.
- Verify Ohio River and Appalachian references are visible but not oversized.

## Phase gate

- [ ] All six states pass standard acceptance gates.
- [ ] Wide-state rasters use documented aspect ratios.
- [ ] Coastal states have no excessive ocean padding.
- [ ] Appalachian states retain terrain underneath route overlays.
- [ ] Phase measurements are recorded below.

## Completion record

| State | Status | Full MB | Lite MB | Elapsed | Notes |
|---|---|---:|---:|---:|---|
| VA | Not started | — | — | — | — |
| WV | Not started | — | — | — | — |
| NC | Not started | — | — | — | — |
| SC | Not started | — | — | — | — |
| GA | Not started | — | — | — | — |
| KY | Not started | — | — | — | — |

Phase status: **NOT STARTED**

