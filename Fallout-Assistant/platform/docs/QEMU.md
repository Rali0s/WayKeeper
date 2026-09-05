# QEMU development target

WayKeeper uses QEMU's `virt` ARM64 board with Cortex-A53 CPUs, virtio block/network devices, UEFI, and the same 512 MB or 1 GB RAM limits used for the intended board.

```sh
platform/scripts/build-qemu.sh lite
platform/scripts/run-qemu.sh lite serial

platform/scripts/build-qemu.sh full
platform/scripts/run-qemu.sh full display
```

SSH is forwarded to `localhost:2222`. The development qcow2 overlay preserves changes while keeping the decompressed build image pristine; delete only the `.dev.qcow2` file to reset a VM.

The generic `virt` machine is the correct fast userspace development target, but it cannot validate Orange Pi GPIO, PMIC, AW859A Wi-Fi/Bluetooth, HDMI timing, display rotation, thermal throttling, or touch-controller device tree. Those require a flashed Zero 2 and UART boot log.

The Full display path adds a virtio GPU and USB tablet. The in-image `cage` kiosk owns the screen, `foot` renders the ANSI terminal, and `wvkbd` reserves a 240-pixel bottom QWERTY strip. No desktop shell, panel, browser, or graphical launcher is installed.

An ARM64 Linux build host is strongly recommended for release images. On x86 Linux the builder uses static QEMU translation, and on Docker Desktop it additionally uses `proot` because the VM does not expose cross-architecture binfmt registration. That portable fallback is correct but can take hours for kernel/toolchain package configuration. The debootstrap package cache persists under `platform/cache/` between attempts.
