#!/usr/bin/env bash
set -euo pipefail
export DEBIAN_FRONTEND=noninteractive

profile="$(tr -d '[:space:]' < /etc/waykeeper-build-profile)"
if [[ "$profile" != full && "$profile" != lite ]]; then
    echo "invalid WayKeeper image profile: $profile" >&2
    exit 64
fi

if ! grep -Rqs 'non-free-firmware' /etc/apt/sources.list /etc/apt/sources.list.d 2>/dev/null; then
    cat > /etc/apt/sources.list.d/waykeeper-nonfree.list <<'EOF'
deb http://deb.debian.org/debian trixie main contrib non-free-firmware
EOF
fi

source_root=/opt/waykeeper-src/Fallout-Assistant
manifest_root="$source_root/platform-manifests"
if [[ ! -d "$manifest_root" ]]; then
    manifest_root=/usr/local/share/waykeeper/manifests
fi

mapfile -t packages < <(sed -e '/^[[:space:]]*#/d' -e '/^[[:space:]]*$/d' \
    "$manifest_root/common.packages" "$manifest_root/$profile.packages")
apt-get update
apt-get install -y --no-install-recommends "${packages[@]}"

if ! getent group waykeeper >/dev/null; then groupadd --gid 1000 waykeeper; fi
if ! id waykeeper >/dev/null 2>&1; then
    useradd --uid 1000 --gid waykeeper --create-home --shell /bin/bash waykeeper
fi
for group in audio bluetooth input netdev render seat video; do
    if getent group "$group" >/dev/null; then usermod -aG "$group" waykeeper; fi
done

cmake -S "$source_root" -B /tmp/waykeeper-build -G Ninja \
    -DCMAKE_BUILD_TYPE=MinSizeRel \
    -DOFFGRID_BUILD_TESTS=OFF \
    -DWAYKEEPER_ENABLE_OLLAMA=OFF \
    -DWAYKEEPER_APPLIANCE_MODE=ON
cmake --build /tmp/waykeeper-build --target offgrid-assistant --parallel 2
install -m 0755 /tmp/waykeeper-build/offgrid-assistant /usr/local/bin/waykeeper

install -d -o waykeeper -g waykeeper /opt/waykeeper /var/lib/waykeeper
cp -a "$source_root/cards" "$source_root/library" "$source_root/maps" /opt/waykeeper/
cp -a /opt/waykeeper-src/RES /opt/waykeeper/
cat > /var/lib/waykeeper/settings.ini <<EOF
width_pixels=640
height_pixels=480
resize_on_launch=1
theme=blue
room_code=WK-01
layout_mode=workstation
touch_keyboard=$([[ "$profile" == full ]] && echo 1 || echo 0)
companion_render=off
EOF
chown -R waykeeper:waykeeper /opt/waykeeper /var/lib/waykeeper

chmod 0755 /usr/local/libexec/waykeeper-tty /usr/local/libexec/waykeeper-cage-session
systemctl enable NetworkManager.service bluetooth.service zramswap.service \
    waykeeper-ssh-hostkeys.service 2>/dev/null || true
systemctl disable getty@tty1.service 2>/dev/null || true
if [[ "$profile" == full ]]; then
    systemctl enable seatd.service waykeeper-kiosk.service
    systemctl set-default graphical.target
    if command -v plymouth-set-default-theme >/dev/null 2>&1; then
        plymouth-set-default-theme waykeeper
        update-initramfs -u
    fi
else
    systemctl enable waykeeper-shell.service
    systemctl set-default multi-user.target
fi

# Build tools are not used at runtime. Dependencies are retained to avoid
# accidentally autoremove-linked GDAL/SQLite libraries in the Full profile.
apt-get purge -y cmake ninja-build build-essential pkg-config || true
apt-get clean
find /etc/ssh -maxdepth 1 -type f -name 'ssh_host_*' -delete
rm -rf /var/lib/apt/lists/* /tmp/waykeeper-build /opt/waykeeper-src
