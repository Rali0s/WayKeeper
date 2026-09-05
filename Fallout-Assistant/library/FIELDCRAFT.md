# Fieldcraft offline pack

The fieldcraft pack adds 20 source PDFs and 1,097 page-aligned text pages to the
main `F2` library. Run `scripts/download_fieldcraft_library.sh` to download,
verify, extract, and merge the pack. The script is idempotent.

## Coverage

| Need | Offline sources |
|---|---|
| Hunting and current-law orientation | NYSDEC hunting/trapping guide; Today's Hunter in New York; TPWD hunter education manual |
| Trapping, skinning, and pelt preparation | NYSDEC Trapper Education Manual; TPWD field-dressing leaflet |
| Cooking and safe game preparation | Ohio State venison guide; Texas A&M wild-game packaging/storage; TPWD beginning angler guide |
| Fishing and fly fishing | NYSDEC freshwater regulations; TPWD beginning angler and fly-fishing manuals |
| Fire building | California State Parks flint-and-steel manual; existing Army survival manual for lighter, flint, bow drill, and hand drill |
| Firearm and rifle safety/care | Today's Hunter in New York; TPWD hunter education manual |
| Bears, wolves, and other wildlife | Alaska wildlife-safety and wolf-country booklets; NYSDEC wolf/coyote photo guide |
| Poison ivy, oak, and sumac | US Forest Service identification and exposure guide with source photography |
| Emergency shelter and lean-tos | UAF/NOAA outdoor-survival manuals; existing Army survival manual for poncho, field-expedient, snow, swamp-bed, and debris shelters |
| Log cabins and cold-climate building | Alaska Housing Finance Corporation log guide; USFS log-building and decay manuals; UAF cold-climate building overview |

Useful visual anchors:

- Today's Hunter PDF page 29: illustrated firearm-cleaning sequence.
- NYSDEC Trapper Education PDF page 95 onward: cased-fur skinning.
- USFS poison-plants PDF page 2: eastern and western poison ivy.
- USFS poison-plants PDF page 3: poison oak and poison sumac.
- Army survival PDF pages 126–131: prepared fire starters, flint/steel,
  bow drill, and hand drill.
- Army survival PDF pages 140–149: poncho lean-to, snow shelters,
  field-expedient lean-to, swamp bed, and debris hut.
- UAF/NOAA outdoor-survival PDF page 35: emergency debris-hut lesson;
  page 128: illustrated student build sequence.
- Alaska Log Building Construction Guide PDF page 28: illustrated foundation
  anchoring options; page 40: log-scribing sequence.
- USFS *Building with Logs* is a scanned visual manual; use `I` to read its
  drawings in the ANSI image viewer.

Open a page in `F2`, press `I`, and use the ANSI image viewer when text
extraction cannot preserve the diagram or photograph. Under `F7`, press `I` for
the three poison-plant image cards.

## Safety and currency

- Hunting, trapping, fishing, firearm, and fire rules vary by place and change
  over time. Current law and closures override every downloaded manual.
- Follow the exact firearm manufacturer's current manual. Unload and clear the
  firearm, remove ammunition from the work area, and stop if the procedure is
  not understood.
- Wildlife avoidance and species-specific agency guidance come first. A firearm
  is not a generic bear-response plan.
- Field-dressing and cooking references do not make diseased, contaminated, or
  temperature-abused meat safe.
- One photograph never establishes a certain plant identification. Never eat or
  handle a plant based only on an image card.
- Improvised shelter instructions are conditional on site and weather. Check
  deadfall, flood, avalanche, drainage, fire, ventilation, and exposure risks.
- Cabin manuals are reference material, not approved plans. Current codes,
  permits, engineering, fire clearances, foundations, and snow/wind/seismic
  loads control permanent construction.

Acquisition inputs are in `fieldcraft-sources.tsv`. Verified SHA-256 values,
page counts, dates, publishers, URLs, and source-specific cautions are in
`fieldcraft-provenance.tsv`.
