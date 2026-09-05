# WayKeeper ARM64 Platform

This directory turns the existing ANSI shell into two Debian 13 (`trixie`) ARM64 profiles without replacing its interface.

| Profile | Target | Interface | Included content | Minimum RAM |
| --- | --- | --- | --- | --- |
| Lite | QEMU / Orange Pi Zero 2 512 MB | tty1 + UART ANSI | reviewed cards, text/readers, networking, Wi-Fi/Bluetooth services | 512 MB |
| Full | QEMU / Orange Pi Zero 2 1 GB preferred | `cage` + `foot` + bottom QWERTY `wvkbd` | Lite plus PDFs, maps, herbs database, GDAL/SQLite/Poppler and boot graphics | 512 MB supported, 1 GB recommended |

Ollama is retained in C++ source but `WAYKEEPER_ENABLE_OLLAMA=OFF` is hard-set by the image installer. No Ollama package, model, service, or port is present in either image.

Both image profiles compile with `WAYKEEPER_APPLIANCE_MODE=ON`. Escape backs out
of nested screens, but Escape, `Q`, `QUIT`, or `EXIT` at the command center enters
the public Sentinel lock instead of terminating to Linux. Lite and Full also run
under restart-always systemd services as a second containment layer. Desktop
development builds leave this option off so developers can still exit normally.
Authenticated SSH or a maintenance console can stop the WayKeeper service when
board-level development is required.

## QEMU development

QEMU uses the generic ARM64 `virt` board. It validates Debian boot, the ANSI application, services, memory behavior, networking, and keyboard flow. It does **not** emulate the Allwinner H616, AW859A radio, Orange Pi PMIC, HDMI quirks, GPIO, or actual touchscreen.

```sh
platform/scripts/build-qemu.sh lite
platform/scripts/run-qemu.sh lite
```

The builder requires Docker and approximately 5 GB free space. It creates a GPT raw disk with an ARM64 EFI System Partition and Debian root partition under `platform/dist/`. Full defaults to 1024 MB RAM; Lite defaults to 512 MB. Native ARM64 Linux is the practical release host; Docker Desktop uses a much slower `proot` + QEMU fallback.

## Orange Pi Zero 2

```sh
platform/scripts/build-orangepi.sh lite
platform/scripts/build-orangepi.sh full
```

The board builder uses a pinned Armbian build framework to supply the Zero 2-specific U-Boot SPL, ARM Trusted Firmware, kernel, `sun50i-h616-orangepi-zero2.dtb`, modules, and board firmware. It overlays this repository's Debian userspace and does not install an Armbian desktop. Expect the upstream builder to need about 50 GB free disk and 8 GB host RAM.

Flash the resulting image, never the release ISO:

```sh
xzcat platform/dist/waykeeper-orangepi-zero2-lite.img.xz | sudo dd of=/dev/SDX bs=8M conv=fsync status=progress
```

Verify `/dev/SDX` carefully. This command overwrites the selected device.

## Release bundle

After both images exist:

```sh
platform/scripts/make-release-iso.sh
```

The resulting `waykeeper-arm64-release.iso` is a mountable/downloadable ISO-9660 bundle, not Orange Pi boot media. It contains the flashable images and SHA-256 manifest. No script uploads it.

## Runtime layout

- `/usr/local/bin/waykeeper` — compiled shell.
- `/opt/waykeeper` — cards, catalog, offline resources, mascots, and optional maps/PDFs.
- `/var/lib/waykeeper` — mutable operator profile, settings, inventory, and journal.
- `waykeeper-shell.service` — Lite direct tty/UART service.
- `waykeeper-kiosk.service` — Full touch terminal service.
- `WAYKEEPER_APPLIANCE_MODE=ON` — compiled exit lock; the operator UI cannot expose a shell.
- NetworkManager and BlueZ manage radios; firmware comes from Debian non-free-firmware plus the board build.

See [`docs/IMAGE-FORMATS.md`](docs/IMAGE-FORMATS.md), [`docs/QEMU.md`](docs/QEMU.md), and [`docs/USB-C-UART-LATER-RELEASE.md`](docs/USB-C-UART-LATER-RELEASE.md).
