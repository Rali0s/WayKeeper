# ANSI Survival Operations Shell

## Visual language

The console borrows the information density and hierarchy of professional market terminals without imitating a specific product:

- the selected Blue, Gold, or Green field theme identifies commands, labels, navigation, and evidence hierarchy;
- green means a local service is reachable;
- red means offline, active incident context, or a safety stop;
- white is primary operational data;
- gray is instruction and provenance metadata.

It uses standard ANSI/VT sequences, a fixed-width layout, a mascot startup mark, and a plain-text fallback. No GUI framework is required by the shell. The embedded Full profile hosts it in a minimal Wayland kiosk only to support touchscreen input; Lite stays on tty/UART.

The persistent header includes a 1–16 character room code, theme name, and battery rail. Battery percentage is read from `WAYKEEPER_BATTERY_PERCENT` or Linux `/sys/class/power_supply`; systems without a readable gauge show external power rather than inventing a charge value. The room code also appears beside the active WayKeeper mascot.

The `[2] FIELD I/O // UART + BLE` selector opens the Nearby BLE Mesh engineering
milestone alongside UART Scout. The non-interactive CLI retains `mesh` and
`chat` commands for scripts and diagnostics.
The current build exposes readiness, requested ECO/ACTIVE/OFF profiles, nickname,
privacy notes, and exact-state panic wipe. Radio transmit remains visibly locked
until `WAYKEEPER-BITCHAT-BUILD-WORKFLOW.md` passes on Orange Pi hardware.

The interactive shell enters the terminal alternate-screen buffer and disables
autowrap until exit, keeping the board out of normal scrollback. Navigation input
is locked to Tab/Shift-Tab, Enter, Escape, arrows, Home/End, Page Up/Page Down,
0-9, and F1-F12. Stray printable keys are ignored at the command center. Full
typing is accepted only inside a deliberate text field, search, composer, or
editor.

Desktop development builds use Escape at the command center to exit. Orange Pi
Full and Lite images compile in appliance mode: Escape still returns from child
screens, but Escape or any textual exit command at the command center enters
Sentinel instead of terminating to a Linux shell. The board's systemd unit also
uses `Restart=always`. Maintenance remains available through authenticated SSH
or by stopping the service from an authorized console; it is not exposed through
the wearer-facing ANSI interface.

## Sentinel privacy lock

Every interactive prompt carries a real-time five-minute dead-man timer. Any key
press resets it, including navigation inside a survival card, reader, menu, or
data-entry form. When it expires, the current screen is unwound and replaced by
the public `SENTINEL` display. No operator name, condition, inventory, location,
map, battery percentage, network state, notification, filename, or recent
activity is rendered there.

`LOCK` or `SENTINEL` enters the standard public display immediately. `QUIET`
shows a resting unit and `BLACKOUT` shows only a sealed-terminal mark and room
code. The hidden wake chord is `Escape`, `W`, `K`, `Enter`, in that order. It is
documented inside the private Help/System screens but is never printed on the
public display. The lock is a casual privacy barrier, not disk encryption or
strong authentication.

Sentinel rotates three harmless ANSI public cards. `SPEED`, `SPEED 0.5`,
`SPEED 1`, and `SPEED 2` control that movie rate; the same setting is available
under `F5 / SYSTEM` and persists in `state/settings.ini`. At normal speed each
card lasts 12 seconds (24 seconds at 0.5x and 6 seconds at 2x), safely below five
minutes. Movie speed never changes the five-minute security timeout.

An optional Last Will & Testament card can be written with `WILL` or `WILL EDIT`
and published with `WILL ON`; `WILL OFF` removes it from the public rotation
without deleting the locally saved text. The same edit and toggle controls are
available under `F5 / SYSTEM`. It is disabled by default, requires a non-empty
message before it can be enabled, and is never shown in Blackout mode. Longer
messages are divided into numbered cards so the complete text can rotate across
the lockscreen. The plaintext source is stored at `state/last-will.txt`, limited
to 4096 bytes, excluded from Git, and rejected if it contains terminal-control
characters. This is an opt-in found-device message; it does not execute, witness,
or establish the legal validity of a will.

## Command-center models

The default `WORKSTATION` model adds a fixed focus rail above the command center. `Tab` advances
through Navigation, Field I/O, Archive Find, Guide Query, and Minimal; `Shift+Tab` moves backward. Field
I/O opens the UART/Nearby Chat selector. When a search field
has focus, press Enter and type a query. Archive Find routes into the local
indexed library. Guide Query routes through the existing evidence-first Guide. Neither field sends
data to the network by itself.

At 640x480 the workstation becomes a fixed 79-column shell: a compact header,
six-cell tool rail, and two-column menu keep all twelve F-key workspaces visible.
Essential HP/H2O/food/battery/solar/radiation/network status remains on screen.
The command-center companion and three-panel dashboard are omitted; WolfPup is
reserved for the privacy-safe idle display. Larger development terminals can
still use the expanded workstation. `STATIC` remains selectable under `F5 / SYSTEM`.

At launch the shell clears the fixed alternate screen before drawing a text-only
640x480 diagnostic splash. The compact profile limits every physical line to the
detected LCD width. Progress fills are green, warnings are red, section labels use
gold/green backgrounds, and structural borders remain cyan.

The public idle screen derives its mascot size from the terminal, preserves the
WolfPup source aspect ratio, and exposes no private archive content.

See `docs/BLOOMBERG_WORKSTATION_RESEARCH.md` for the design research and language conclusions.

## First-run record

The terminal asks for:

1. Operator name.
2. Incident: Test, Nuclear, Famine, Zombie drill, Outbreak, Flood, Wildfire, Earthquake, Severe Weather, Grid Failure, or Other.
3. Terrain: Urban, Suburban, Rural, Forest, Mountain, Desert, Tundra, Coastal, Wetlands, or Other.

Zombie is always labeled a fictional drill. Test and fictional profiles show a cyan `SIMULATION` badge. The profile provides relevance context; it does not assert that an emergency exists or determine the user's physical location.

## Shell grammar

The home prompt follows this form:

```text
OG/<operator>@<terrain>:<incident>>
```

The command center, reviewed-card menu, PDF catalog, PDF search results, profile
actions, map catalog, Herbs home/search results, and first-run incident/terrain
selectors share one `INPUT LOCK` interaction layer. The active row is highlighted
in cyan. `Up/Down` move one item; `Page Down/Page Up` move one viewport;
`Home/End` select the first and last items; `Enter` opens; and `Escape` returns.
The selected PDF or Herb search result gets a bounded
three-line preview. Every action clears and redraws the header, visible rows,
selection status, and footer, so content never pushes navigation into terminal
scrollback.

Physical F1-F12 keys and common terminal navigation sequences are accepted.
Stray printable keys at the command center are discarded without emitting an
error line. Explicit text fields retain normal typing. Piped input retains the
line-oriented command fallback for automation and tests.

`F6` or `MAP` opens the offline GeoTIFF catalog; the selected terrain renders as ANSI true-color half-block cells with a plain-text fallback. The live terrain view removes the global header and F-key rail, leaving only one compact map identity row so the raster receives the LCD's vertical space. It opens centered at `4.0x` and can zoom out to the complete `1.0x` state view. Physical arrow keys pan immediately by 20% of the visible area; `H/J/K/L` are equivalent, `R` recenters, and Escape returns to the map list. View bounds are clamped to the source raster. Offline USGS layers add single-cell gold trails (`*`), cyan rail (`#`), pale roads (`=`), blue water (`~`), red town icons (`@`), and named lake icons (`O`). `N` opens a collision-managed list of the named features actually visible in the current map view and their quadrants. This orientation workflow requires no GPS: match signs, water, routes, and terrain. `M` optionally records a manual coordinate from a paper map or known survey point, `P` manages saved field marks, and `W` toggles orange waypoint lines. User overlays are compact per-state TSV files under `state/map-overlays`. `F7` or `HERBS` searches the separate safety-ranked plant database and preserves exact source-PDF page citations.

`I` on either PDF reader rasterizes the exact source page through Poppler, caches
the PNG under `tmp/pdf-page-images`, and renders it through the same GDAL-backed
true-color half-block engine used by maps. Arrows or `H/J/K/L` pan, `+/-` zoom,
and `R` resets. This keeps diagrams and photographs inside the fixed reader. `F7`, then
`I`, opens real US Forest Service poison ivy/oak/sumac photo cards with a visible
identification safety gate.

Both PDF readers use a Nano/Vim-style, terminal-sized viewport rather than
printing an entire extracted page. Each action clears and redraws the shared
header, source identity, visible text lines, progress percentage, and command
footer. `Space` or Page Down moves forward; `B` or Page Up moves backward;
`J/K` move one visual line; `D/U` move half a viewport; `GG/Home` and `G/End`
jump to the top and bottom. `N/P` change the source PDF page and `Q` returns to
the library. These keys act immediately on an interactive terminal. `:` opens
line-oriented command entry for `:GOTO 51`, `:OPENPDF`, or `:BACK`; piped input
continues to accept those commands without the colon. Page Down at the end continues at the next PDF page;
Page Up at the beginning opens the bottom of the previous page. Since terminals
normally encode Space and Shift+Space identically, `B` is the portable reverse
binding. Reviewed survival cards and generated local Guide responses also use
this fixed reader viewport, including the same line, half-page,
full-page, top, bottom, and return controls.

The Guide always exposes the evidence set before inference and asks permission before running Ollama. Missing evidence causes a visible safety stop rather than an improvised answer.

`F8`, `JOURNAL`, or `LOG` opens the local Captain's Log and survival-note
system. The journal list and readers remain inside the fixed-screen shell. `L` creates a
Captain's Log with structured sleep, daily-mileage, and condition fields; `H`
opens their health/travel rollup. `N` creates a survival help note, `S` searches, `E` edits, and
`A` switches to the reversible archive. The fixed `WRITE LOCK` composer redraws
the latest lines and recognizes `.SAVE`, `.UNDO`, `.CLEAR`, and `.CANCEL`.
Entries snapshot operator/incident/terrain context and are stored as inspectable
plaintext under `state/journal`; they never require the model or network.

## Window state

On startup the shell sends an ANSI pixel-resize request for the native 640x480 LCD.
`F5 / SYSTEM` persists a preset or custom 640x480–3840x2160 size and the
resize-on-relaunch toggle in `state/settings.ini`. A terminal emulator is free
to ignore window-management sequences. At roughly 80x24 cells, the shell switches
to the two-column command center and compact per-screen headers automatically.
Viewport budgets keep every fixed footer visible on the wrist display.

WayKeeper never opens a separate companion window. `F5 / SYSTEM` cycles the
companion between `AUTO`, `ANSI`, and `OFF`. In `AUTO`, ordinary terminals, SSH,
and tty sessions use the terminal-native RGB24 half-block portrait. A custom
WayTerm host can advertise `WAYTERM_INLINE_IMAGES=1` or `TERM_PROGRAM=WayTerm`;
the shell then emits a private OSC 777 request containing the percent-encoded PNG
path and requested cell geometry. The host renders that original RGB32 image at
the current cursor without moving it and keeps it behind subsequent cell text.
`OFFGRID_INLINE_COMPANION=wayterm` forces this contract during host development;
`OFFGRID_INLINE_COMPANION=ansi` disables inline-image negotiation.

## Personal-use state

`state/profile.ini`, `state/settings.ini`, `state/last-will.txt`, and
`state/journal` are local and excluded from Git. There is no login, analytics,
remote location service, journal upload, or profile upload. Sellable-product
concerns such as multi-user state, encryption, migration, consent screens,
packaging, and privacy notices remain deferred.
