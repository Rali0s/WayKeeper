#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
destination="$project_root/maps/ny"
rail_api="https://carto.nationalmap.gov/arcgis/rest/services/transportation/MapServer/38/query"
bounds="-79.7624,40.4961,-71.8562,45.0159"
page_size=2000

for tool in curl jq ogr2ogr ogrinfo; do
    command -v "$tool" >/dev/null || { echo "Required tool is missing: $tool" >&2; exit 2; }
done

mkdir -p "$destination"
work_dir="$(mktemp -d /tmp/offgrid-ny-railroads.XXXXXX)"
trap 'find "$work_dir" -type f -delete 2>/dev/null || true; find "$work_dir" -depth -type d -exec rmdir {} \; 2>/dev/null || true' EXIT

rail_count="$(curl -fsSLG "$rail_api" \
    --data-urlencode 'where=1=1' --data-urlencode "geometry=$bounds" \
    --data-urlencode 'geometryType=esriGeometryEnvelope' --data-urlencode 'inSR=4326' \
    --data-urlencode 'spatialRel=esriSpatialRelIntersects' \
    --data-urlencode 'returnCountOnly=true' --data-urlencode 'f=json' | jq -er '.count')"

raw_gpkg="$work_dir/railroads-raw.gpkg"
fields='objectid,name,railusage,railclassification,railsubdivision,railowner,railownercode,lengthkm,source_originator,loaddate'
for ((offset = 0; offset < rail_count; offset += page_size)); do
    page="$work_dir/page-$offset.geojson"
    curl -fsSLG "$rail_api" \
        --data-urlencode 'where=1=1' --data-urlencode "geometry=$bounds" \
        --data-urlencode 'geometryType=esriGeometryEnvelope' --data-urlencode 'inSR=4326' \
        --data-urlencode 'spatialRel=esriSpatialRelIntersects' \
        --data-urlencode "outFields=$fields" --data-urlencode 'orderByFields=objectid' \
        --data-urlencode "resultOffset=$offset" --data-urlencode "resultRecordCount=$page_size" \
        --data-urlencode 'returnGeometry=true' --data-urlencode 'outSR=4326' \
        --data-urlencode 'maxAllowableOffset=0.00008' --data-urlencode 'geometryPrecision=6' \
        --data-urlencode 'f=geojson' -o "$page"
    [[ "$(jq -er '.features | length' "$page")" -gt 0 ]] || { echo "Empty rail page" >&2; exit 3; }
    if [[ ! -f "$raw_gpkg" ]]; then
        ogr2ogr -f GPKG "$raw_gpkg" "$page" -nln railroads -nlt PROMOTE_TO_MULTI \
            -lco SPATIAL_INDEX=YES -lco DESCRIPTION='USGS National Map railroads intersecting New York'
    else
        ogr2ogr -f GPKG -update -append "$raw_gpkg" "$page" -nln railroads -nlt PROMOTE_TO_MULTI
    fi
done

output="$destination/USGS-National-Map-New-York-railroads.gpkg"
temporary="$work_dir/railroads.gpkg"
boundary="$destination/USGS-National-Map-New-York-boundary.geojson"
ogr2ogr -f GPKG "$temporary" "$raw_gpkg" railroads -nln railroads \
    -clipsrc "$boundary" -simplify 0.00008 -nlt PROMOTE_TO_MULTI \
    -lco SPATIAL_INDEX=YES -lco DESCRIPTION='USGS railroads clipped to New York State'
cp "$temporary" "$output"
ogrinfo -ro -so "$output" railroads
echo "Saved $rail_count source rail segments to $output"
