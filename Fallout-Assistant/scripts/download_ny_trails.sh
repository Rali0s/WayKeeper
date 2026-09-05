#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
destination="$project_root/maps/ny"
trail_api="https://carto.nationalmap.gov/arcgis/rest/services/transportation/MapServer/37/query"
boundary_api="https://carto.nationalmap.gov/arcgis/rest/services/govunits/MapServer/22/query"
bounds="-79.7624,40.4961,-71.8562,45.0159"
page_size=2000

for tool in curl jq ogr2ogr ogrinfo; do
    command -v "$tool" >/dev/null || {
        echo "Required tool is missing: $tool" >&2
        exit 2
    }
done

mkdir -p "$destination"
work_dir="$(mktemp -d /tmp/offgrid-ny-trails.XXXXXX)"
cleanup() {
    find "$work_dir" -type f -delete 2>/dev/null || true
    find "$work_dir" -depth -type d -exec rmdir {} \; 2>/dev/null || true
}
trap cleanup EXIT

boundary_temp="$work_dir/new-york-boundary.geojson"
boundary_output="$destination/USGS-National-Map-New-York-boundary.geojson"
curl -fsSLG "$boundary_api" \
    --data-urlencode "where=STATE_NAME='New York'" \
    --data-urlencode 'outFields=STATE_NAME,STATE_FIPSCODE,GNIS_ID,AREASQKM' \
    --data-urlencode 'returnGeometry=true' \
    --data-urlencode 'outSR=4326' \
    --data-urlencode 'geometryPrecision=6' \
    --data-urlencode 'f=geojson' \
    -o "$boundary_temp"
jq -e '.features | length == 1' "$boundary_temp" >/dev/null
cp "$boundary_temp" "$boundary_output"

trail_where="trailtype='Terra Trail' AND hikerpedestrian IN ('Y','Yes')"
trail_count="$(curl -fsSLG "$trail_api" \
    --data-urlencode "where=$trail_where" \
    --data-urlencode "geometry=$bounds" \
    --data-urlencode 'geometryType=esriGeometryEnvelope' \
    --data-urlencode 'inSR=4326' \
    --data-urlencode 'spatialRel=esriSpatialRelIntersects' \
    --data-urlencode 'returnCountOnly=true' \
    --data-urlencode 'f=json' | jq -er '.count')"

raw_gpkg="$work_dir/new-york-hiking-trails-raw.gpkg"
fields='objectid,name,trailnumber,trailtype,hikerpedestrian,trailsurface,seasonopen,primarytrailmaintainer,nationaltraildesignation,lengthmiles,sourceoriginator,publisheddate'

echo "Downloading $trail_count USGS pedestrian trail segments..."
for ((offset = 0; offset < trail_count; offset += page_size)); do
    page="$work_dir/page-$offset.geojson"
    curl -fsSLG "$trail_api" \
        --data-urlencode "where=$trail_where" \
        --data-urlencode "geometry=$bounds" \
        --data-urlencode 'geometryType=esriGeometryEnvelope' \
        --data-urlencode 'inSR=4326' \
        --data-urlencode 'spatialRel=esriSpatialRelIntersects' \
        --data-urlencode "outFields=$fields" \
        --data-urlencode 'orderByFields=objectid' \
        --data-urlencode "resultOffset=$offset" \
        --data-urlencode "resultRecordCount=$page_size" \
        --data-urlencode 'returnGeometry=true' \
        --data-urlencode 'outSR=4326' \
        --data-urlencode 'maxAllowableOffset=0.00015' \
        --data-urlencode 'geometryPrecision=6' \
        --data-urlencode 'f=geojson' \
        -o "$page"
    page_count="$(jq -er '.features | length' "$page")"
    if ((page_count == 0)); then
        echo "Unexpected empty page at offset $offset" >&2
        exit 3
    fi
    if [[ ! -f "$raw_gpkg" ]]; then
        ogr2ogr -f GPKG "$raw_gpkg" "$page" \
            -nln hiking_trails -nlt PROMOTE_TO_MULTI \
            -lco SPATIAL_INDEX=YES -lco DESCRIPTION='USGS pedestrian-accessible terra trails'
    else
        ogr2ogr -f GPKG -update -append "$raw_gpkg" "$page" \
            -nln hiking_trails -nlt PROMOTE_TO_MULTI
    fi
    printf '  %d / %d\n' "$((offset + page_count))" "$trail_count"
done

trail_output="$destination/USGS-National-Digital-Trails-New-York-overview.gpkg"
trail_temp="$work_dir/new-york-hiking-trails.gpkg"
ogr2ogr -f GPKG "$trail_temp" "$raw_gpkg" hiking_trails \
    -nln hiking_trails -clipsrc "$boundary_output" -simplify 0.00015 \
    -nlt PROMOTE_TO_MULTI -lco SPATIAL_INDEX=YES \
    -lco DESCRIPTION='USGS pedestrian-accessible terra trails clipped to New York State'
cp "$trail_temp" "$trail_output"

echo
ogrinfo -ro -so "$trail_output" hiking_trails
echo "Saved: $trail_output"
