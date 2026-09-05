# WayKeeper Off-Grid

WayKeeper is a local-first ANSI operations terminal and a reproducible Debian ARM64 image project. The existing C++ ANSI shell remains the only product interface. The embedded Full profile adds a minimal Wayland kiosk solely to host the terminal and touch keyboard; Lite runs directly on a Linux virtual terminal or UART.

## Repository map

- `Fallout-Assistant/` — preserved C++20 ANSI shell, classified offline readers, fixed-cell schematics, maps, tests, and UI assets.
- `Fallout-Assistant/platform/` — QEMU and Orange Pi Zero 2 image builders, root filesystem overlay, package profiles, and hardware notes.
- `RES/WayKeeper TM/` — WayKeeper mascot and hardware concept references.
- `Survival-Library/` and `Scraper/` — supporting reference manifests and collection tools; they are not automatically included in WayKeeper images.

## Start here

```sh
cmake -S Fallout-Assistant -B Fallout-Assistant/build/dev -G Ninja
cmake --build Fallout-Assistant/build/dev
ctest --test-dir Fallout-Assistant/build/dev --output-on-failure

Fallout-Assistant/platform/scripts/build-qemu.sh lite
Fallout-Assistant/platform/scripts/run-qemu.sh lite
```

Orange Pi builds use the board-specific builder documented in [`Fallout-Assistant/platform/README.md`](Fallout-Assistant/platform/README.md). They produce flashable `.img.xz` files, because the Orange Pi firmware does not boot PC-style ISO media. `make-release-iso.sh` can additionally create a mountable ISO-9660 download bundle containing Full and Lite images, checksums, and flashing instructions.

## Git safety

The upstream repository is [Rali0s/WayKeeper](https://github.com/Rali0s/WayKeeper). The supplied ignore rules exclude environment files, credentials, operator state, build trees, generated images, large downloaded document corpora, and independent local projects. Nothing in the setup scripts stages, commits, pushes, uploads, or creates a remote.
