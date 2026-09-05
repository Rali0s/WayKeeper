# PHASE 1 TRACKER

Started: 2026-08-30

Phase 1 begins with 25 candidates. They are not yet displayed by the live ANSI
Cards screen.

## Source allow-list

| Source | PDF pages | Candidate target | Current cross-check |
|---|---:|---:|---|
| FEMA Emergency Supply List | 2 | 10 | Ready.gov supply checklist |
| CDC Chemical Agents: Sheltering in Place | 2 | 7 | CDC Chemical Emergencies, 2026 |
| USDA Forest Service poison ivy guide | 6 | 8 | CDC/NIOSH poisonous-plant guidance |

The 2006 CDC and 2007 Forest Service PDFs are retained as the offline source
record, but no time-sensitive instruction is accepted merely because it appears
there. Current official guidance controls when language differs.

## Candidate register

| ID | Lane | Risk | State |
|---|---|---|---|
| preparedness.kit.several-days | Emergency kit | low | candidate |
| preparedness.kit.distributed | Emergency kit | low | candidate |
| preparedness.kit.power-light | Emergency kit | low | candidate |
| preparedness.kit.alert-radio | Emergency kit | low | candidate |
| preparedness.kit.first-aid-signal | Emergency kit | low | candidate |
| preparedness.kit.sanitation | Emergency kit | low | candidate |
| preparedness.kit.shelter-materials | Emergency kit | moderate | candidate |
| preparedness.kit.documents-cash | Emergency kit | low | candidate |
| preparedness.kit.medication-needs | Emergency kit | moderate | candidate |
| preparedness.kit.clothing-warmth | Emergency kit | low | candidate |
| chemical.shelter.authority-decision | Chemical shelter | high | candidate |
| chemical.shelter.safe-room | Chemical shelter | high | candidate |
| chemical.shelter.vehicle-limit | Chemical shelter | high | candidate |
| chemical.shelter.seal-pack | Chemical shelter | moderate | candidate |
| chemical.shelter.hvac-shutdown | Chemical shelter | high | candidate |
| chemical.shelter.seal-room | Chemical shelter | high | candidate |
| chemical.shelter.all-clear | Chemical shelter | high | candidate |
| plants.urushiol.identification | Poisonous plants | moderate | candidate |
| plants.urushiol.exposure-routes | Poisonous plants | moderate | candidate |
| plants.urushiol.never-burn | Poisonous plants | high | candidate |
| plants.urushiol.field-clothing | Poisonous plants | moderate | candidate |
| plants.urushiol.clean-gear | Poisonous plants | moderate | candidate |
| plants.urushiol.wash-skin | Poisonous plants | moderate | candidate |
| plants.urushiol.blister-fluid | Poisonous plants | moderate | candidate |
| plants.urushiol.emergency-signs | Poisonous plants | high | candidate |

## Promotion checklist

- [x] Mechanical validator passes (25 cards, 0 failures on 2026-08-30).
- [x] Every source page has been visually inspected.
- [x] Every material claim is supported by its cited page and cross-check.
- [x] Current official cross-check agrees with the candidate language.
- [x] High-risk cards include clear authority or emergency escalation language.
- [ ] A named human reviewer records accept, revise, merge, or reject.
- [x] Candidate bodies fit the 79-column ANSI reader (largest is 18 rows).
- [x] Candidates are visible in the isolated F1 Phase 1 review queue.
- [ ] Runtime loader/index work is complete before publication.

## Current gate

The batch is **awaiting-human-review / candidate-only**. The source and machine
checks are complete. A named reviewer must record accept, revise, merge, or
reject before any card can be marked `reviewed` or enter the runtime index.
