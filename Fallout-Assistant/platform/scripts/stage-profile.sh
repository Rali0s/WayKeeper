#!/usr/bin/env bash
set -euo pipefail

profile="${1:-}"
destination="${2:-}"
if [[ "$profile" != full && "$profile" != lite ]]; then
    echo "usage: $0 full|lite DESTINATION" >&2
    exit 64
fi
if [[ -z "$destination" || "$destination" == / ]]; then
    echo "refusing unsafe staging destination" >&2
    exit 64
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
platform_dir="$(cd "$script_dir/.." && pwd)"
app_dir="$(cd "$platform_dir/.." && pwd)"
workspace_dir="$(cd "$app_dir/.." && pwd)"
source_stage="$destination/opt/waykeeper-src/Fallout-Assistant"

mkdir -p "$source_stage" "$destination/usr/local/libexec"
rsync -a "$platform_dir/overlay/" "$destination/"
install -m 0755 "$platform_dir/install-image.sh" \
    "$destination/usr/local/libexec/waykeeper-install-image"

for path in CMakeLists.txt CMakePresets.json include src java cards config; do
    rsync -a "$app_dir/$path" "$source_stage/"
done
mkdir -p "$source_stage/library" "$source_stage/maps"
rsync -a --include='*.tsv' --include='*.md' --exclude='*' \
    "$app_dir/library/" "$source_stage/library/"
rsync -a "$app_dir/library/text" "$app_dir/library/readers" "$source_stage/library/"
mkdir -p "$source_stage/library/segments/Schematics"
rsync -a "$app_dir/library/segments/Schematics/txt" \
    "$source_stage/library/segments/Schematics/"
install -m 0644 "$app_dir/library/segments/Schematics/README.md" \
    "$app_dir/library/segments/Schematics/segment.json" \
    "$source_stage/library/segments/Schematics/"
rsync -a --include='*.tsv' --include='*.md' --include='terrain-colors.txt' --exclude='*' \
    "$app_dir/maps/" "$source_stage/maps/"

if [[ "$profile" == full ]]; then
    rsync -a \
        --exclude='segments/Schematics/records/' \
        --exclude='segments/Schematics/catalog.tsv' \
        --exclude='segments/Schematics/sources.tsv' \
        --exclude='segments/Schematics/INDEX.md' \
        --exclude='segments/Schematics/LOW-ENERGY-CANDIDATES.md' \
        --exclude='segments/Schematics/checksums.sha256' \
        "$app_dir/library/" "$source_stage/library/"
    rsync -a "$app_dir/maps/" "$source_stage/maps/"
fi

mascot_source="$workspace_dir/RES/WayKeeper TM"
mascot_stage="$destination/opt/waykeeper-src/RES/WayKeeper TM"
mkdir -p "$mascot_stage"
for image in SurvivalMode.png VaultTec-Blue.png Zombie-FalloutMode.png \
    Raider-Waykeeper.png Vault-Tec-WayKeeper-Easteregg.png; do
    install -m 0644 "$mascot_source/$image" "$mascot_stage/$image"
done
install -m 0644 "$mascot_source/SurvivalMode.png" \
    "$destination/usr/share/plymouth/themes/waykeeper/waykeeper.png"

printf '%s\n' "$profile" > "$destination/etc/waykeeper-build-profile"
