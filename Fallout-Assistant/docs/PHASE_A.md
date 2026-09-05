# Phase A

## Goal

Produce a bootable, text-first assistant that remains useful with the model disabled. Phase A proves the safety, retrieval, sensor, portability, and energy-accounting boundaries before choosing final enclosure and solar capacity.

## Included now

- C++20/CMake core on macOS, Linux, and MSYS2/MinGW Windows.
- ANSI terminal with a non-color fallback.
- Reviewed-card lookup with the water-tablet decision card.
- Query energy estimator and reserve-aware route selection.
- Dormant OGP1 parser retained for future compatibility; it is not exposed in the Survival UI.
- Registered local source roots and corpus policy.
- Offline Captain's Log and survival-note journal with search and reversible archive.
- Shared UART Scout master profile for CLI and ANSI use, local serial discovery,
  passive carrier/read tests, baud scoring, offline manual lookup, and an
  explicit authorization/transmit gate.
- Nearby BLE Mesh ANSI/state milestone with BlueZ readiness inspection and a
  deliberately locked protocol-transmit gate.

## Current focus: personal macOS console

The current build is personal-use software. Packaging, licensing review, support expectations, and sellable-product decisions are explicitly deferred.

## Next implementation slices

1. SQLite FTS5 manual catalog with page-level citations and SHA-256 verification.
2. `llama.cpp` process adapter, streamed output, cancellation, and model-off mode.
3. On-device calibration command that records idle watts, load watts, tokens/second, screen state, thermals, and battery conversion losses.
4. Solar/battery dashboard with state-of-charge uncertainty and reserve alarms.
5. Keyboard-driven Lilliput layout and optional framebuffer UI.
6. Local encrypted user preferences and wellbeing check-ins.
7. Complete the cross-platform libserialport backend and OGP1 live reader.
8. Complete the pinned Orange Pi/BlueZ/bitchat interoperability gates.
9. Only after the core is mature: PortaPack, Morse, radio, Sonar, and other lo-fi hardware modules.

## Acceptance gates

- A reviewed card is available in under one second with the model stopped.
- Every safety answer shows a source and review date.
- Search results identify document and page.
- The app survives malformed serial lines and missing peripherals.
- A model response can be interrupted immediately.
- The power supervisor can shut down the computer without assistance from the AI process.
- Production builds pass on Linux, macOS, and Windows/MSYS2.

## Battery strategy

Use common, replaceable chemistries without treating loose cells as interchangeable:

- Base station: a fused 12.8 V LiFePO4 pack with a chemistry-specific BMS and charger.
- Portable peripherals: protected, manufacturer-matched 18650 packs where supported.
- Low-drain accessories: standard AA NiMH cells.
- Interconnect: USB-C Power Delivery and separately fused DC rails.

Do not mix cell ages, capacities, brands, charge states, or chemistries in one pack. Final pack, fuse, wire, connector, thermal, and solar-controller sizing requires electrical review and measured load data.
