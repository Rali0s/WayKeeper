# WAYKEEPER Hardware Concepts

Status: Phase-A engineering concept. Dimensions, runtime, ingress protection, thermal limits,
drop resistance, charging safety, and human-factors claims require prototype testing before field use.

## Architecture decision

Use the Jetson Orin Nano 8 GB module/carrier for the primary prototype. The original Jetson Nano
developer kit is a useful low-power fallback, but it boots from microSD and does not provide two
native M.2 Key-M NVMe sockets. The Orin Nano developer carrier provides two Key-M sockets plus a
microSD slot, satisfying the requested 2 x 2 TB NVMe and 1 TB microSD layout without USB-NVMe
bridges. Operate the module in its 7 W profile for passive cooling. Do not enable 25 W Super mode in
a sealed fanless enclosure.

NVIDIA developer kits are for development and prototyping, not production. A sellable unit would
need a production module, custom carrier, EMC work, safety certifications, and a supported thermal
design.

## Concept A: WK-01 self-contained field terminal

Target envelope: 285 x 205 x 78 mm closed, 2.3-3.0 kg with 128 Wh battery, excluding solar panel.

- Jetson Orin Nano 8 GB, locked to 7 W field mode with a temporary 15 W inference mode.
- 2 x 2 TB low-power 2280 NVMe SSDs. Keep the archive SSD mounted read-only when practical and
  let the second SSD enter autonomous power state transition idle.
- 1 TB high-endurance UHS-I microSD for boot/recovery, not swap.
- Lilliput MoPro7-class 7-inch 1280x800 display for the efficient build: approximately 5.8 W.
  A 1,800-nit H7 is a sunlight-readable option but can consume up to 16 W and materially shortens
  runtime.
- Hinged 40% wired USB keyboard using a low-power RP2040/Arduino-compatible USB-HID controller,
  sealed membrane or replaceable mechanical key deck.
- Projection keyboard is optional only. It has no tactile feedback, performs poorly in sunlight,
  needs a flat clean surface, and is unreliable with gloves, rain, dust, or vehicle vibration.
- 12.8 V 10 Ah / 128 Wh LiFePO4 removable battery with BMS, fuse, service disconnect, low-
  temperature charge inhibit, internal heater, redundant cell-temperature sensing, and fuel gauge.
- 10-28 V solar/DC input -> MPPT charger -> protected 12.8 V bus. Separate 5 V/5 A regulated
  compute rail and protected display rail. Never connect raw panel voltage to the Jetson.
- External aluminum heat spreader and shallow fins, isolated from the internal electronics with a
  gasketed thermal bulkhead. No internal fan.
- Replaceable polycarbonate display lens, elastomer corner bumpers, captive hardware, drain path,
  and gasketed I/O doors. IP54 is a prototype target, not a claim.
- Primary 100 W folding panel for dependable recharge; 60 W is the minimum useful field panel.

### WK-01 power model

Runtime uses 128 Wh nominal x 0.85 = 108.8 Wh usable after reserve and conversion losses.

| Mode | Compute | Display | Storage/I/O/conversion | Total | Estimated runtime |
|---|---:|---:|---:|---:|---:|
| Screen-off archive/server | 7 W | 0 W | 4 W | 11 W | 9.9 h |
| PDF/map/ANSI reader | 7 W | 4 W | 4 W | 15 W | 7.3 h |
| Normal local Guide | 10 W | 5 W | 6 W | 21 W | 5.2 h |
| AI + both SSDs active | 15 W | 6 W | 14 W | 35 W | 3.1 h |

A 256 Wh extended battery approximately doubles these figures. The 1,800-nit Lilliput H7 adds as
much as 10 W over the efficient screen assumption, reducing the 21 W case to roughly 3.5 hours.

Solar output is not panel nameplate output. Use 35 W delivered as the planning value for a well-
aimed 60 W panel and 60 W delivered for a 100 W panel in good sun:

| Panel | Unit off: replenish 108.8 Wh | Unit running at 21 W |
|---|---:|---:|
| 60 W nominal / 35 W delivered | 3.1 peak-sun h | 7.8 peak-sun h |
| 100 W nominal / 60 W delivered | 1.8 peak-sun h | 2.8 peak-sun h |

Cloud, heat, panel angle, shade, charge taper, and battery temperature extend those times. Design
around watt-hours harvested per day rather than the panel badge.

## Battery thermal-control subsystem

The operating requirement is -5 to 25 degrees F (-21 to -4 degrees C) ambient without permitting
cold charging, plus survival in direct summer sun without heat-soaking the cells. Insulation alone
cannot meet both conditions. The battery needs its own controlled thermal zone, separate from the
Jetson heat spreader and separate from the solar-exposed outer shell.

Use a certified self-heating LiFePO4 pack for Phase A rather than building a loose-cell heated pack.
The pack must expose temperature/BMS status or provide an independent charge-permission line. The
system controller must be able to disconnect the MPPT charger upstream of the battery; the BMS hard
cutoff remains the last safety layer, not normal regulation.

### Physical battery bay

- Double-wall battery drawer: light-colored or reflective external sun shell, 8-12 mm closed-cell
  insulation, radiant barrier, and an internal flame-resistant cell cradle.
- Two temperature sensors bonded to opposite ends of the cell assembly, one compartment-air sensor,
  and one outer-shell solar-load sensor. Use the coldest cell sensor for charge permission and the
  hottest cell sensor for heater/charge shutdown.
- Two distributed low-watt-density heater zones on an aluminum equalization plate outside the cell
  restraint. Do not create one concentrated heater hot spot or place unregulated pads directly on
  cells. A 20-30 W total heater is appropriate for the 128 Wh concept, subject to pack approval.
- A one-shot thermal fuse and independent hardware thermostat must interrupt the heater even if the
  software controller fails on.
- The battery and Jetson require separate thermal paths. Compute waste heat may warm the surrounding
  insulated bay in winter, but there must be no permanent conductive bridge that can drive processor
  or solar heat into the cells during summer.
- Place the battery low and shaded. Include a deployable aluminized sun cover and a visible `BATTERY
  HOT / MOVE TO SHADE` alarm. Never leave the unit operating or charging inside a sun-heated vehicle.

### Conservative control state machine

Final thresholds must come from the selected pack manufacturer. Initial prototype setpoints are:

| Cell temperature | Charge | Heater | Compute/load policy |
|---|---|---|---|
| Below -20 C / -4 F | Blocked | Input-powered warmup | Emergency shutdown except thermal controller |
| -20 to 0 C | Blocked | Warm from charger/solar first | Reduced-current discharge; avoid AI peaks |
| 0 to 5 C | Blocked | Continue warmup | Reader/boot permitted if pack voltage is stable |
| 5 to 10 C | Enabled only after both sensors remain above 5 C | Stop at 10 C | Normal 7 W field mode |
| 10 to 35 C | Enabled | Off | Normal operation |
| 35 to 40 C | Derated | Off | Reduce charge current and display/AI load |
| 40 to 45 C | Blocked by supervisory controller | Off | Warning; move to shade; 7 W maximum |
| Above 45 C | Blocked | Hardware-inhibited | Controlled shutdown; pack BMS remains final cutoff |

Use hysteresis and a five-minute temperature qualification before enabling charge so a warm surface
sensor cannot mask a frozen cell core. Solar/DC input powers the heater before it is connected as a
charger: `INPUT -> HEATER -> CELL WARM -> CHARGE PERMISSION -> MPPT/BMS CHARGE`.

Battery-powered freeze maintenance is optional and expensive. Enable it only when the operator
selects `COLD STANDBY`, state of charge is above 40 percent, and the pack manufacturer permits it.
Suggested control is heater on below 2 C and off at 8 C. Otherwise, allow the battery to cold-soak,
block charging, and warm it from solar/DC input when energy becomes available.

### Cold-energy allowance

Heating a 1.2 kg pack from approximately -20 C to +5 C needs roughly 8-10 Wh ideally; insulation,
thermal bridges, and wind make 15-25 Wh a more realistic planning allowance. A 25 W heater therefore
needs approximately 40-75 minutes before charging, then the pack can accept the remaining solar
power. Maintaining temperature may average 4-10 W depending on enclosure, wind, and access cycles.

If the 128 Wh WK-01 must spend 8 W continuously maintaining heat, its reader load rises from 15 W
to 23 W and expected runtime falls from 7.3 hours to about 4.7 hours. At -20 C an unheated lithium
pack may also expose substantially less usable capacity and greater voltage sag. Cold-weather
mission planning should therefore reserve 25 percent energy for warming and another 20 percent for
cold-capacity loss until chamber tests provide measured values.

## Concept B: WK-J1 jacket system

The torso carries mass; the wrist carries only display and controls.

- Fanless compute pod in a removable upper-back yoke. An exterior finned spine faces away from the
  body, with spacer mesh and an insulating air gap against the wearer.
- 128 Wh LiFePO4 pack on the rear belt or lower backpack, centered near the body's center of mass.
  Use a self-heating pack bay inside the jacket/backpack weather envelope; body warmth reduces heater
  demand, but spacer insulation prevents the pack from becoming a wearer hot spot.
- 5-inch Lilliput T5-class display in a replaceable forearm cradle. Its listed display power is up
  to 6 W. The T5 touch panel controls monitor functions; do not assume it is a USB touch input for
  Linux. Use physical keys or a separate USB-HID touch/control device.
- Five-key glove/brace navigation pad: up, down, accept, back, emergency/home. Retain a wired mini
  keyboard in a chest or belt pouch for journal entry.
- Power/data harness runs through replaceable seam channels, not permanently sewn conductors.
  Use keyed locking connectors at modules, magnetic/breakaway couplers at the shoulder and cuff,
  abrasion sleeves, drip loops, and service slack at elbow articulation.
- Solar panel mounts on the backpack and charges the belt battery through MPPT. The jacket must
  disconnect completely for laundering and fire safety.

Expected 128 Wh runtime: about 7.8 hours for reading/navigation at 14 W, 4.5 hours for local AI at
24 W, and 12 hours screen-off at 9 W. Electronics mass target is 1.4-2.0 kg distributed across the
back and belt; the wrist assembly target is 250-400 g.

## Concept C: WK-W1 detachable forearm computer

Target envelope: 190 x 105 x 38 mm, 0.85-1.15 kg. This is a forearm computer, not a wristwatch.
It must bridge the forearm on a four-point compression brace so its mass does not load the wrist
joint. Include a one-pull mechanical emergency release.

- Jetson module on a custom carrier; a full developer carrier is too large for a credible cuff.
- Two 2280 SSDs lengthwise below the display and a gasketed microSD service hatch.
- 5-inch display beneath a replaceable hard-coated polycarbonate sacrificial lens.
- 60 Wh certified 21700-based cartridge with BMS, cell fuse, temperature sensing, and rigid cell
  restraint. It needs charge lockout below 5 C and above 45 C. The cuff should use body warmth and a
  5-10 W thermostatic warming film for cold-start recovery, not continuous high-power heating. Do
  not design around loose field-swappable cells on the first prototype.
- Graphite spreader/heat pipe to an outer aluminum fin spine. No heat spreader may contact skin.
- Skin-side spacer mesh target below 42 C; touched exterior target below 48 C. Throttle before
  either limit is exceeded. These are design targets requiring instrumented testing.
- Dock contacts charge the detached cuff from the backpack solar/battery station.
- A 3 W-rated cuff solar strip is emergency trickle only. Body angle and partial shade are likely
  to yield 0.5-1.5 W, insufficient to operate the computer continuously.

Runtime uses 60 Wh x 0.85 = 51 Wh usable: approximately 5.7 hours screen-off at 9 W, 3.6 hours for
reading/navigation at 14 W, and 2.1 hours for local AI at 24 W. The micro-solar strip extends a
14 W reading load by only roughly 4-11 percent in strong light.

## Flexible-display decision

A flexible OLED is not recommended for Phase A. It requires a custom panel supply chain, driver
integration, curved cover tooling, environmental sealing, and burn-in controls for a mostly static
ANSI interface. Flexibility does not make the finished assembly impact-proof. Keep it as a later
industrial-design study.

A flexible plastic-backplane E Ink secondary status strip is more defensible for battery, compass,
incident, and next-waypoint data because it is sunlight-readable and holds static content without
continuous display power. It is not suitable as the primary scrolling terminal because refresh is
slower than LCD/OLED.

## Prototype order

1. Bench power tree with electronic load, fuse coordination, BMS, MPPT, brownout logging, and safe
   shutdown. Validate each rail before connecting the Jetson.
2. Open-frame Orin Nano + dual NVMe + microSD + efficient Lilliput display. Measure real wattage for
   boot, PDF, map, Ollama, SSD indexing, sleep, and shutdown.
3. WK-01 printed/polymer enclosure with external passive heat spreader. Perform thermal soak before
   attempting ingress sealing.
4. Jacket harness with dummy masses and breakaway cables. Complete range-of-motion, snag, fall,
   doffing, and emergency-release tests before installing powered hardware.
5. Forearm cuff using a dead-weight ergonomic shell, then a low-voltage display-only prototype.
   Add compute and battery only after the brace is comfortable and safely releasable.

## Rough prototype allowances

- WK-01: USD 1,300-2,000 depending on SSDs, screen, solar panel, and enclosure process.
- WK-J1 jacket: USD 1,600-2,800 including custom garment/harness work.
- WK-W1 cuff: USD 1,800-3,500 because it requires a custom carrier, thermal assembly, and brace.

These are planning allowances, not quotations. Production cost and compliance are separate.

## Engineering references checked 2026-08-16

- NVIDIA Jetson Orin Nano developer carrier: dual M.2 Key-M sockets, microSD, 7-15 W reference
  module profiles: https://developer.nvidia.com/blog/develop-ai-powered-robots-smart-vision-systems-and-more-with-nvidia-jetson-orin-nano-developer-kit/
- NVIDIA Orin Nano Super: USD 249 developer kit and 7/15/25 W software power profiles:
  https://www.nvidia.com/en-us/autonomous-machines/embedded-systems/jetson-orin/
- NVIDIA original Nano: microSD boot and 5/10 W profiles:
  https://developer.nvidia.com/embedded/learn/jetson-nano-2gb-devkit-user-guide
- Lilliput MoPro7: 7-inch 1280x800, 5.8 W:
  https://www.lilliputdirect.com/support/documents/mopro7/MoPro7_MAN.pdf
- Lilliput T5: 5-inch 1920x1080, display power up to 6 W:
  https://lilliputdirect.com/Lilliput-T5
- Representative 12.8 V 10 Ah LiFePO4 pack: 128 Wh, 1.2 kg, BMS and temperature limits:
  https://data.accu-24.de/Accu-24/8000935/Techn_Datenblatt_ACCU-24_A-LFP-12-10_en.pdf
- Victron Lithium Smart battery manual: 5 C default minimum allowed-to-charge temperature and
  warning that charging below the limit can permanently damage cells:
  https://www.victronenergy.com/upload/documents/Lithium_Battery_Smart/15958-Manual_Lithium_Smart_Battery-pdf-en.pdf
- RELiON low-temperature battery FAQ: charge input is diverted to the internal heater until the
  battery reaches a safe charging temperature; typical warm-up takes 1-1.5 hours depending on
  model and conditions:
  https://www.relionbattery.com/resource-center/support/faqs
- RELiON LT-series overview: heater-assisted charging can be initiated at temperatures as low as
  -20 C when a charge source is present:
  https://www.relionbattery.com/low-temperature-series-line
- Representative 60 W folding panel: 22 V nominal output and approximately 2 kg:
  https://manuals.ecoflow.com/us/product/60w-portable-solar-panel-type-c?lang=en_US
- E Ink flexible plastic-backplane technology and limitations:
  https://www.eink.com/tech/detail/Flexible

## Visual concepts

- `RES/WayKeeper TM/Hardware Concepts/WK-01-Field-Terminal-Mockup.png`
- `RES/WayKeeper TM/Hardware Concepts/WK-Wearable-Jacket-Mockup-v2.png`
- `RES/WayKeeper TM/Hardware Concepts/WK-Wrist-Unit-Mockup.png`
- `docs/hardware/WK-01-Field-Terminal-Blueprint.svg`
- `docs/hardware/WK-Wearable-Jacket-Blueprint.svg`
- `docs/hardware/WK-Wrist-Unit-Blueprint.svg`
- `docs/hardware/WK-Battery-Thermal-Blueprint.svg`
