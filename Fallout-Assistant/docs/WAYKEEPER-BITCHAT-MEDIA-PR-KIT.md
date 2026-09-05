# WayKeeper (TM) + Nearby BLE Mesh — Media and PR Kit

Status: collaboration preview / engineering prototype  
Publication state: draft — do not publish until radio interoperability and name approval  
Prepared: August 29, 2026

## One-line description

WayKeeper is exploring a small, text-only, unofficial bitchat-compatible BLE
mesh transport for its rugged, local-first ANSI field terminal.

## Short description

WayKeeper (TM) is an offline knowledge and field-operations terminal designed
around a compact Orange Pi Zero 2 and a highly readable ANSI interface. Its
Nearby Mesh prototype adds a native Linux communications surface inspired by
bitchat's infrastructure-independent Bluetooth mesh model. The current milestone
installs the ANSI controls, bounded state and hardware qualification workflow;
radio transmission remains deliberately locked until cross-platform tests pass.

## Project facts

- Platform: native C++20 WayKeeper ANSI shell
- Target hardware: Orange Pi Zero 2 H616
- Radio candidate: onboard AW859A Bluetooth 5.0; qualified USB BLE fallback
- UI: keyboard/touch-friendly fixed-cell ANSI
- Intended transport: Linux BlueZ GATT
- Intended compatibility: official bitchat iOS and Android clients
- Lite goal: text-only public nearby mesh below 100 MB installed
- Heavy services excluded: Android runtime, Swift runtime, Ollama, Nostr, Tor,
  Wi-Fi Aware, geohash, voice, files and media
- Privacy: no transcript or message preview on the WayKeeper Sentinel screen
- Current truth: ANSI/state milestone complete; radio protocol TX locked

## Why it matters

The collaboration opportunity connects two complementary ideas:

1. bitchat demonstrates local, account-free BLE mesh communication.
2. WayKeeper provides an offline, low-resource Linux field terminal with maps,
   manuals, schematics, inventory, journal, UART tools and privacy lock screens.

Together, they could establish a tested native Linux/ARM64 interoperability path
without turning WayKeeper into another mobile app.

## Approved talking points

- “WayKeeper is evaluating an unofficial bitchat-compatible Linux transport.”
- “The prototype is text-only and designed for constrained ARM64 hardware.”
- “The project will not claim interoperability before three-device testing.”
- “Public nearby mesh traffic is public; private-message claims require verified Noise tests.”
- “The team is seeking technical collaboration with permissionlesstech.”

## Claims not approved

Do not state that:

- permissionlesstech, Jack Dorsey, or the bitchat maintainers endorse WayKeeper
- WayKeeper currently sends or relays bitchat traffic
- public mesh messages are end-to-end encrypted
- BLE replaces emergency, licensed, Sub-GHz, satellite, or public-safety radio
- the Android repository is public domain without noting its current GPL-3.0 license file
- WayKeeper is anonymous, untrackable, certified, waterproof, or field-proven

## Draft announcement

### Headline

WayKeeper Opens a Native Linux Path Toward Nearby Bluetooth Mesh Messaging

### Subheadline

The rugged ANSI field-terminal project has completed its first communications
milestone and is inviting permissionlesstech to explore cross-platform bitchat
interoperability on low-resource ARM64 hardware.

### Release copy

WayKeeper today introduced the engineering workflow and ANSI control surface for
Nearby Mesh, a compact native Linux communications add-on intended to interoperate
with bitchat over Bluetooth Low Energy.

Rather than embedding an Android application, WayKeeper is pursuing a separate,
bounded Linux service connected to its existing ANSI terminal. The first milestone
includes the operator interface, ECO/ACTIVE/OFF profiles, radio-readiness checks,
privacy boundaries and panic-wipe state contract. Transmission remains locked
until the Orange Pi controller, BlueZ GATT transport and iOS-to-WayKeeper-to-Android
relay chain pass documented acceptance tests.

WayKeeper is inviting permissionlesstech maintainers to review the compatibility
approach, identify stable protocol fixtures and discuss whether a native Linux
client or reference transport could benefit the wider bitchat ecosystem.

### Suggested project quote

> WayKeeper is built around information that remains useful when infrastructure
> disappears. Nearby text mesh is a natural extension, but only if we earn every
> interoperability and privacy claim through real hardware testing.

Quote requires project-owner approval before publication.

## FAQ

### Is this an official bitchat product?

No. It is currently an unofficial WayKeeper engineering prototype and collaboration proposal.

### Does it work over Bluetooth today?

The ANSI integration and bounded state are built. The BlueZ GATT and compatible
wire-protocol transport are not yet enabled, so transmission is locked.

### Why not run the Android app?

WayKeeper targets a small Debian/ARM64 image and ANSI interface. A native service
avoids Android, Compose and mobile lifecycle dependencies.

### Will it be under 100 MB?

That is the hard packaging goal. The target for the Lite increment is below 30 MB,
subject to actual ARM64 dependency and firmware measurements.

### Is it encrypted?

Public nearby-room content is public. Private messaging will not be described as
encrypted until the Noise interoperability gate passes.

### Does it share location?

The Lite BLE design does not require GPS or geohash. “Nearby” means radio proximity,
possibly extended by multiple relaying peers.

## Collaboration requests

- confirm the preferred stable release/commit for protocol implementation
- identify authoritative BLE service, packet and Noise fixtures
- advise on client naming and compatibility marks
- review a future native Linux test harness
- participate in iOS–WayKeeper–Android relay testing
- discuss whether generic Linux support belongs upstream, adjacent, or as a documented client

## Media assets required before publication

- approved WayKeeper wordmark and mascot
- Orange Pi prototype photograph labeled “engineering prototype”
- ANSI Nearby Mesh screenshot showing `PROTOCOL TX LOCKED`
- architecture diagram
- license/attribution statement
- approved maintainer quote, if collaboration occurs

## Contact

WayKeeper project contact: **add the project owner's public media email before publication**  
bitchat/permissionlesstech collaboration route: <https://bitchat.free/contact.html>  
Technical workflow: `docs/WAYKEEPER-BITCHAT-BUILD-WORKFLOW.md`

