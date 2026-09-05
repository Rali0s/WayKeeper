# Phase 02 — Northern New England and Mid-Atlantic

## Scope

States: **Maine, New Hampshire, Vermont, Massachusetts, Pennsylvania, Maryland**  
Nominal timebox: **75-120 minutes**  
Temporary workspace ceiling: **2 GB**

Run only after Phase 01 establishes the generic builder and current layer
contract. This phase exercises mountains, islands, irregular coastlines, dense
rail networks, and the Chesapeake Bay.

## State work cards

### Maine — ME

```sh
./scripts/maps/build_state_pack.sh --state ME --edition full --road-scale 1m --resume
./scripts/maps/verify_state_pack.sh --state ME
```

- Keep the long coast and offshore islands from expanding the useful viewport.
- Confirm northern towns and routes remain visible despite sparse settlement.

### New Hampshire — NH

```sh
./scripts/maps/build_state_pack.sh --state NH --edition full --road-scale 1m --resume
./scripts/maps/verify_state_pack.sh --state NH
```

- Inspect White Mountain relief and trail density.
- Ensure trail gold does not conceal terrain at 4x zoom.

### Vermont — VT

```sh
./scripts/maps/build_state_pack.sh --state VT --edition full --road-scale 1m --resume
./scripts/maps/verify_state_pack.sh --state VT
```

- Verify mountain corridors, named rivers, and north-south highways.
- Inspect labels around Lake Champlain without letting water dominate.

### Massachusetts — MA

```sh
./scripts/maps/build_state_pack.sh --state MA --edition full --road-scale 1m --resume
./scripts/maps/verify_state_pack.sh --state MA
```

- Validate Cape Cod, Martha's Vineyard, and Nantucket in the terrain layout.
- Test collision handling in the Boston transport/town cluster.

### Pennsylvania — PA

```sh
./scripts/maps/build_state_pack.sh --state PA --edition full --road-scale 1m --resume
./scripts/maps/verify_state_pack.sh --state PA
```

- Budget for dense rail, trail, river, and highway geometry.
- Verify Appalachian terrain remains readable underneath overlays.

### Maryland — MD

```sh
./scripts/maps/build_state_pack.sh --state MD --edition full --road-scale 1m --resume
./scripts/maps/verify_state_pack.sh --state MD
```

- Handle the narrow western extension and Chesapeake split deliberately.
- Keep bay polygons blue without masking both shores.

## Phase gate

- [ ] All six state packs pass the standard acceptance gates.
- [ ] Island/coastal extents contain no accidental national-scale envelope.
- [ ] Pennsylvania stays below the 80 MB hard review threshold.
- [ ] Mountain trail views preserve thin-line rendering.
- [ ] Phase measurements are recorded below.

## Completion record

| State | Status | Full MB | Lite MB | Elapsed | Notes |
|---|---|---:|---:|---:|---|
| ME | Not started | — | — | — | — |
| NH | Not started | — | — | — | — |
| VT | Not started | — | — | — | — |
| MA | Not started | — | — | — | — |
| PA | Not started | — | — | — | — |
| MD | Not started | — | — | — | — |

Phase status: **NOT STARTED**

