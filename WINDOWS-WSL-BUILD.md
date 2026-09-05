# WayKeeper Windows / WSL2 build handoff

The build scripts are Linux shell scripts. Run them inside an Ubuntu WSL2
distribution, not directly from PowerShell or Command Prompt. Keep the unpacked
project in the WSL filesystem (for example `~/waykeeper-build`) rather than under
`/mnt/c`; Linux filesystem performance matters during the Armbian build.

## Host preparation

1. Enable WSL2 and install Ubuntu 24.04.
2. Install Docker Desktop and enable integration for the Ubuntu distribution.
3. Give Docker enough disk space. The Orange Pi builder checks for at least
   50 GiB free; 80 GiB or more is recommended for both profiles and caches.
4. From the Ubuntu WSL terminal, install the small host-side tool set:

   ```sh
   sudo apt update
   sudo apt install -y git rsync unzip xz-utils xorriso
   ```

5. Copy the ZIP into the WSL home directory and unpack it:

   ```sh
   mkdir -p ~/waykeeper-build
   cd ~/waykeeper-build
   unzip /mnt/c/Users/YOUR_NAME/Downloads/WayKeeper-ARM64-BuildKit-2026-08-16.zip
   cd WayKeeper-ARM64-BuildKit/Fallout-Assistant
   ```

## Build order

Confirm Docker is reachable:

```sh
docker version
```

Build the generic ARM64 QEMU images:

```sh
platform/scripts/build-qemu.sh lite
platform/scripts/build-qemu.sh full
```

Build the flashable Orange Pi Zero 2 images:

```sh
platform/scripts/build-orangepi.sh lite
platform/scripts/build-orangepi.sh full
```

The Armbian framework may install or request additional build-host packages on
its first run. Do not run the build from a FAT/exFAT/NTFS-mounted source tree;
Unix permissions and symbolic links are required.

After all four `.img.xz` files exist, create the mountable release bundle:

```sh
platform/scripts/make-release-iso.sh
```

Outputs are written to `Fallout-Assistant/platform/dist/`. The Orange Pi boots
from an extracted/flashed `waykeeper-orangepi-zero2-*.img.xz` image. The `.iso`
is a mountable distribution container and is not Orange Pi boot media.

## Verification

Every image and the release ISO receives a `.sha256` sidecar. Verify one with:

```sh
sha256sum -c platform/dist/waykeeper-orangepi-zero2-lite.img.xz.sha256
```

Ollama remains in source history only and is compile-disabled in both embedded
profiles. No build or packaging script uploads artifacts.
