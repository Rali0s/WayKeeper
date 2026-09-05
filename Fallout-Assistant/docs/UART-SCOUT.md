# WayKeeper Field I/O / UART Scout

## Outcome

UART Scout is a shared field-I/O subsystem, not a device-specific hacking tab.
It uses one persisted master connection profile from both the ANSI workstation
and the ordinary CLI. The first implementation supports local serial/USB-UART,
virtual COM, one explicit TCP endpoint, Telnet carrier checks, and RFC 2217
carrier checks. It never scans a subnet and never transmits until an operator
records authorization and separately enables the transmit gate.

The feature is available from the workstation's second tab, `[2] FIELD I/O`, or
from `offgrid-assistant scout ...`. It remains useful without a GUI, window
manager, Java, Ollama, or network connection.

## Architecture

```text
ANSI FIELD I/O tab              waykeeper scout CLI
          \                         /
           +-- io-scout.ini -------+
                    |
             UART Scout core
          /         |          \
 local serial   explicit TCP   offline manual search
 USB-UART/vCOM  Telnet/RFC2217  SurvivalLibrary
          \         |          /
           adapter/service profile
 raw | AT modem | OGP1 | ATA/SIP | Ghostline | Flipper Zero
```

`Ethernet`, `USB`, and a local COM port are connection media. `TCP`, Telnet,
and RFC 2217 are transports or session protocols. SIP is an application
protocol; an ATA is a device role; VoIP is a service family; com0com is a
Windows virtual-port provider. I2C, SPI, CAN, RS-485, and similar buses require
the correct physical adapter and isolation. They must not be presented as
synonyms in one protocol selector.

## Connection lifecycle

1. Identify equipment and record a profile name.
2. Search the offline WayKeeper library for its manual or service reference.
3. Select a transport and one explicit endpoint.
4. Select an adapter/service interpretation.
5. Set serial framing or a TCP port.
6. Save the master profile. Saving clears authorization and transmit state.
7. Perform a read-only carrier test.
8. For serial sources, listen without a payload or run the baud heuristic.
9. Record authorization for equipment the operator owns or is approved to
   service. Transmit remains locked.
10. Enable TX separately, send an exact payload, inspect a sanitized response,
    then lock TX again.

The baud scan listens at 300, 1200, 2400, 4800, 9600, 19200, 38400, 57600,
115200, 230400, 460800, and 921600 baud. It scores observed bytes by printable
content, line endings, and an OGP1 frame marker. It sends no payload. Opening
some USB-UART adapters may still change DTR/RTS or reset attached development
boards, so “passive” describes application bytes rather than a guarantee of no
electrical side effect.

## Segment review

| Segment | CLI viability | Field-I/O adaptation |
| --- | --- | --- |
| F1 Cards | Complete | Add reviewed electrical-isolation and connector cards later; no model required. |
| F2 Documents | Complete | Existing offline search is reused as the target-manual step. Device-family packs should remain optional. |
| F3 Guide | Complete with model off | May explain captured, sanitized text; it must never invent commands or auto-transmit them. |
| F4 Profile | Complete | Incident and terrain remain operator context, not device authorization. |
| F5 System | Complete | Should eventually report serial backend, permissions, USB topology, and isolation-hardware readiness. |
| F6 Maps | Complete | Grid Watch may place operator-approved infrastructure points on map overlays later; no network discovery is inferred from a map. |
| F7 Herbs | Complete | No direct I/O dependency. Future environmental sensors can publish OGP1 readings with measured/estimated quality. |
| F8 Journal/Citizens | Complete | Connection events and field findings may be attached to logs later; secrets and raw credentials must not be copied automatically. |
| F9 Society | Complete | Rebuilding references remain read-only. Public-service restoration does not grant access to utility controls. |
| F10 Inventory | Complete | Track isolated adapters, cables, terminators, fuses, voltage levels, and known-good loopback plugs. |
| F11 OOBE | Complete | May install a curated operations list and optional serial packages; must not silently enable login services. |
| F12 About | Complete | No direct I/O dependency. |
| ANSI/Minimal UI | Complete | `[2] FIELD I/O` fits the wide and minimal tab rails; all core actions also exist outside the UI. |
| Telemetry | Parser exists | OGP1 becomes the sensor adapter. CRC and sequence numbers remain a later protocol revision. |
| Network status | Basic | Grid Watch shows local link/address plus the one configured target. It deliberately does not sweep ranges. |
| ARM images | Portable core | Linux device permissions and stable `/dev/serial/by-id` rules remain image integration work. |
| Windows/MSYS2 | Core/profile/TCP portable | COM enumeration is present. Serial read/write is reserved for the planned libserialport backend. |
| macOS | Operational | Enumerates `/dev/cu.*`; POSIX carrier, listen, baud scan, and explicit send are implemented. |
| Linux | Operational | Enumerates `/dev/serial/by-id`, `ttyUSB`, `ttyACM`, `ttyS`, and `ttyAMA`; POSIX I/O is implemented. |

## CLI contract

```sh
offgrid-assistant scout help
offgrid-assistant scout ports
offgrid-assistant scout protocols
offgrid-assistant scout configure serial /dev/cu.usbserial-0001 115200 sensor "device service manual"
offgrid-assistant scout status
offgrid-assistant scout probe
offgrid-assistant scout listen 2500
offgrid-assistant scout baud-scan 300
offgrid-assistant scout manual
offgrid-assistant scout authorize
offgrid-assistant scout tx on
offgrid-assistant scout send 'status'
offgrid-assistant scout tx off
```

CLI and ANSI both read `state/io-scout.ini` (or the directory selected by
`OFFGRID_STATE_DIR`). The file is intentionally plain text for field recovery.
It contains endpoint settings, not credentials.

## Grid restoration watch

The current watch answers only three safe questions:

- Does WayKeeper have a local link and address?
- Is the explicitly configured serial port openable or TCP port reachable?
- What did the authorized operator record in the journal and map?

Future work may add a small allowlist of operator-owned endpoints with last-seen
time, link carrier, DHCP state, DNS/NTP availability, and OGP1 power/sensor
readings. It must not discover or attempt login to utility, telecom, emergency,
industrial-control, or municipal equipment merely because it is reachable.

BusyBox is useful as an optional Linux field toolbox for commands such as `ip`,
`ping`, `nc`, and `telnet`. WayKeeper should expose approved diagnostics through
an argument-safe wrapper and audit log, not grant a hidden shell or interpolate
untrusted device text into a command.

## Ghostline boundary

Ghostline remains out of process, but its WayKeeper adapter is now operational.
UART Scout can start and stop a forced observe-only Ghostline relay for the one
approved master target, show process state/logs, and render the last GLCAP1
records as timestamped terminal hex/ASCII. It also retains a `DLT_USER0` PCAP
for later Wireshark inspection.

For TCP, clients connect to loopback `127.0.0.1:17777`; Ghostline relays to the
configured target. Port 1883 enables MQTT framing and CONNACK summaries. For a
serial target, the same loopback endpoint bridges to the configured UART/vCOM
port and baud. Ghostline's standalone profile format also supports serial-pair
operation with independent ingress and upstream baud.

Capture is limited to 4 MiB of payload per run with a 1024-byte per-read snap
length. It is application-stream capture: no promiscuous mode, root privilege,
subnet scan, or fabricated Ethernet/IP headers. WayKeeper requires recorded
authorization and the GHOSTLINE adapter before launch.

## Flipper roles

Flipper Zero has three practical optional roles:

1. USB-UART bridge between WayKeeper and a 3.3 V UART target.
2. USB CLI/log endpoint; the official CLI uses 230400 baud.
3. Custom sensor or RPC front end that emits OGP1 records.

Those uses reduce the number of separate adapters in a field kit. Flipper's
standalone Sub-GHz, NFC, infrared, iButton, and other applications do not need
WayKeeper integration. Radio transmit/replay automation remains outside this
layer. Flipper ONE is reserved as a future Linux/network adapter because its
published hardware and software are still under active development.

## Orange Pi board identity and connector plan

The existing WayKeeper image target is **Orange Pi Zero 2 (H616)**. The board
described as having two USB-C connectors appears to be **Orange Pi Zero 2W**,
which is a different target and device tree. Lock the exact board before making
an enclosure, cable, or PCB decision.

Official Zero 2W documentation describes USB0 as dual-role and USB1 as host,
with additional USB host and 100 Mb Ethernet available through its 24-pin
expansion connector. A field unit should use a proper hub, expansion board, or
dedicated USB-to-Ethernet/USB-UART topology. A mechanical “KVM” toggle is the
wrong abstraction for USB role negotiation and can create back-power or signal
integrity faults.

Never connect SoC UART pins directly to USB D+/D−, USB-C CC, or Ethernet pairs.
Use voltage-correct USB-UART, isolated RS-232/RS-485/CAN, and Ethernet hardware.
Verify VBUS direction, ESD protection, grounding, and boot/recovery behavior on
the exact board.

## Primary references

- Orange Pi Zero 2W official documentation:
  <https://www.orangepi.org/orangepiwiki/index.php/Orange_Pi_Zero_2W>
- RFC 2217, Telnet COM Port Control Option:
  <https://www.rfc-editor.org/info/rfc2217>
- libserialport cross-platform serial API:
  <https://sigrok.org/wiki/Libserialport>
- ser2net serial-to-network bridge:
  <https://github.com/cminyard/ser2net>
- com0com Windows virtual serial pairs:
  <https://com0com.sourceforge.net/>
- Flipper Zero CLI:
  <https://docs.flipper.net/zero/development/cli>
- Flipper Zero GPIO and USB-UART bridge:
  <https://docs.flipper.net/zero/gpio-and-modules>
- Flipper expansion protocol:
  <https://developer.flipper.net/flipperzero/doxygen/expansion_protocol.html>
- Flipper ONE technical specifications:
  <https://docs.flipper.net/one/general/tech-specs>
- BusyBox applet reference:
  <https://busybox.net/downloads/BusyBox.html>

## Next engineering slices

1. Adopt libserialport for one tested macOS/Linux/Windows serial backend.
2. Add stable Linux udev aliases and least-privilege group rules.
3. Add RFC 2217 negotiation rather than treating it as TCP carrier only.
4. Add line/hex views, bounded capture files, timestamps, and OGP1 live parsing.
5. Add an allowlisted diagnostic runner for `ping`, DNS, NTP, and route status.
6. Add connection-profile collections without weakening the single active
   master-session model.
7. Add electrical adapter inventory and a pre-connect voltage/isolation check.
8. Port Ghostline's prepared com0com serial profile to a real Win32 serial runtime.
