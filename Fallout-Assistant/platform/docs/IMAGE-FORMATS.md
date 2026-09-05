# Image formats

WayKeeper uses three artifacts with deliberately different jobs:

1. `waykeeper-qemu-arm64-{full,lite}.img.xz` — compressed GPT disk images. Decompress and attach as a virtio block device to QEMU's ARM64 `virt` machine.
2. `waykeeper-orangepi-zero2-{full,lite}.img.xz` — compressed raw microSD/eMMC images containing the H616 boot chain. Flash one directly to removable media.
3. `waykeeper-arm64-release.iso` — mountable ISO-9660 download bundle containing all four images, documentation, and checksums.

An ISO is not used as Orange Pi boot media. The H616 Boot ROM expects its SPL/U-Boot layout at fixed sectors of a raw block device. Renaming an image to `.iso` would make it less safe and would not make it bootable.

Full includes the touch compositor, on-screen keyboard, raster/PDF support, maps, and available offline corpora. Lite omits the GUI stack, PDFs, map rasters, and herb database while retaining reviewed cards and text readers.

All generated artifacts live under `platform/dist/`, are Git-ignored, and are accompanied by SHA-256 files. The build scripts do not upload or publish them.

