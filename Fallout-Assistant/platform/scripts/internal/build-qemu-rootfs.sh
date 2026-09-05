#!/usr/bin/env bash
set -euo pipefail
export DEBIAN_FRONTEND=noninteractive

profile="${1:?profile required}"
output="${2:?output path required}"
workspace=/workspace
app="$workspace/Fallout-Assistant"
platform="$app/platform"
size_gib=4
[[ "$profile" == full ]] && size_gib=8

raw="${output%.xz}"
mkdir -p "$(dirname "$output")" /mnt/waykeeper-root /mnt/waykeeper-efi
truncate -s "${size_gib}G" "$raw"
parted -s "$raw" mklabel gpt
parted -s "$raw" mkpart ESP fat32 1MiB 257MiB
parted -s "$raw" set 1 esp on
parted -s "$raw" mkpart root ext4 257MiB 100%

efi_offset=$((1 * 1024 * 1024))
efi_size=$((256 * 1024 * 1024))
root_offset=$((257 * 1024 * 1024))
loop_efi="$(losetup --find --show --offset "$efi_offset" --sizelimit "$efi_size" "$raw")"
loop_root="$(losetup --find --show --offset "$root_offset" "$raw")"
cleanup() {
    set +e
    umount -R /mnt/waykeeper-root 2>/dev/null
    losetup -d "$loop_efi" 2>/dev/null
    losetup -d "$loop_root" 2>/dev/null
}
trap cleanup EXIT
mkfs.vfat -F 32 -n WK_EFI "$loop_efi"
mkfs.ext4 -F -L WAYKEEPER_ROOT "$loop_root"
mount "$loop_root" /mnt/waykeeper-root
mkdir -p /mnt/waykeeper-root/boot/efi
mount "$loop_efi" /mnt/waykeeper-root/boot/efi

debootstrap --arch=arm64 --foreign --variant=minbase \
    --cache-dir=/cache trixie /mnt/waykeeper-root https://deb.debian.org/debian
arm_chroot() {
    if [[ "$(dpkg --print-architecture)" == arm64 ]]; then
        chroot /mnt/waykeeper-root "$@"
    else
        proot -0 -r /mnt/waykeeper-root -w / -b /dev \
            -q /usr/bin/qemu-aarch64-static "$@"
    fi
}
DEBOOTSTRAP_DIR=/debootstrap arm_chroot /bin/sh /debootstrap/debootstrap --second-stage

cat > /mnt/waykeeper-root/etc/apt/sources.list <<'EOF'
deb http://deb.debian.org/debian trixie main contrib non-free-firmware
deb http://security.debian.org/debian-security trixie-security main contrib non-free-firmware
deb http://deb.debian.org/debian trixie-updates main contrib non-free-firmware
EOF
echo waykeeper > /mnt/waykeeper-root/etc/hostname
cat > /mnt/waykeeper-root/etc/hosts <<'EOF'
127.0.0.1 localhost
127.0.1.1 waykeeper
::1 localhost ip6-localhost ip6-loopback
EOF

stage=/tmp/waykeeper-stage
rm -rf "$stage"
"$platform/scripts/stage-profile.sh" "$profile" "$stage"
rsync -a "$platform/manifests/" "$stage/opt/waykeeper-src/Fallout-Assistant/platform-manifests/"
rsync -a "$stage/" /mnt/waykeeper-root/

mount --bind /dev /mnt/waykeeper-root/dev
mount -t proc proc /mnt/waykeeper-root/proc
mount -t sysfs sys /mnt/waykeeper-root/sys
arm_chroot /usr/bin/apt-get update
arm_chroot /usr/bin/apt-get install -y --no-install-recommends \
    linux-image-arm64 grub-efi-arm64-bin
arm_chroot /bin/bash /usr/local/libexec/waykeeper-install-image

root_uuid="$(blkid -s UUID -o value "$loop_root")"
efi_uuid="$(blkid -s UUID -o value "$loop_efi")"
cat > /mnt/waykeeper-root/etc/fstab <<EOF
UUID=$root_uuid / ext4 defaults,noatime,commit=60 0 1
UUID=$efi_uuid /boot/efi vfat umask=0077 0 2
EOF
arm_chroot /bin/bash -c \
    'grub-install --target=arm64-efi --efi-directory=/boot/efi --bootloader-id=WAYKEEPER --removable --no-nvram'
arm_chroot /bin/bash -c update-grub
sync
cleanup
trap - EXIT
xz -T0 -6 -f "$raw"
sha256sum "$output" > "$output.sha256"
