#!/usr/bin/env bash
set -euo pipefail

# Armbian executes this hook inside the target root filesystem after its board
# kernel, bootloader, DTB, modules, and firmware have been installed.
/usr/local/libexec/waykeeper-install-image

