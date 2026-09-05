#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
platform_dir="$(cd "$script_dir/.." && pwd)"
dist="$platform_dir/dist"
bundle="$platform_dir/work/release-bundle"
output="$dist/waykeeper-arm64-release.iso"
mkdir -p "$dist" "$platform_dir/work"

artifacts=(
    "$dist/waykeeper-orangepi-zero2-full.img.xz"
    "$dist/waykeeper-orangepi-zero2-lite.img.xz"
    "$dist/waykeeper-qemu-arm64-full.img.xz"
    "$dist/waykeeper-qemu-arm64-lite.img.xz"
)
for artifact in "${artifacts[@]}"; do
    [[ -f "$artifact" ]] || { echo "missing release artifact: $artifact" >&2; exit 66; }
done

rm -rf "$bundle"
mkdir -p "$bundle/images" "$bundle/docs"
for artifact in "${artifacts[@]}"; do install -m 0644 "$artifact" "$bundle/images/"; done
install -m 0644 "$platform_dir/docs/IMAGE-FORMATS.md" "$platform_dir/docs/QEMU.md" \
    "$platform_dir/docs/USB-C-UART-LATER-RELEASE.md" "$bundle/docs/"
(
    cd "$bundle"
    sha256sum images/* > SHA256SUMS
)
cat > "$bundle/README.txt" <<'EOF'
WAYKEEPER ARM64 RELEASE

The .img.xz files in images/ are the bootable/flashable media.
The ISO itself is a mountable download bundle; Orange Pi firmware does not boot it.
Verify SHA256SUMS before decompressing or flashing.
No component of this bundle uploads data or enables Ollama.
EOF

if command -v xorriso >/dev/null; then
    xorriso -as mkisofs -quiet -V WAYKEEPER_ARM64 -J -r -o "$output" "$bundle"
elif command -v hdiutil >/dev/null; then
    hdiutil makehybrid -quiet -iso -joliet -default-volume-name WAYKEEPER_ARM64 \
        -o "$output" "$bundle"
else
    echo "install xorriso (Linux) or use hdiutil (macOS)" >&2
    exit 69
fi
sha256sum "$output" > "$output.sha256"
echo "built $output"

