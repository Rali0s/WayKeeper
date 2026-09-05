# Unsent Collaboration Email Draft — WayKeeper (TM) and bitchat

Delivery status: **DRAFT ONLY — NOT SENT**  
To: permissionlesstech / bitchat maintainers  
Official contact route: <https://bitchat.free/contact.html> directs development
collaboration to the permissionlesstech GitHub organization. No public email
address was listed, so do not guess one. Submit this as a GitHub Discussion or
maintainer-approved private contact when available.

## Subject

Collaboration proposal: native Linux/ARM64 bitchat interoperability for WayKeeper

## Message

Hello permissionlesstech team,

My name is Michael Cohee, and I am building WayKeeper (TM), a rugged, local-first
ANSI field terminal targeting the Orange Pi Zero 2 and small Debian/ARM64 images.
It combines offline manuals, maps, schematics, inventory, journal, privacy lock
screens and authorized UART field tools in a text-first interface.

We believe bitchat's nearby BLE mesh is a natural communications complement to
WayKeeper. We are exploring a small native Linux service rather than embedding
the Android or iOS applications. The intended architecture is:

```text
WayKeeper ANSI <-> local Unix socket <-> native mesh service <-> BlueZ GATT
```

Our first milestone is complete: the ANSI `CHAT/MESH` surface, bounded profile
and future state paths, radio-readiness view, ECO/ACTIVE/OFF targets, privacy
notices and exact-state panic wipe are integrated. Radio transmission is
deliberately locked; we are not claiming protocol compatibility yet.

Before implementing the wire transport, we would value your guidance on:

1. the preferred release or commit to treat as the compatibility baseline;
2. authoritative BLE service, packet, fragmentation and Noise test fixtures;
3. naming and attribution for an unofficial compatible Linux client;
4. whether a reusable Linux interoperability harness would be useful upstream;
5. a future iOS <-> WayKeeper <-> Android three-device relay test.

The Lite scope is intentionally narrow: public nearby text mesh first, then
private Noise sessions only after test-vector and device validation. Nostr, Tor,
geohash, media, files, voice and Wi-Fi Aware are excluded from the initial build.
Our installed-size ceiling is 100 MB, with a target below 30 MB.

We have also noted the current license difference between the Unlicensed
iOS/macOS repository and the GPL-3.0 Android `LICENSE.md`. Our plan is to author
the native Linux component from the public protocol and Unlicensed reference,
avoid copying Android Kotlin code, preserve notices and publish exact upstream
commit hashes and build instructions.

This is a collaboration request, not an endorsement claim. We would be happy to
adapt the plan to the project's preferred compatibility and contribution model.

Technical workflow:
`docs/WAYKEEPER-BITCHAT-BUILD-WORKFLOW.md`

Thank you for building an unusually practical offline communications project.

Best,

Michael Cohee  
WayKeeper (TM)  
**Add your preferred public reply email and repository URL before sending**

## GitHub Discussion version

Use the subject as the Discussion title and the message body unchanged. Remove
private contact information if the Discussion is public. Do not post security
findings publicly; use the repository's private vulnerability-reporting channel.

