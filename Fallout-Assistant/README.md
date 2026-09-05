# WAYKEEPER // OFF-GRID Fallout Assistance Tech Assistant

WayKeeper is a personal, local-first ANSI survival console. The same C++ shell runs on macOS/Linux development hosts, a constrained ARM64 QEMU target, and the Orange Pi Zero 2 image profiles under `platform/`. It provides reviewed survival cards, offline PDF/text readers, maps, inventory, and journals without replacing the ANSI interface with a desktop application.

The central rule is **evidence before eloquence**:

1. Reviewed emergency card.
2. Exact indexed-manual passage.
3. Deterministic calculator or live sensor.
4. Local Qwen model for synthesis, with citations.

The model is never the source of truth for radiation, medical, water-treatment, electrical, or radio-safety decisions.

PDF passages enter the card system through a candidate, citation, safety, and
human-review gate. See the
[PDF-to-card workflow](docs/pdf-to-cards-workflow/README.md) and its
[three-phase build](docs/pdf-to-cards-workflow/THREE-PHASE-BUILD.md). Candidate
cards are isolated from the live F1 Cards surface until a named reviewer
promotes them.

## Build

macOS or Linux with CMake, Ninja, and GDAL:

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
./build/dev/offgrid-assistant status
```

Import the existing PDFs and launch the interface:

```sh
scripts/import_library.sh
scripts/run.sh
```

MSYS2/MinGW64 on Windows:

```sh
cmake -S . -B build/msys2 -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/msys2
ctest --test-dir build/msys2 --output-on-failure
```

The code uses C++20 and GDAL for offline GeoTIFF terrain and ANSI raster images. Poppler supplies PDF text extraction and page rasterization. ANSI/VT output is enabled on Windows consoles when supported and remains usable without color. Builds without GDAL retain the text assistant and report raster support as unavailable.

The application never opens a separate mascot window. Its default `AUTO`
companion mode uses the terminal-native RGB24 half-block portrait unless a custom
WayTerm host advertises inline-image support. Such a host sets
`WAYTERM_INLINE_IMAGES=1` (or `TERM_PROGRAM=WayTerm`) and handles the private OSC
777 placement request, loading the original PNG inside the existing companion
grid without moving the cursor. `OFFGRID_INLINE_COMPANION=wayterm` forces that
capability for host development; `OFFGRID_INLINE_COMPANION=ansi` disables it.
F5 / System cycles `AUTO`, `ANSI`, and `OFF`.

## ANSI operations shell

On first launch the terminal asks for a local operator name, incident type, and terrain. The profile is stored only in `state/profile.ini` and can be changed with `PROFILE`. Test and fictional-zombie profiles are visibly marked `SIMULATION`.

The command center and every list-based screen run in `INPUT LOCK`: the header,
navigation strip, selected row, and footer stay fixed while only the item
viewport changes. The interactive shell uses an alternate screen with autowrap
disabled, so it does not accumulate normal terminal scrollback. At the command
center, only Tab/Shift-Tab, Enter, Escape, arrows, paging keys, 0-9, and F1-F12
are accepted. Search and editor fields accept text only after Enter opens them.

The command center defaults to the responsive `WORKSTATION` model. At the native
640x480 target it becomes a 79-column, two-column menu with all twelve F-key
workspaces visible and no companion artwork. `Tab` and `Shift+Tab`
move focus between its menu, shared Field I/O/UART Scout, local Archive Find field,
evidence-first Guide Query, fixed-cell Schematics viewer, and display mode. Larger
development terminals can still show the wide dashboard. The WolfPup mascot remains
on the privacy-safe idle screen instead of consuming the wrist-computer menu.
The preserved `STATIC` model remains available under `F5 / SYSTEM`.

```text
Up / Down                  previous / next item
Page Down / Page Up        next / previous menu viewport
Home / End                 first / last item
Enter                      open highlighted item
1-9 / 0                    direct choices 1-9 / 10
F1-F12                     direct primary workspace
Escape                     return / exit
```

The persistent function strip routes to:

```text
F1 / CARDS                 reviewed survival cards + isolated review queue
F2 / LIBRARY               classified books + repair/agriculture + archives + Cookbook
TAB / SCHEMATICS           fixed-cell, horizontally pannable text diagrams
F3 / ASK <question>        evidence-first local Guide
F4 / PROFILE               operator and incident context
F5 / STATUS                local service status and terminal settings
F6 / MAP                   state-selectable offline terrain and route viewer
F7 / HERBS                 plants/herbs evidence search and source reader
F8 / JOURNAL               Captain's Log and survival help notes
F9 / SOCIETY               belief, civics, philosophy, economy, rebuilding, archive
F10 / INVENTORY            health, loadout, coordinates, power, and sensors
F11 / OOBE                 guided Civilization Installation + operations list
F12 / ABOUT                WayKeeper mission and credits
SEARCH <terms>             page-ranked source search
SCOUT / UART               shared Field I/O master connection
HELP                       command reference
QUIT                       close and save local state
```

F1 opens two explicit lanes. `REVIEWED / LIVE` contains only trusted cards used
by the Guide. `PHASE 1 / REVIEW QUEUE` loads the Markdown candidates under
`cards/candidates/phase-1` for visual inspection. Candidate readers show risk,
limits, PDF page, currency check, and a permanent `NOT REVIEWED / NOT LIVE`
warning; opening a candidate cannot promote it or make Guide return it.

After five minutes without keyboard input, any menu, card, reader, or prompt
locks into the public WayKeeper Sentinel display. `LOCK`, `QUIET`, and
`BLACKOUT` enter its three privacy states manually. Wake the private terminal
with `Escape`, `W`, `K`, `Enter`. Sentinel reveals only the room code and
harmless public status copy. `SPEED 0.5`, `SPEED 1`, or `SPEED 2` changes the
public ANSI-card rotation rate without changing the real-time lock timeout.

`WILL` opens the private multiline composer for an optional Last Will & Testament
found-device message. `WILL ON` adds its numbered pages to the Sentinel rotation;
`WILL OFF` hides them without deleting the local text. The feature starts off,
never appears in Blackout, and stores its control-character-safe 4096-byte
plaintext message under the Git-excluded `state/last-will.txt`. It is a public
message feature, not a mechanism for executing or validating a legal will.

UART Scout's Ghostline adapter starts a separate, forced observe-only relay for
the one authorized master target. It supports TCP or TCP-to-serial at the saved
baud, bounded GLCAP1 recovery text, DLT_USER0 PCAP, MQTT CONNACK summaries, and
an ANSI hex/ASCII reader. Use `scout ghostline status|start|stop|capture` outside
the UI; the same controls live under `[2] FIELD I/O`.

F6 first opens a state-pack selector, so states can be installed or removed
independently without growing a monolithic national database. New York and
Florida are currently installed; each combines an official USGS 3DEP preview
with offline USGS National Digital Trails and railroad overlays.
Pedestrian-accessible trail segments render with a gold
core and dark contrast halo in color terminals and as `*` in plain text. The
halo uses only a terminal-sized mask rather than another statewide raster. The
F6 viewer starts centered at `4.0×`—one level closer than the previous `2.0×`
view.
Arrow keys pan immediately; `H/J/K/L` provide Vim-style alternatives, `R`
recenters, and `Q` returns to the map list. Each pan moves 20% of the visible
area and redraws both terrain and trails. `G` records a labeled decimal-degree
GPS point using a Homestead, Compound, Town, Village, Market, Water, Food,
Abandoned Structure, or Scavenge key. `P` opens the point registry and `W`
toggles orange waypoint lines between points in insertion order. Route mileage
uses great-circle distance and overlays persist per state in
`state/map-overlays`. Zoom-out reaches the complete `1.0x` state view. Use
`F6`, then `OPEN 1`, or render the
uncropped command-line overview directly:

```sh
build/prod/offgrid-assistant map maps/fl/USGS-3DEP-Florida-State-preview.tif
```

The core state-pack budget is 48 MB per state (about 2.4 GB if all fifty packs
hit that ceiling); detailed regional topo tiles remain optional. Towns and
settlements are stored as compact points, not imagery or building footprints.
See [map storage profile](maps/STORAGE-PROFILE.md) for the packaging contract.

PDF and herb source pages open in a fixed-height terminal pager. The operations
header and F1-F12 menu are redrawn while only the source-text viewport scrolls:

```text
Space / Page Down   next viewport (then next PDF page at the bottom)
B / Page Up         previous viewport (then previous PDF page at the top)
J / K               down / up one visual line
D / U               down / up half a viewport
GG / Home           top of the current PDF page
G / End             bottom of the current PDF page
N / P               next / previous PDF source page
I                   render the exact PDF page as an ANSI image
:GOTO <page>        enter command mode and jump to an exact cited PDF page
Q                   leave the reader (`:BACK` also works)
```

The Schematics tab uses a separate allow-list under
`library/segments/Schematics/txt`. It preserves leading spaces, trailing spaces,
and eight-cell tab stops, never word-wraps a diagram, and provides horizontal
`H/L` or arrow-key panning. Its local cards include the WayKeeper operator,
UART Scout, and hardware self-repair field manuals alongside the power, console,
boot, and radio architecture maps. Historical metadata-only high-energy records
are not presented as buildable schematics.

Nearby BLE Mesh is available as a deliberately locked build milestone through
the `[2] FIELD I/O // UART + BLE` selector (or the non-interactive `mesh` CLI).
The ANSI surface, bounded profile, readiness report, and panic-wipe contract are implemented; it
does not transmit or claim bitchat interoperability until the Orange Pi/BlueZ
workflow in [`docs/WAYKEEPER-BITCHAT-BUILD-WORKFLOW.md`](docs/WAYKEEPER-BITCHAT-BUILD-WORKFLOW.md)
passes.

The Nearby BLE sidebar uses five fixed items: Power, Nickname, Radio Status,
Chat, and Back. Item 4 opens a persistent fixed-screen chat console with its
session transcript above a `BLE CHAT>` input at the bottom; `/BACK` returns and
`/CLEAR` clears the RAM-only local view. Item 5 always returns from the sidebar.
While the protocol gate is locked, text is accepted only for interface testing,
marked `LOCAL TEST / TX LOCKED / NOT SENT`, and never queued or described as
delivered. The console and BLE screen consume carried Enter keys from numeric
shortcuts so entry cannot immediately cancel chat or cycle radio power.

Interactive ANSI navigation runs in a fixed alternate-screen buffer with
terminal wrapping disabled. The command center ignores stray printable keys and
accepts Tab/Shift-Tab, Enter, Escape, arrows, 0-9, paging keys, and F1-F12.
Typing is enabled only after entering an explicit text editor or search field.

Pager keys act immediately on an interactive terminal; no Return is required.
Use `:` before a longer command. Piped/non-interactive input retains the
line-oriented command fallback. Most terminals do not distinguish Shift+Space
from Space, so `B` is the portable reverse-page binding. Survival cards and
local Guide responses use the same fixed reader viewport.

## Captain's Log and survival notes

`F8`, `JOURNAL`, or `LOG` opens a completely offline journal. `L` starts a
Captain's Log entry and `N` starts a survival help note. Each entry records its
created/updated time, kind, title, tags, operator, incident, terrain, and
multiline body. Captain's Log entries also offer optional position/location,
weather/conditions, watch/shift, sleep-hours, miles-traveled, and health-note
prompts. `H` opens a health/travel rollup tied directly to those Captain's Log
records. The fixed `WRITE LOCK` composer
accepts ordinary lines plus:

```text
.SAVE       commit the entry
.UNDO       remove the last line
.CLEAR      clear after confirmation
.CANCEL     leave without saving
```

The journal list and entry reader use the same fixed-screen scrolling controls as
the rest of the terminal. `S` searches titles, tags, context, and bodies. `E`
edits an active entry. `A` archives after an explicit confirmation; archived
entries remain available under `A ARCHIVE` and can be restored with `R`.
Nothing is sent to Ollama or the network. Entries are human-readable files under
`state/journal`; local state remains excluded from Git.

Press `I` on any PDF or herb source page to render that exact page as a cached,
true-color ANSI image. The image viewer uses arrow keys or `H/J/K/L` to pan,
`+/-` to zoom, `R` to reset, and `O` to hand the original PDF to macOS. This is
useful for diagrams, firearm-care illustrations, maps embedded in manuals, and
real reference photography that would be lost in extracted text. Under `F7`,
`I` opens dedicated poison ivy, poison oak, and poison sumac image cards sourced
from a US Forest Service PDF. They remain reference photos—not a positive plant
identification or permission to consume or handle a plant.

The shell requests the Waveshare target's native `640x480` terminal window on launch.
`F5 / SYSTEM` provides larger development presets and validated custom sizes, plus a persistent
resize-on-relaunch toggle. These settings are stored locally in
`state/settings.ini`. ANSI window resize is a request; terminal applications may
choose to ignore it.

See [ANSI shell design](docs/ANSI_SHELL.md) for the interaction model.

## Ollama (retained, disabled)

The legacy loopback-only Ollama adapter remains in source for archival/desktop experimentation, but `WAYKEEPER_ENABLE_OLLAMA` defaults to `OFF` and both embedded image profiles force it off. They install no Ollama package, model, service, or listener. Evidence search and reviewed cards continue to work without inference.

A non-embedded developer must deliberately opt in at configure time before the old setup script can be used:

```sh
cmake -S . -B build/ollama -DWAYKEEPER_ENABLE_OLLAMA=ON
cmake --build build/ollama
ollama serve
scripts/setup_ollama.sh
```

The default model is `qwen3:4b`. Override it with `OFFGRID_OLLAMA_MODEL`. The Guide searches the local manual text first, displays the evidence, and asks permission before spending battery power on inference.

## First working commands

```sh
offgrid-assistant ask "How long should I leave water purification tablets in for?"
offgrid-assistant cost 12 8 300 180 25
offgrid-assistant parse 'OGP1|solar|battery_voltage|13.42|V|1786766400000|measured'
offgrid-assistant scout ports
offgrid-assistant scout protocols
```

Energy figures must come from a calibrated power profile for the actual computer, model, context size, screen state, and ambient conditions. They are engineering estimates, not battery guarantees.

See [Phase A](docs/PHASE_A.md), [architecture](docs/ARCHITECTURE.md), [UART Scout](docs/UART-SCOUT.md), [corpus plan](docs/CORPUS_PLAN.md), and [wellbeing policy](docs/PERSONALITY_AND_WELLBEING.md).

## Plants and herbs evidence database

`library/plants-herbs` contains a separate safety-ranked corpus for future plant
identification and medicinal-herb cards: 10 official WHO, USDA, and Upstate New
York Poison Center PDFs; 2,266 page-aligned text records; checksums and source
provenance; and a full-text SQLite database with structured tables for botanical
identity, evidence, toxic lookalikes, contraindications, interactions, and
contamination. Imported claims are not approved as treatment instructions.

```sh
scripts/search_plants_herbs_db.py "poison hemlock"
scripts/search_plants_herbs_db.py "willow"
build/prod/offgrid-assistant herbs "willow contraindications"
```

See [plants and herbs database](library/plants-herbs/README.md) for safety rules,
source acquisition, and the next review stage.

## Fieldcraft PDF pack

Run the idempotent downloader to install or verify 20 official/institutional
fieldcraft PDFs (1,097 pages): hunting, trapping, skinning and pelt preparation,
game-food safety, fishing, fly fishing, flint-and-steel fire building, firearm
safety/care, bear and wolf safety, wolf/coyote identification, and poison-plant
photography. Shelter coverage progresses from debris beds, A-frames, lean-tos,
and cold-weather shelter to log construction, cabin durability, and decay
prevention.

```sh
scripts/download_fieldcraft_library.sh
```

`library/fieldcraft-sources.tsv` is the acquisition manifest;
`library/fieldcraft-provenance.tsv` records SHA-256, verified page count,
download date, source URL, publisher, and safety note. Hunting and fishing
regulation documents are visibly marked in the reader because seasons, limits,
licenses, and laws change. The existing Army survival manual supplies the wider
lighter, flint, bow-drill, and hand-drill fire progression. Shelter pages carry
separate site-hazard or permanent-structure warnings; historical cabin manuals
do not replace current codes, permits, engineering, fire safety, or snow-load
review.

## Society source pack

Run the idempotent Society importer to add the six-source `F9` collection:

```sh
scripts/download_society_library.sh
```

The collection separates the King James and Catholic Douay-Rheims Bibles,
preserves Eliphas Levi under his correct title, combines the U.S. founding
papers, adds the current official Government Manual, and archives KUBARK as a
declassified historical abuse record. Title 10 is not used as a general
government manual because it concerns the Armed Forces. See
[Society library](library/SOCIETY.md) for provenance and safety boundaries.

The Philosophy branch adds 197 non-card reader entries under the recommended
ten-shelf F9 structure. Public-domain Project Gutenberg editions are downloaded
and adapted for the ANSI pager; uncertain editions remain review records, and
modern copyrighted books remain visible as license-required bibliography
entries without copied text. Run `scripts/download_philosophy_library.py` and
see [Philosophy library](library/PHILOSOPHY.md) for the shelf and rights model.

`F12`, `ABOUT`, or `CREDITS` opens the WayKeeper mission statement and complete
production credits in the same fixed-screen reader. On macOS it also starts the
seven-track `RES/Music` wasteland-radio playlist, loops it only while the Credits
screen remains open, and stops playback immediately on return. The album cover
is deliberately omitted from the 640x480 reader so text receives the full viewport.

`F11`, `OOBE`, or `CIV` opens the ANSI Civilization Installation console. Its
preflight can launch the installer into the local WayKeeper state directory,
view an existing installation summary, or open the generated public
`OPERATIONS-LIST.txt`. Every installation also receives a machine-editable
`var/lib/waykeeper/operations/operations.tsv` for later automation.

F9 now also contains separate Survival Economy (including barter and money),
Rebuilding Society, and TEXTFILES Underground branches. The first two add ten
open or expressly reusable FEMA, NIST, and Federal Reserve PDFs for barter,
fair measurement, value comparison, continuity, lifelines, recovery, and
community resilience. The underground branch exposes 21 curated local readers
and a pointer to the existing 33,009-file archive without polluting reviewed
Guide search with the entire unverified corpus. See
[Recovery and economy](library/RECOVERY-ECONOMY.md) and
[TEXTFILES archive](library/TEXTFILES.md).

The opening sequence now uses a 100-column WK-01 ANSI instrument panel with a
30 x 18 WayKeeper raster beside live boot-state indicators. The image pipeline
uses 24-bit foreground/background RGB and upper-half-block cells to carry two
vertical image pixels per terminal cell, exceeding an 8-bit/256-color palette;
the dashboard chrome itself uses ANSI-256, `OFFGRID_COLOR_MODE=256` quantizes
the mascot to strict 8-bit terminal color, and plain terminals receive a
grayscale fallback. Incident profile
changes trigger a high-resolution half-block ANSI mascot: Vault-Blue for
Nuclear/Radiological, Zombie-Fallout for the fictional Zombie mode, and Survival
Mode for every other incident. F4 also shows a compact active-mode mascot.

F2 now opens a collection hub with the complete document library, a tiered
`Technical Workshop`, Present-Day Prep, Zombie Drill, and a separate `Cookbook`
shelf. Present-Day Prep covers emergency/hurricane/recovery kits, distributed
home-work-vehicle kits, lawful secure firearm/ammunition storage, cartography,
and a buy/rotate/store field guide. Zombie Drill is explicitly fictional but
uses real safe-room, shelter-in-place, wind-retrofit, and defensive facility
assessment material for storms, chemical releases, looting, and civil disorder.
Its arms content is limited to safe storage and theft prevention. Run
`scripts/download_prep_security_library.sh` to rebuild 8 official PDFs, local
reader text, checksums, page counts, safety notes, and rights notes; see
`library/PREP-SECURITY.md` and `library/prep-security-provenance.tsv`.

The workshop contains
the complete 24-module Navy Electricity and Electronics Training Series plus
11 vehicle and shop references covering cars, SUVs, trucks, heavy equipment,
hydraulics, tires, construction mechanics, aerial lifts, and bucket trucks.
Run `scripts/download_technical_library.sh` to rebuild its verified PDFs,
searchable text, checksums, and catalog entries; see `library/TECHNICAL.md` and
`library/technical-provenance.tsv` for scope and provenance. The final workshop
shelf contains a compact survey of large product-manual archives and the
recommended catalog-first/selective-cache strategy.

The workshop now also opens a 15-shelf Agriculture + Horticulture field
library. Its 26 validated sources cover soil, seed, crops, orchards, water,
controlled growing, IPM, tools, livestock, pasture, calendars, New York,
storage, and planning. Run `scripts/download_agriculture_library.sh` to rebuild
the PDF, text, catalog, checksum, safety, and rights records; see
`library/AGRICULTURE.md` and `library/agriculture-provenance.tsv`.

`ONLINE MANUAL RESOURCES` launches a network-aware discovery console for
Internet Archive Manuals, iFixit, ManualsLib, and ManualsOnline. It searches
official APIs where available, hands off provider search where scraping would
be brittle, and can stage metadata links in
`state/manual-resource-queue.tsv`. Bulk download is deliberately absent: no
manual content is fetched until a later source-by-source rights, revision,
model, safety, and checksum review.

`scripts/import_cookbook.py` registers the local 336-page
`RES/*Anarchist*Cookbook*.pdf` as a restricted underground archive. Its source
text is deliberately excluded from Guide and DeepSearch because it includes
illegal, violent, and potentially lethal material; deliberate page-image or
external viewing requires an acknowledgement. See `library/cookbook-provenance.tsv`.

Reader-triggered one-shot boot easter eggs are stored in
`state/next-boot-unlock.ini`. Opening the regulated moonshining/distilling reader
arms `Raider-Waykeeper.png`; opening the Cookbook record arms
`Vault-Tec-WayKeeper-Easteregg.png`. The most recently opened qualifying reader
wins, the image appears after the WK-01 boot panel on the next launch, and the
pending state is removed after that display. F5 reports whether a next-boot egg
is armed.
