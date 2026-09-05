# Phase 01 — Calibration and Northeast Core

## Scope

States: **New York, Florida, Delaware, Rhode Island, Connecticut, New Jersey**  
Nominal timebox: **3-5 hours on the first run; 60-90 minutes after the builder exists**  
Temporary workspace ceiling: **2 GB**

This phase establishes the generic builder contract and replaces the two
prototype packs with repeatable Full packs. Do not begin Phase 02 until New York
and Florida have matching layers and one clean command can rebuild either state.

## Phase preparation

- [ ] Implement `scripts/maps/build_state_pack.sh` if it is absent.
- [ ] Implement the verifier and packager described in `README.md`.
- [ ] Add a data-driven state manifest; do not hard-code state logic in shell.
- [ ] Preserve the current New York and Florida packs until replacements pass.
- [ ] Confirm road source is USGS 1M-scale, not the prototype 10M layer.

## State work cards

### New York — NY

```sh
./scripts/maps/build_state_pack.sh --state NY --edition full --road-scale 1m --resume
./scripts/maps/verify_state_pack.sh --state NY
```

- Reuse the working town/hydro/landmark renderer as the calibration reference.
- Replace 10M roads with 1M roads and precomputed route markers.
- Compare trail, rail, town, river, lake, and road counts to the prior manifest.
- Verify central New York exposes town, lake, river, road, and rail references.

### Florida — FL

```sh
./scripts/maps/build_state_pack.sh --state FL --edition full --road-scale 1m --resume
./scripts/maps/verify_state_pack.sh --state FL
```

- Add the towns, roads, route markers, and hydrography missing from the prototype.
- Preserve flat-terrain compression and avoid treating ocean as inland water.
- Inspect Keys, the Everglades, and Panhandle extents at statewide zoom.

### Delaware — DE

```sh
./scripts/maps/build_state_pack.sh --state DE --edition full --road-scale 1m --resume
./scripts/maps/verify_state_pack.sh --state DE
```

- Prevent Delaware Bay geometry from overwhelming the narrow land area.
- Confirm Wilmington and Dover survive collision management.

### Rhode Island — RI

```sh
./scripts/maps/build_state_pack.sh --state RI --edition full --road-scale 1m --resume
./scripts/maps/verify_state_pack.sh --state RI
```

- Use a land-focused raster aspect ratio around Narragansett Bay.
- Confirm the tiny state does not over-render towns or road labels.

### Connecticut — CT

```sh
./scripts/maps/build_state_pack.sh --state CT --edition full --road-scale 1m --resume
./scripts/maps/verify_state_pack.sh --state CT
```

- Inspect dense town and transport label collisions.
- Keep Long Island Sound from dominating hydrography color.

### New Jersey — NJ

```sh
./scripts/maps/build_state_pack.sh --state NJ --edition full --road-scale 1m --resume
./scripts/maps/verify_state_pack.sh --state NJ
```

- Validate dense rail/highway rendering without hiding terrain.
- Inspect the narrow statewide aspect and barrier-island coastline.

## Phase gate

- [ ] All six Full packs pass the standard acceptance gates.
- [ ] New York and Florida have identical required layer types.
- [ ] One generic command rebuilds every state in this phase.
- [ ] Full and Lite packages were extracted and inspected.
- [ ] Measured average size is used to revise the national estimate.

## Completion record

| State | Status | Full MB | Lite MB | Elapsed | Notes |
|---|---|---:|---:|---:|---|
| NY | Not started | — | — | — | — |
| FL | Not started | — | — | — | — |
| DE | Not started | — | — | — | — |
| RI | Not started | — | — | — | — |
| CT | Not started | — | — | — | — |
| NJ | Not started | — | — | — | — |

Phase status: **NOT STARTED**
