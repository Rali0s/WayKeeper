# Phase 09 — Pacific, Alaska, and Hawaii

## Scope

States: **California, Oregon, Washington, Alaska, Hawaii**  
Nominal timebox: **2-4 hours**  
Temporary workspace ceiling: **6 GB**

This phase is intentionally longer. California is dense; Alaska crosses the
antimeridian and cannot be one useful statewide raster; Hawaii must not be one
large ocean rectangle. Do not force this phase into a one-hour limit.

## State work cards

### California — CA

```sh
./scripts/maps/build_state_pack.sh --state CA --edition full --road-scale 1m --regions north,south --resume
./scripts/maps/verify_state_pack.sh --state CA
```

- Default to northern and southern terrain regions under one state manifest.
- Expect dense roads, towns, trails, rail, and extreme terrain variation.
- Stop for review if the Full pack exceeds 80 MB.

### Oregon — OR

```sh
./scripts/maps/build_state_pack.sh --state OR --edition full --road-scale 1m --resume
./scripts/maps/verify_state_pack.sh --state OR
```

- Inspect coast, Cascades, and high desert separately.
- Verify trail density and Columbia River geometry remain balanced.

### Washington — WA

```sh
./scripts/maps/build_state_pack.sh --state WA --edition full --road-scale 1m --resume
./scripts/maps/verify_state_pack.sh --state WA
```

- Preserve Olympic Peninsula, Puget Sound, Cascades, and eastern Washington.
- Test dense Seattle-area labels without masking water/terrain references.

### Alaska — AK

```sh
./scripts/maps/build_state_pack.sh --state AK --edition full --road-scale 1m --regions alaska --resume
./scripts/maps/verify_state_pack.sh --state AK
```

- Mandatory regional tiling: Southeast, Southcentral, Interior, Southwest,
  Arctic, and Aleutians are the starting partition.
- Normalize or split antimeridian-crossing geometry before clipping.
- Do not imply complete road, trail, or hydrography coverage in remote regions.
- Validate every region independently at 80x24 and from the state selector.

### Hawaii — HI

```sh
./scripts/maps/build_state_pack.sh --state HI --edition full --road-scale 1m --regions hawaii --resume
./scripts/maps/verify_state_pack.sh --state HI
```

- Use island groups rather than one statewide ocean-filled raster.
- Initial groups: Hawai'i, Maui Nui, O'ahu, and Kaua'i/Ni'ihau.
- Verify town and route labels remain associated with the correct island.

## National completion gate

- [ ] All five states pass standard acceptance gates.
- [ ] California uses a documented regional decision.
- [ ] Alaska has no antimeridian wrap or continent-sized empty raster.
- [ ] Hawaii uses documented island groups.
- [ ] All fifty state codes appear exactly once across phases 01-09.
- [ ] National Full and Lite extracted totals are calculated.
- [ ] Individual state checksums and archive indexes are complete.
- [ ] A clean WayKeeper install can discover any extracted state independently.
- [ ] Nothing has been uploaded or published without separate authorization.

## Completion record

| State | Status | Full MB | Lite MB | Elapsed | Notes |
|---|---|---:|---:|---:|---|
| CA | Not started | — | — | — | — |
| OR | Not started | — | — | — | — |
| WA | Not started | — | — | — | — |
| AK | Not started | — | — | — | — |
| HI | Not started | — | — | — | — |

Phase status: **NOT STARTED**

National program status: **NOT STARTED**

