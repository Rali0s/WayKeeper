# WayKeeper Body System MK II

## DIY Prototype Build Specification and Bill of Materials

**Revision:** 0.1  
**Date:** August 16, 2026  
**Purpose:** Body-retained, offline Linux/ANSI survival archive with an inward-folding flexible display, permanent keyboard, vest-mounted power, and optional walking-energy harvester.

> This document describes a buildable prototype assembled from purchasable modules. It is not a certified medical, protective, waterproof, impact-rated, or intrinsically safe device.

---

## 1. Recommended prototype architecture

### Wrist module

- Raspberry Pi Zero 2 W running Raspberry Pi OS Lite 64-bit.
- 256 GB microSD archive with a second mirrored card stored in the vest.
- 5.1–6.67 inch flexible AMOLED in an inward-folding, large-radius screen cassette.
- PiSugar 3 1200 mAh UPS for graceful shutdown and temporary operation after a vest-power disconnect.
- M5Stack CardKB mounted on a permanent hinged plate on the inner forearm.
- Five low-profile chord keys on the outer side rail.
- Directional scroll encoder in place of the custom trackball for the first prototype.
- Small monochrome status display visible while the primary screen is closed.

### Vest module

- Voltaic V50 48 Wh protected, always-on battery in a padded utility pocket.
- Fused 5 V power harness routed inside the vest and sleeve.
- Deliberate emergency breakaway connector beneath a protected flap.
- Optional tamper loop, buzzer, and encrypted-storage lock trigger.
- Optional suspended-mass generator cartridge added only after the base system works.

### Target physical envelope

| State | Target dimensions |
|---|---:|
| Wrist terminal, closed | 117 × 84 × 32 mm / 4.6 × 3.3 × 1.25 in |
| Primary display, opened | approximately 168 × 84 mm / 6.6 × 3.3 in |
| Inner keyboard module | 88 × 54 × 5 mm / 3.46 × 2.13 × 0.20 in |
| Vest battery | 118 × 82 × 24 mm / 4.65 × 3.23 × 0.94 in |
| Estimated wrist mass | 250–350 g / 8.8–12.3 oz |
| Vest battery mass | 368 g / 13 oz |

---

## 2. Core bill of materials

Prices are current reference prices as of the revision date, before tax and shipping. Budget values are used where an exact vendor price or configuration is not fixed.

| Subsystem | Part | Qty. | Unit cost | Extended | Source/status | Notes |
|---|---|---:|---:|---:|---|---|
| Computer | Raspberry Pi Zero 2 W | 1 | $15.00 | $15.00 | [Raspberry Pi](https://magazine.raspberrypi.com/articles/review-raspberry-pi-zero-2w) | 1 GHz quad-core, 512 MB RAM, 65 × 30 mm. |
| Wrist UPS | PiSugar 3, 1200 mAh | 1 | $39.99 | $39.99 | [PiSugar](https://www.pisugar.com/products/pisugar-3-raspberry-pi-zero-battery) | UPS, RTC, watchdog, safe shutdown, 5 V/3 A maximum. |
| Archive | 256 GB A2/U3 microSD | 1 | $30.00 budget | $30.00 | [Samsung PRO Plus reference](https://www.samsung.com/us/computing/memory-storage/memory-cards/pro-plus---adapter-microsdxc-256gb-mb-md256ka-am/) | Archive is primarily read-only. Buy from an authorized retailer. |
| Archive redundancy | Second matching 256 GB card | 1 | $30.00 budget | $30.00 | Same as above | Store a verified mirror in the vest. Strongly recommended. |
| Flexible display | DFRobot 6.67-inch flexible AMOLED HDMI kit | 1 | $199.00 | $199.00 | [DFRobot DFR1262](https://www.dfrobot.com/product-3113.html) | Includes HDMI controller; page currently lists it out of stock/backorder. Bendable, not confirmed dynamic-fold rated. |
| Display fallback | Gesight 5.1-inch flexible AMOLED | 1 alt. | $205.27 | — | [Gesight BF051FBM-AK0](https://gesight.com/product/5-1-inch-flexible-display-720x1520-mipi-dsi/) | Buy instead of DFRobot. Requires MIPI/HDMI controller quotation. |
| Main keyboard | M5Stack CardKB v1.1 | 1 | $7.95 | $7.95 | [M5Stack](https://shop.m5stack.com/products/cardkb-mini-keyboard-programmable-unit-v1-1-mega8a) | 50 keys, I²C 0x5F, 88 × 54 × 5 mm, 14.3 g. |
| Navigation | Adafruit ANO directional encoder with I²C adapter | 1 | $13.95 | $13.95 | [Adafruit](https://www.adafruit.com/product/5740) | Scroll, directional input, and button press. Easier to source than a miniature trackball. |
| Closed-status display | Adafruit 1.3-inch 128×64 OLED breakout | 1 | $19.95 | $19.95 | [Adafruit](https://www.adafruit.com/category/product/938) | Shows READY, power, clock, radiation/sensor status, and tamper state. |
| Chord rail switches | Kailh CHOC low-profile switches, 10-pack | 1 | $11.95 | $11.95 | [Adafruit](https://www.adafruit.com/product/5113) | Use five now; keep five spares. |
| Chord switch PCBs | NeoKey CHOC socket breakouts | 5 | $1.75 | $8.75 | [Adafruit](https://www.adafruit.com/product/5756) | Simplifies replacement and hand wiring. |
| Chord keycaps | CHOC keycap pack | 1 | $3.75 budget | $3.75 | Adafruit accessory | Five-key binary/chord entry. |
| Display cable | Mini-HDMI adapter/cable | 1 | $12.00 budget | $12.00 | Maker supplier | Keep the cable short and strain-relieved. |
| I²C integration | Grove/STEMMA cables and bidirectional level shifter | 1 lot | $12.00 budget | $12.00 | Maker supplier | Verify CardKB logic levels before direct connection. |
| Vest battery | Voltaic V50, 48 Wh | 1 | $74.00 | $74.00 | [Voltaic](https://voltaicsystems.com/v50/) | Always-on 5 V/2 A output, 3 A maximum, protected and solar-compatible. |
| Power harness | 22 AWG silicone wire, inline 2 A fuse, XT30 or locking connector, protected breakaway | 1 lot | $25.00 budget | $25.00 | Electronics supplier | Route inside sleeve; fuse close to the battery. |
| Power switching | Load switch/MOSFET for primary display | 1 | $8.00 budget | $8.00 | Electronics supplier | Completely turns off the 3 W primary display when folded. |
| Tamper functions | Hall switch, conductive loop, piezo buzzer, vibration motor | 1 lot | $18.00 budget | $18.00 | Maker supplier | Optional for the first electronics test; recommended for wearable build. |
| Cuff | Nylon webbing, hook-and-loop, neoprene, hidden dual-release buckle | 1 lot | $25.00 budget | $25.00 | Outdoor/sewing supplier | Device remains retained in normal use but has deliberate emergency release. |
| Enclosure | ASA/PETG, TPU bumpers, threaded inserts, screws, gasket cord | 1 lot | $50.00 budget | $50.00 | Self-printed | Outsourced printing may add $75–$150. |
| Vest | Existing weighted vest or MOLLE-style training vest | 1 | $60.00 budget | $60.00 | Outdoor supplier | Battery needs its own padded, ventilated pocket separate from metal weights. |
| Bench consumables | Perfboard, headers, connectors, heat-shrink, solder, adhesive | 1 lot | $30.00 budget | $30.00 | Electronics supplier | Allow spares for cable failures and rework. |

### Core prototype total

| Configuration | Estimated cost |
|---|---:|
| One archive card, self-printed enclosure | **$630–$700** |
| Two archive cards, better harness and spare parts | **$700–$800** |
| Outsourced enclosure and hinge components | **$800–$950** |

The estimate assumes a $199–$240 flexible display kit. A custom, dynamically fold-rated display and controller can move the prototype above $1,000.

---

## 3. Optional kinetic-harvester experiment

Do not make the battery the moving mass. Keep the battery rigidly protected and use a normal training plate or separate suspended carriage to drive the generator.

| Part | Qty. | Cost | Source/notes |
|---|---:|---:|---|
| Pololu 47:1 25D 12 V metal gearmotor | 1 | $29.95 | [Pololu](https://www.pololu.com/product/3229/resources); use as a brushed generator. |
| Pololu 5 V/3 A step-up/step-down regulator | 1 | $17.95 | [Pololu S13V30F5](https://www.pololu.com/product/4082/specs); 2.8–22 V input. |
| Schottky bridge/rectifier, fuse, TVS, capacitors | 1 lot | $15 budget | Converts and smooths generator output. |
| GT2 belt, pulleys, one-way bearing or mechanical rectifier | 1 lot | $25–$45 | Converts vertical carriage motion to generator rotation. |
| Linear rails/bushings and springs | 1 lot | $30–$60 | Tune around normal walking cadence. |
| Printed carriage and protective generator shell | 1 lot | $20–$35 | Keep moving mass away from the battery and spine. |

**Experimental subsystem total:** approximately **$140–$200**.

Expected first-prototype output is only **1–3 W average** after tuning. Treat anything higher as a test result, not a design assumption. Add the generator only after the vest, battery, cable, UPS, and terminal operate reliably.

---

## 4. Electrical layout

```text
OPTIONAL SUSPENDED GENERATOR
    12 V brushed gearmotor
             |
      rectifier + TVS
             |
     smoothing capacitor
             |
    regulated 5.1 V output
             |
             +----------------------+
                                    |
48 Wh VEST BATTERY <----------------+
  always-on 5 V USB-A
             |
      2 A inline fuse
             |
 internal 22 AWG sleeve cable
             |
 protected emergency breakaway
             |
       PiSugar 3 input
        /           \
 Pi Zero 2 W     4 Wh wrist UPS
      |
      +-- mini HDMI --> AMOLED controller --> foldable screen
      |
      +-- GPIO power switch --> AMOLED controller power
      |
      +-- I2C bus --> CardKB
      |           --> status OLED
      |           --> directional encoder
      |           --> PiSugar RTC/battery data
      |
      +-- GPIO --> five chord keys
              --> tamper loop
              --> buzzer/haptic alert
```

All I²C addresses must be checked before final wiring. Keep primary-display power separate from display data so Linux can switch the panel completely off when the enclosure closes.

---

## 5. Expected power budget

| Operating state | Estimated draw |
|---|---:|
| Pi boot, display opening | 6–8 W transient |
| Primary AMOLED open, ANSI interface active | 4.8–6.0 W |
| AMOLED closed/off, Pi available | 1.5–2.5 W |
| Deep idle with radios disabled | approximately 1–2 W target |

With roughly 40 Wh useable from a nominal 48 Wh pack:

- Continuous open-display operation: approximately **7–8 hours**.
- Mixed operation with the screen closed most of the time: approximately **10–16 hours**.
- The 1200 mAh wrist UPS: approximately **30–60 minutes with the main screen on**, longer with it off.

Do not plan runtime from mAh alone; use watt-hours and measure the finished system at the vest battery output.

---

## 6. Mechanical design rules

1. **No sharp OLED crease.** Use an inward teardrop hinge that maintains the panel manufacturer's specified dynamic bend radius.
2. **Treat the display as a cartridge.** The panel, protector, FPC, and hinge liner should be removable without dismantling the computer.
3. **Keep sweat away from the bare panel.** The DFRobot module explicitly has no IP rating. Seal the closed cavity and add a replaceable perimeter gasket.
4. **Do not laminate an unknown cover directly to the OLED.** Obtain the manufacturer's approved cover-film stack and neutral-axis instructions.
5. **Separate impact structure from electronics.** Rubber corners and a rigid magnesium/aluminum or glass-filled polymer plate should take impacts, not the display or Pi PCB.
6. **Protect the FPC.** Use a controlled service loop with hard motion stops.
7. **Keep lithium cells out of the impact plate.** The vest battery stays in a padded, ventilated compartment with a fuse close to its output.
8. **Retained is not trapped.** Use a hidden two-action emergency release for fire, water, medical access, or entanglement.

---

## 7. DIY keyboard implementation

### Full keyboard

For the prototype, do not manufacture the curved silicone keyboard yet. Mount the 88 × 54 mm CardKB on a thin rigid plate along the inner forearm. The plate remains permanently connected to the cuff but can hinge outward 15–25 degrees for typing comfort.

### Five-key chord rail

Five keys provide 31 nonzero combinations. Suggested first mapping:

- Five single-key chords: E, T, A, O, I.
- Ten two-key chords: the next ten most frequent letters.
- Remaining three- to five-key chords: uncommon letters and commands.
- Long-press: alternate layer.
- Directional encoder: cursor, scroll, selection, and menu navigation.

The chord map should be software-configurable and displayed as an ANSI overlay during training.

---

## 8. Software stack

- Raspberry Pi OS Lite 64-bit; no graphical desktop.
- Read-only root filesystem or overlay filesystem.
- `systemd` service launches WayKeeper on boot.
- Linux virtual console or framebuffer terminal.
- ANSI/ncurses user interface.
- SQLite FTS5 index for manuals, ledgers, doctrine, contacts, maps, and OCR text.
- `ripgrep`, `fzf`, `less`, `tmux`, and `sqlite3` as maintenance tools.
- Pre-extract and OCR PDFs on a desktop; copy both documents and the finished index to the card.
- LUKS-encrypted writable partition for private ledgers and signatures.
- Unencrypted recovery partition containing boot repair instructions and checksums.
- Radios off by default; physical software-controlled radio enable mode.
- Primary display powered off automatically when the hinge sensor reports closed.
- Low-battery service performs sync, closes the database, and shuts down cleanly.

---

## 9. Build sequence

### Phase A — Desk prototype

1. Boot the Pi Zero 2 W from a normal 32–64 GB test card.
2. Connect the flexible-display controller by mini HDMI.
3. Connect CardKB and directional encoder.
4. Confirm ANSI console dimensions, rotation, font size, and keyboard mapping.
5. Measure idle, search, and peak power with a USB power meter.

**Stop condition:** Do not design the enclosure until the actual screen/controller/FPC assembly is in hand and measured.

### Phase B — Power and archive

1. Add PiSugar 3 and verify graceful shutdown.
2. Connect the V50 through the fused body harness.
3. Test deliberate cable breaks and hot reconnection.
4. Create the 256 GB archive card and verified mirror.
5. Test 8–12 hour mixed-operation runs.

### Phase C — Wearable enclosure

1. Make a cardboard/foam forearm mockup first.
2. Print the curved wrist cradle and keyboard plate.
3. Build rigid corner armor and screen cassette.
4. Route the cable inside the vest and sleeve.
5. Add gasket, sweat barrier, strain relief, and emergency release.
6. Test walking, typing, kneeling, sitting, and arm rotation before adding the display.

### Phase D — FOLED hinge

1. Obtain written bend-radius and cycle specifications from the display supplier.
2. Machine/print the teardrop hinge and hard stops.
3. Cycle an unpowered sample or sacrificial panel first.
4. Add hinge sensor and automatic screen-power switching.
5. Inspect the fold area and FPC after 100, 1,000, and 10,000 cycles.

### Phase E — Kinetic generator

1. Bench-test the generator using a controlled reciprocating jig.
2. Log voltage, current, mechanical stroke, frequency, and temperature.
3. Add a dummy weight carriage, not the battery.
4. Perform unloaded walking tests before adding meaningful mass.
5. Measure net stored watt-hours, not open-circuit voltage.

---

## 10. Questions for the flexible-display manufacturer

Request written answers before designing the final hinge:

1. Is this panel rated for **dynamic repeated folding**, or only static bending?
2. What are the minimum dynamic and static bend radii?
3. What is the guaranteed fold-cycle count at that radius?
4. Which region of the display and FPC is permitted to bend?
5. Can the supplier provide a matching HDMI or Raspberry Pi-compatible controller?
6. What is the exact controller-board size and typical/max power draw?
7. What cover film, adhesive, and neutral-axis stack are approved?
8. What moisture barrier and sweat protection are required?
9. Is touch available, and how does touch change the bend radius?
10. What are sample price, engineering sample lead time, MOQ, and replacement availability?
11. Can the panel operate predominantly black with cyan/blue text without image-retention problems?
12. Can the supplier provide STEP files, FPC drawings, connector part numbers, and a reference hinge design?

---

## 11. Go/no-go assessment

### Build now

- Pi, archive, ANSI software, UPS, vest power, CardKB, chord rail, navigation control, status display, cable harness, and enclosure mockup.

### Build after receiving the display

- Exact wrist shell, screen cassette, FPC routing, hinge, protective window, and thermal layout.

### Experimental

- Repeatedly folding an off-the-shelf bendable display.
- Walking-energy generator.
- Splash sealing around the fold.
- Custom curved membrane keyboard.

**Conclusion:** A working body-powered ANSI WayKeeper can be assembled as a DIY project. The compute, archive, keyboard, UPS, vest battery, and software are straightforward. The only genuinely custom-risk component is a rugged, repeatedly folded OLED hinge. Build the complete system first with the display held flat or gently curved; then treat the folding cassette as a replaceable development subsystem.
