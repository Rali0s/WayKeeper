#!/usr/bin/env bash
set -euo pipefail

profile="${1:-lite}"
if [[ "$profile" != full && "$profile" != lite ]]; then
    echo "usage: $0 [full|lite]" >&2
    exit 64
fi
command -v docker >/dev/null || { echo "Docker is required" >&2; exit 69; }

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
app_dir="$(cd "$script_dir/../.." && pwd)"
workspace_dir="$(cd "$app_dir/.." && pwd)"
dist="$app_dir/platform/dist"
cache="$app_dir/platform/cache/debootstrap"
mkdir -p "$dist" "$cache"
output="/out/waykeeper-qemu-arm64-$profile.img.xz"
builder_image="waykeeper-qemu-builder:bookworm"

docker build --tag "$builder_image" \
    --file "$app_dir/platform/docker/qemu-builder.Dockerfile" \
    "$app_dir/platform/docker"

docker run --rm --privileged \
    -v "$workspace_dir:/workspace:ro" \
    -v "$dist:/out" \
    -v "$cache:/cache" \
    "$builder_image" \
    /workspace/Fallout-Assistant/platform/scripts/internal/build-qemu-rootfs.sh \
    "$profile" "$output"

echo "built $dist/waykeeper-qemu-arm64-$profile.img.xz"
