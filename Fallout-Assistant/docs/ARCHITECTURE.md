# Architecture

## Field stack

```text
macOS Terminal + keyboard
          |
   ANSI Survival UI + CLI
    |      |      |       |          |
  cards  PDF    Guide   Journal   Field I/O
                                      |   |
                                    UART BLE*
          |      |       |
      page text  Ollama  plaintext logs
          |      |       |
          local files only
```

The terminal and `scout` CLI must remain useful if the graphical shell, model
process, vector index, or network is unavailable. Field I/O uses the same
plain-text master profile in both interfaces.

## Field I/O boundary

UART Scout owns connection profiles, local port enumeration, explicit endpoint
carrier tests, passive serial reads, baud scoring, and the authorization/TX
gate. Offline manual search remains in `SurvivalLibrary`; OGP1 remains the
sensor-record format. Ghostline is an optional external TCP adapter and Flipper
Zero is an optional USB-UART/CLI adapter. Neither is a runtime dependency.

The Ghostline child-process adapter is forced observe-only and captures only
traffic deliberately routed through its loopback or serial relay. It writes a
bounded recovery-text GLCAP1 file plus optional DLT_USER0 PCAP. The first
release does not scan address ranges, attempt login, or auto-send a command
inferred by the Guide. See [UART Scout](UART-SCOUT.md).

`BLE*` is the Nearby Mesh build milestone: ANSI controls, bounded state and
readiness inspection exist, but BlueZ GATT and bitchat-compatible transmit stay
locked until the documented Orange Pi interoperability gates pass. See
[Nearby BLE Mesh workflow](WAYKEEPER-BITCHAT-BUILD-WORKFLOW.md).

## Journal storage

The Captain's Log uses one `OFFGRID-JOURNAL-1` plaintext file per entry under
the local state directory. Updates use a temporary file and rollback backup.
Archive moves entries into `state/journal/archive` rather than deleting them.
The store has no model, database, or network dependency, so notes remain
inspectable with ordinary recovery tools.

## Model and retrieval

- **Model:** Qwen3-4B GGUF Q4_K_M is the initial field target. Test Qwen3-8B only when the measured energy and memory budget permits.
- **Runtime:** Ollama is convenient during development. Production should link or launch `llama.cpp` directly to reduce moving parts and allow explicit context, thread, batch, and power-mode control.
- **Embeddings:** Qwen3-Embedding-0.6B GGUF is a reasonable starting point; benchmark it on the actual hardware.
- **Index:** SQLite FTS5 for exact text and metadata plus a compact, memory-mapped vector file. A server vector database is unnecessary for the expected library size.
- **Citations:** Every retrieved passage retains document title, edition, page, source URL, license status, ingestion date, and content hash.
- **Preparation:** OCR, chunking, embeddings, and integrity manifests are produced on a workstation. The field unit performs retrieval, not bulk indexing.

## Hardware boundary

Development remains cross-platform, while the shipping field target is Orange Pi
Zero 2 (H616) with constrained RAM and microSD storage. Desktop macOS remains the
current build host. Optional services must not become requirements for the ANSI
core.

Use a small microcontroller as power supervisor. It owns battery/solar measurement, fan and thermal alarms, watchdog, hard-shutdown sequencing, and physical wake buttons. It must not depend on Linux being healthy.

Recommended storage is a replaceable NVMe drive plus a read-only duplicate USB library. Keep card data and checksums independently recoverable.

## Controlled radio milestones and deferred lo-fi hardware

Generic authorized field I/O is now active through UART Scout. Radio control,
PortaPack, Sonar hardware, replay, and transmit automation remain deferred.
Nearby Mesh has a separate locked engineering workflow for ordinary BLE GATT;
it does not grant a generic radio-transmit path.

The default policy is passive reception:

- PortaPack/Mayhem receive apps and Morse decoding can publish observations through OGP1.
- Existing TPMS Sonar material is an experimental passive concept, not a completed sensor.
- Transmit functions are a separate process and hardware path. They require physical arming, explicit operator action, applicable license/band-plan configuration, and a visible transmit indicator.
- Jamming, replay, spoofing, and automatic transmission are outside the assistant design.

Mayhem code is GPL-licensed. Keep the assistant adapter at a serial/file/process boundary unless the combined program intentionally adopts compatible licensing.

## Telemetry protocol

Phase A uses one inspectable ASCII record:

```text
OGP1|source|metric|value|unit|unix_ms|quality
```

Quality is `measured`, `estimated`, `simulated`, or `fault`. Sources include `solar`, `battery`, `environment`, `portapack`, and `sonar`. Phase B should add sequence numbers and CRC while retaining line-oriented recovery.
