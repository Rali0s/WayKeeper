#!/usr/bin/env bash
set -euo pipefail

profile="${1:-lite}"
if [[ "$profile" != full && "$profile" != lite ]]; then
    echo "usage: $0 [full|lite]" >&2
    exit 64
fi
for tool in git rsync; do
    command -v "$tool" >/dev/null || { echo "$tool is required" >&2; exit 69; }
done

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
platform_dir="$(cd "$script_dir/.." && pwd)"
app_dir="$(cd "$platform_dir/.." && pwd)"
work="$platform_dir/work/armbian-build"
dist="$platform_dir/dist"
armbian_ref="${ARMBIAN_REF:-v26.8.0-trunk.343}"

available_kib="$(df -Pk "$platform_dir" | awk 'NR==2 {print $4}')"
if (( available_kib < 50 * 1024 * 1024 )); then
    echo "Orange Pi source builds require at least 50 GiB free; only $((available_kib / 1024 / 1024)) GiB is available." >&2
    exit 70
fi

mkdir -p "$platform_dir/work" "$dist"
if [[ ! -d "$work/.git" ]]; then
    git clone --branch "$armbian_ref" --depth 1 https://github.com/armbian/build.git "$work"
else
    git -C "$work" fetch --depth 1 origin "$armbian_ref"
    git -C "$work" checkout --detach FETCH_HEAD
fi

overlay="$work/userpatches/overlay"
if [[ -e "$overlay" ]]; then
    echo "refusing to overwrite existing Armbian userpatches overlay: $overlay" >&2
    exit 73
fi
mkdir -p "$overlay"
cleanup() {
    rm -rf "$overlay"
    rm -f "$work/userpatches/customize-image.sh"
}
trap cleanup EXIT
"$script_dir/stage-profile.sh" "$profile" "$overlay"
rsync -a "$platform_dir/manifests/" \
    "$overlay/opt/waykeeper-src/Fallout-Assistant/platform-manifests/"
install -m 0755 "$platform_dir/armbian/customize-image.sh" \
    "$work/userpatches/customize-image.sh"

(
    cd "$work"
    ./compile.sh build \
        BOARD=orangepizero2 \
        BRANCH=current \
        RELEASE=trixie \
        BUILD_MINIMAL=yes \
        BUILD_DESKTOP=no \
        KERNEL_CONFIGURE=no \
        KERNEL_BTF=no \
        COMPRESS_OUTPUTIMAGE=xz \
        SHARE_LOG=no
)

mapfile -t images < <(find "$work/output/images" -maxdepth 1 -type f -name '*.img.xz' -print -quit)
if (( ${#images[@]} != 1 )); then
    echo "expected one Armbian image; inspect $work/output/images" >&2
    exit 65
fi
destination="$dist/waykeeper-orangepi-zero2-$profile.img.xz"
install -m 0644 "${images[0]}" "$destination"
sha256sum "$destination" > "$destination.sha256"
echo "built $destination"

