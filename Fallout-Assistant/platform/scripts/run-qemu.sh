#!/usr/bin/env bash
set -euo pipefail

profile="${1:-lite}"
display_mode="${2:-serial}"
if [[ "$profile" != full && "$profile" != lite ]]; then
    echo "usage: $0 [full|lite] [serial|display]" >&2
    exit 64
fi
command -v qemu-system-aarch64 >/dev/null || { echo "qemu-system-aarch64 is required" >&2; exit 69; }

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
dist="$(cd "$script_dir/.." && pwd)/dist"
compressed="$dist/waykeeper-qemu-arm64-$profile.img.xz"
raw="$dist/waykeeper-qemu-arm64-$profile.img"
overlay="$dist/waykeeper-qemu-arm64-$profile.dev.qcow2"
[[ -f "$compressed" || -f "$raw" ]] || { echo "build the $profile image first" >&2; exit 66; }
if [[ ! -f "$raw" ]]; then xz -dk "$compressed"; fi
if [[ ! -f "$overlay" ]]; then qemu-img create -f qcow2 -F raw -b "$raw" "$overlay"; fi

firmware="${WAYKEEPER_QEMU_EFI:-}"
if [[ -z "$firmware" ]]; then
    for candidate in \
        /opt/homebrew/share/qemu/edk2-aarch64-code.fd \
        /usr/share/AAVMF/AAVMF_CODE.fd \
        /usr/share/qemu-efi-aarch64/QEMU_EFI.fd; do
        if [[ -f "$candidate" ]]; then firmware="$candidate"; break; fi
    done
fi
[[ -f "$firmware" ]] || { echo "set WAYKEEPER_QEMU_EFI to ARM64 EDK2 firmware" >&2; exit 66; }

memory=512
[[ "$profile" == full ]] && memory=1024
display_args=(-nographic)
if [[ "$display_mode" == display ]]; then
    display_args=(-device virtio-gpu-pci -device qemu-xhci -device usb-tablet)
fi

exec qemu-system-aarch64 \
    -machine virt -cpu cortex-a53 -smp 4 -m "$memory" \
    -bios "$firmware" \
    -drive if=none,file="$overlay",id=hd0,format=qcow2,cache=writeback \
    -device virtio-blk-pci,drive=hd0 \
    -netdev user,id=net0,hostfwd=tcp::2222-:22 \
    -device virtio-net-pci,netdev=net0 \
    "${display_args[@]}"

