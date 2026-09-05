# WayKeeper (TM) Nearby BLE Mesh Build Workflow

Status: engineering milestone — ANSI and bounded state implemented; radio transmit locked  
Target: Orange Pi Zero 2 (H616), Debian/Armbian ARM64, onboard AW859A or qualified USB BLE adapter  
Upstream compatibility target: `permissionlesstech/bitchat` protocol version pinned during Gate 2

## 1. Objective

Add a small, text-only, unofficial bitchat-compatible BLE mesh transport to the
WayKeeper ANSI shell. It is an add-on, not a mobile-app port. It must remain
useful without Android, Swift, a GUI, Ollama, Wi-Fi, Nostr, Tor, or GPS.

The shipped increment should remain below 100 MB. The working target is below
30 MB installed and below 50 MB resident memory after the real radio backend is
added.

## 2. Current implementation boundary

Implemented now:

- `offgrid-assistant mesh ...` CLI surface
- fixed-screen ANSI entry through the Field I/O selector
- four-choice Nearby Chat screen: mode, nickname, status, back
- fixed `#bluetooth` public nearby channel
- validated 24-byte local nickname
- requested `OFF`, `ECO`, and `ACTIVE` profiles
- host/BlueZ/HCI readiness display
- exact-file panic wipe for profile, history, identity, and outbox paths
- no chat content on Sentinel
- explicit privacy and license boundary
- protocol TX locked until qualification is complete

Not implemented or claimed:

- BlueZ GATT transport
- bitchat packet compatibility
- scanning, advertising, peer discovery, relay, or send
- Noise private messages
- courier, Nostr, Tor, geohash, Wi-Fi Aware, media, files, or voice

The locked state is intentional. A UI prototype must not fabricate a successful
radio operation.

## 3. Source layout

```text
include/offgrid/mesh.hpp      profile, readiness and bounded-state contract
src/mesh.cpp                  portable state and Linux readiness inspection
src/main.cpp                  `mesh` / `chat` CLI
src/ui.cpp                    ANSI Field I/O and Nearby Mesh screens
tests/test_core.cpp           profile, validation, TX lock and wipe tests
state/mesh-profile.ini        requested mode and nickname
state/mesh-history.tsv        reserved bounded public history
state/mesh-identity.key       reserved future protected identity
state/mesh-outbox.bin         reserved future sealed bounded outbox
```

Writable locations follow `OFFGRID_STATE_DIR` when it is set.

## 4. Gate 0 — build the current milestone

```sh
cmake --preset dev
cmake --build --preset dev -j2
ctest --preset dev --output-on-failure
./build/dev/offgrid-assistant mesh status
./build/dev/offgrid-assistant ui
```

Inside ANSI, press Tab to select `[2] FIELD I/O // UART + BLE`, press Enter,
then select Nearby BLE Mesh with `2` or the arrow keys.

Expected result on macOS or Windows: ANSI ready, Linux/BlueZ unavailable, and
protocol TX locked.

## 5. Gate 1 — qualify Orange Pi radio hardware

The Orange Pi Zero 2 specifies an onboard AW859A Wi-Fi/Bluetooth 5.0 module.
Do not add an ESP32 unless the normal Linux controller path fails a documented
requirement. A known Linux-supported USB BLE adapter on the powered hub is the
first fallback.

Install only the pinned image packages:

```sh
sudo apt-get update
sudo apt-get install --no-install-recommends bluez dbus libsystemd-dev libsodium-dev liblz4-dev
sudo systemctl enable --now bluetooth.service
```

Inspect without pairing or sending application data:

```sh
bluetoothctl show
bluetoothctl list
sudo btmgmt info
ls -la /sys/class/bluetooth
./offgrid-assistant mesh qualify
```

Record in the build manifest:

- exact Orange Pi board revision
- kernel, BlueZ, firmware and device-tree versions
- controller manufacturer/version and `hci` path
- onboard or USB topology
- antenna and enclosure configuration
- idle, scan, advertise and connected power measurements

Pass conditions:

1. Controller survives 100 power cycles without disappearing.
2. LE scanning and advertising work concurrently.
3. Local GATT server and remote GATT client operate concurrently.
4. Four simultaneous peer links survive an eight-hour soak.
5. ECO and ACTIVE power are measured, not estimated.

If the AW859A fails, repeat with one approved USB adapter as `hci1`. Never ship a
chipset selected only because `bluetoothctl list` happens to show it once.

## 6. Gate 2 — pin upstream and license inputs

Before writing the wire transport:

1. Record a release tag and full commit hash from
   `https://github.com/permissionlesstech/bitchat`.
2. Archive the matching `WHITEPAPER.md`, `LICENSE`, protocol source and tests.
3. Record the Android commit used only for black-box interoperability testing.
4. Inventory every proposed library and its license.
5. Add source hashes and third-party notices to the WayKeeper image manifest.

The iOS/macOS upstream is Unlicensed/public-domain dedicated. The Android
repository currently contains a GPL-3.0 `LICENSE.md` even though its README says
public domain. Do not copy Android Kotlin source into the WayKeeper core. Treat
Android builds as interoperability peers unless a reviewed GPL distribution
plan explicitly says otherwise.

## 7. Gate 3 — implement the Linux transport

Add `waykeeper-meshd` as a separate process:

```text
WayKeeper ANSI <-> Unix-domain socket <-> waykeeper-meshd <-> BlueZ D-Bus <-> hciN
```

Required BlueZ work:

- register the exact upstream GATT service and characteristics
- advertise the exact service identity
- scan only for the exact service UUID
- connect as GATT central and accept connections as GATT peripheral
- negotiate MTU and never assume a fixed 512-byte payload
- bound every D-Bus object, connection, buffer, timer and reconnect attempt
- stop cleanly when BlueZ disappears or the selected controller changes

Do not substitute `bluetoothctl` text scraping for a production D-Bus backend.
Do not use the BlueZ Mesh API: bitchat implements its own application-layer
controlled flood over BLE GATT and is not Bluetooth SIG Mesh.

## 8. Gate 4 — packet and relay compatibility

Implement only from the pinned public protocol contract:

1. exact header encoding and network byte order
2. message type parsing and length validation
3. signed announcement validation
4. message IDs and bounded deduplication
5. TTL clamp and split-horizon relay
6. jitter and deterministic fanout
7. MTU-aware fragmentation and bounded reassembly
8. peer freshness and connection scheduling
9. public `#bluetooth` messages

Hard limits must be compile-time or profile constants. Begin with upstream
limits but reject earlier when Orange Pi memory measurements require it. Fuzz the
decoder before enabling radio TX.

## 9. Gate 5 — private-message cryptography

Private messages remain disabled until all of these pass:

- Noise XX test vectors
- X25519, ChaCha20-Poly1305, SHA-256 and Ed25519 known-answer tests
- iOS-to-WayKeeper and Android-to-WayKeeper handshake captures
- replay, truncation, reordering, duplicate and malformed-frame tests
- identity storage permissions and atomic panic wipe
- clear distinction between public messages and encrypted DMs in ANSI

Do not design new cryptography. Do not label public mesh chat encrypted.

## 10. Gate 6 — three-device relay acceptance

Use current official iOS and Android builds plus the target Orange Pi:

```text
iOS peer <---- BLE ----> WayKeeper relay <---- BLE ----> Android peer
```

Pass:

- both phones exchange public messages through WayKeeper
- direct phone-to-phone path is absent during the relay test
- TTL falls once at WayKeeper
- duplicate traffic never appears in either timeline
- reconnect and controller restart recover without rebooting WayKeeper
- malformed peers cannot exceed memory, disk or connection limits
- eight-hour ACTIVE and 24-hour ECO soaks complete
- Blackout disables BLE according to the approved privacy policy
- Sentinel never displays nickname, peers, transcript or message previews

## 11. Gate 7 — packaging

Full image:

- BlueZ GATT transport
- public mesh and verified Noise DMs
- bounded encrypted outbox only after review
- complete notices and protocol fixtures

Lite image:

- public BLE mesh only
- no Nostr, Tor, Wi-Fi Aware, geohash, media, files or voice
- smaller history, connection and fragment caps

Release checks:

```sh
cmake --preset prod
cmake --build --preset prod -j2
ctest --preset prod --output-on-failure
du -sh build/prod/offgrid-assistant package-root
```

Publish source, build instructions, dependency notices, exact upstream hashes,
WayKeeper modifications and reproducible image checksums together.

## 12. Operator safety and claims

- Nearby public messages are observable and relayable.
- Current upstream announcements expose a persistent identity and nickname.
- BLE is opportunistic short-range communication, not guaranteed emergency radio.
- Range depends on antenna, enclosure, interference, topology and peer density.
- Never promise encryption until the Noise path is selected and verified.
- Never imply permissionlesstech endorsement without written collaboration.

## Primary upstream references

- Project: <https://github.com/permissionlesstech/bitchat>
- Whitepaper: <https://github.com/permissionlesstech/bitchat/blob/main/WHITEPAPER.md>
- iOS/macOS license: <https://github.com/permissionlesstech/bitchat/blob/main/LICENSE>
- Android repository: <https://github.com/permissionlesstech/bitchat-android>
- Android license: <https://github.com/permissionlesstech/bitchat-android/blob/main/LICENSE.md>
- Official collaboration contact route: <https://bitchat.free/contact.html>
- BlueZ GATT API: <https://bluez.readthedocs.io/en/latest/gatt-api/>
