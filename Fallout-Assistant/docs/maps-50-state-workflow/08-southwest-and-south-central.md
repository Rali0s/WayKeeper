# Phase 08 — Southwest and South-Central

## Scope

States: **Texas, Oklahoma, New Mexico, Arizona, Nevada**  
Nominal timebox: **75-135 minutes**  
Temporary workspace ceiling: **3 GB**

Texas is the size and transport-density stress test. The other states exercise
sparse towns, extreme relief, desert hydrography, and long route geometry.

## State work cards

### Texas — TX

```sh
./scripts/maps/build_state_pack.sh --state TX --edition full --road-scale 1m --resume
./scripts/maps/verify_state_pack.sh --state TX
```

- Permit two terrain regions if a single statewide raster is unreadably coarse.
- Expect the phase's highest road count and potentially high rail volume.
- Stop for review if the Full pack exceeds 80 MB.

### Oklahoma — OK

```sh
./scripts/maps/build_state_pack.sh --state OK --edition full --road-scale 1m --resume
./scripts/maps/verify_state_pack.sh --state OK
```

- Check panhandle handling and east-west aspect ratio.
- Verify reservoirs are ranked rather than displayed indiscriminately.

### New Mexico — NM

```sh
./scripts/maps/build_state_pack.sh --state NM --edition full --road-scale 1m --resume
./scripts/maps/verify_state_pack.sh --state NM
```

- Prioritize numbered roads, towns, rail, and mountain silhouettes.
- Do not treat intermittent desert flowlines as dependable water sources.

### Arizona — AZ

```sh
./scripts/maps/build_state_pack.sh --state AZ --edition full --road-scale 1m --resume
./scripts/maps/verify_state_pack.sh --state AZ
```

- Inspect Grand Canyon/Colorado River geometry and central mountain relief.
- Preserve town/route orientation across large sparsely settled areas.

### Nevada — NV

```sh
./scripts/maps/build_state_pack.sh --state NV --edition full --road-scale 1m --resume
./scripts/maps/verify_state_pack.sh --state NV
```

- Sparse labels must remain useful without inventing local detail.
- Inspect Reno/Carson and Las Vegas clusters separately from central Nevada.

## Phase gate

- [ ] All five states pass standard acceptance gates.
- [ ] Texas has an explicit one- or two-region decision in its manifest.
- [ ] Desert hydrography carries a non-potability/non-reliability warning.
- [ ] Route-marker generation does not occur at render time.
- [ ] Phase measurements are recorded below.

## Completion record

| State | Status | Full MB | Lite MB | Elapsed | Notes |
|---|---|---:|---:|---:|---|
| TX | Not started | — | — | — | — |
| OK | Not started | — | — | — | — |
| NM | Not started | — | — | — | — |
| AZ | Not started | — | — | — | — |
| NV | Not started | — | — | — | — |

Phase status: **NOT STARTED**

