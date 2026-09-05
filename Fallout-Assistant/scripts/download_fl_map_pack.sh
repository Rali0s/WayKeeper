#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
destination="$project_root/maps/fl"
elevation_api="https://elevation.nationalmap.gov/arcgis/rest/services/3DEPElevation/ImageServer/exportImage"
boundary_api="https://carto.nationalmap.gov/arcgis/rest/services/govunits/MapServer/22/query"
trail_api="https://carto.nationalmap.gov/arcgis/rest/services/transportation/MapServer/37/query"
rail_api="https://carto.nationalmap.gov/arcgis/rest/services/transportation/MapServer/38/query"
bounds="-87.6349,24.3963,-80.0314,31.0009"
page_size=2000

for tool in curl jq gdalinfo gdal_translate gdaldem ogr2ogr ogrinfo shasum; do
    command -v "$tool" >/dev/null || {
        echo "Required tool is missing: $tool" >&2
        exit 2
    }
done

mkdir -p "$destination"
work_dir="$(mktemp -d /tmp/offgrid-fl-map.XXXXXX)"
cleanup() {
    find "$work_dir" -type f -delete 2>/dev/null || true
    find "$work_dir" -depth -type d -exec rmdir {} \; 2>/dev/null || true
}
trap cleanup EXIT

echo "[1/5] Florida state boundary"
boundary_temp="$work_dir/florida-boundary.geojson"
boundary_output="$destination/USGS-National-Map-Florida-boundary.geojson"
curl -fsSLG "$boundary_api" \
    --data-urlencode "where=STATE_NAME='Florida'" \
    --data-urlencode 'outFields=STATE_NAME,STATE_FIPSCODE,GNIS_ID,AREASQKM' \
    --data-urlencode 'returnGeometry=true' \
    --data-urlencode 'outSR=4326' \
    --data-urlencode 'geometryPrecision=6' \
    --data-urlencode 'f=geojson' \
    -o "$boundary_temp"
jq -e '.features | length == 1' "$boundary_temp" >/dev/null
cp "$boundary_temp" "$boundary_output"

echo "[2/5] USGS 3DEP terrain overview"
terrain_download="$work_dir/florida-terrain-download.tif"
terrain_output="$destination/USGS-3DEP-Florida-State-preview.tif"
curl -fsSLG "$elevation_api" \
    --data-urlencode "bbox=$bounds" \
    --data-urlencode 'bboxSR=4326' \
    --data-urlencode 'imageSR=4326' \
    --data-urlencode 'size=1200,1000' \
    --data-urlencode 'format=tiff' \
    --data-urlencode 'pixelType=F32' \
    --data-urlencode 'interpolation=RSP_BilinearInterpolation' \
    --data-urlencode 'f=image' \
    -o "$terrain_download"
gdalinfo "$terrain_download" >/dev/null
gdal_translate -q -of GTiff -co COMPRESS=DEFLATE -co PREDICTOR=3 -co TILED=YES \
    "$terrain_download" "$terrain_output"

echo "[3/5] USGS pedestrian-accessible terra trails"
trail_where="trailtype='Terra Trail' AND hikerpedestrian IN ('Y','Yes')"
trail_count="$(curl -fsSLG "$trail_api" \
    --data-urlencode "where=$trail_where" \
    --data-urlencode "geometry=$bounds" \
    --data-urlencode 'geometryType=esriGeometryEnvelope' \
    --data-urlencode 'inSR=4326' \
    --data-urlencode 'spatialRel=esriSpatialRelIntersects' \
    --data-urlencode 'returnCountOnly=true' \
    --data-urlencode 'f=json' | jq -er '.count')"
trail_raw="$work_dir/florida-hiking-trails-raw.gpkg"
trail_fields='objectid,name,trailnumber,trailtype,hikerpedestrian,trailsurface,seasonopen,primarytrailmaintainer,nationaltraildesignation,lengthmiles,sourceoriginator,publisheddate'
for ((offset = 0; offset < trail_count; offset += page_size)); do
    page="$work_dir/trails-$offset.geojson"
    curl -fsSLG "$trail_api" \
        --data-urlencode "where=$trail_where" \
        --data-urlencode "geometry=$bounds" \
        --data-urlencode 'geometryType=esriGeometryEnvelope' \
        --data-urlencode 'inSR=4326' \
        --data-urlencode 'spatialRel=esriSpatialRelIntersects' \
        --data-urlencode "outFields=$trail_fields" \
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
    ((page_count > 0)) || { echo "Unexpected empty trail page at $offset" >&2; exit 3; }
    if [[ ! -f "$trail_raw" ]]; then
        ogr2ogr -f GPKG "$trail_raw" "$page" -nln hiking_trails -nlt PROMOTE_TO_MULTI \
            -lco SPATIAL_INDEX=YES -lco DESCRIPTION='USGS pedestrian-accessible terra trails intersecting Florida'
    else
        ogr2ogr -f GPKG -update -append "$trail_raw" "$page" \
            -nln hiking_trails -nlt PROMOTE_TO_MULTI
    fi
    printf '  trails %d / %d\n' "$((offset + page_count))" "$trail_count"
done
trail_temp="$work_dir/florida-hiking-trails.gpkg"
trail_output="$destination/USGS-National-Digital-Trails-Florida-overview.gpkg"
ogr2ogr -f GPKG "$trail_temp" "$trail_raw" hiking_trails -nln hiking_trails \
    -clipsrc "$boundary_output" -simplify 0.00015 -nlt PROMOTE_TO_MULTI \
    -lco SPATIAL_INDEX=YES \
    -lco DESCRIPTION='USGS pedestrian-accessible terra trails clipped to Florida'
cp "$trail_temp" "$trail_output"

echo "[4/5] USGS National Map railroads"
rail_count="$(curl -fsSLG "$rail_api" \
    --data-urlencode 'where=1=1' \
    --data-urlencode "geometry=$bounds" \
    --data-urlencode 'geometryType=esriGeometryEnvelope' \
    --data-urlencode 'inSR=4326' \
    --data-urlencode 'spatialRel=esriSpatialRelIntersects' \
    --data-urlencode 'returnCountOnly=true' \
    --data-urlencode 'f=json' | jq -er '.count')"
rail_raw="$work_dir/florida-railroads-raw.gpkg"
rail_fields='objectid,name,railusage,railclassification,railsubdivision,railowner,railownercode,lengthkm,source_originator,loaddate'
for ((offset = 0; offset < rail_count; offset += page_size)); do
    page="$work_dir/rails-$offset.geojson"
    curl -fsSLG "$rail_api" \
        --data-urlencode 'where=1=1' \
        --data-urlencode "geometry=$bounds" \
        --data-urlencode 'geometryType=esriGeometryEnvelope' \
        --data-urlencode 'inSR=4326' \
        --data-urlencode 'spatialRel=esriSpatialRelIntersects' \
        --data-urlencode "outFields=$rail_fields" \
        --data-urlencode 'orderByFields=objectid' \
        --data-urlencode "resultOffset=$offset" \
        --data-urlencode "resultRecordCount=$page_size" \
        --data-urlencode 'returnGeometry=true' \
        --data-urlencode 'outSR=4326' \
        --data-urlencode 'maxAllowableOffset=0.00008' \
        --data-urlencode 'geometryPrecision=6' \
        --data-urlencode 'f=geojson' \
        -o "$page"
    page_count="$(jq -er '.features | length' "$page")"
    ((page_count > 0)) || { echo "Unexpected empty railroad page at $offset" >&2; exit 4; }
    if [[ ! -f "$rail_raw" ]]; then
        ogr2ogr -f GPKG "$rail_raw" "$page" -nln railroads -nlt PROMOTE_TO_MULTI \
            -lco SPATIAL_INDEX=YES -lco DESCRIPTION='USGS National Map railroads intersecting Florida'
    else
        ogr2ogr -f GPKG -update -append "$rail_raw" "$page" \
            -nln railroads -nlt PROMOTE_TO_MULTI
    fi
    printf '  railroads %d / %d\n' "$((offset + page_count))" "$rail_count"
done
rail_temp="$work_dir/florida-railroads.gpkg"
rail_output="$destination/USGS-National-Map-Florida-railroads.gpkg"
ogr2ogr -f GPKG "$rail_temp" "$rail_raw" railroads -nln railroads \
    -clipsrc "$boundary_output" -simplify 0.00008 -nlt PROMOTE_TO_MULTI \
    -lco SPATIAL_INDEX=YES -lco DESCRIPTION='USGS railroads clipped to Florida'
cp "$rail_temp" "$rail_output"

echo "[5/5] Terrain color preview"
color_temp="$work_dir/florida-color.tif"
preview_output="$destination/USGS-3DEP-Florida-State-preview.png"
gdaldem color-relief -q "$terrain_output" "$project_root/maps/terrain-colors.txt" "$color_temp" -alpha
gdal_translate -q -of PNG "$color_temp" "$preview_output"

echo
gdalinfo -json "$terrain_output" | jq '{size, cornerCoordinates, band:(.bands[0] | {type, minimum, maximum})}'
ogrinfo -ro -so "$trail_output" hiking_trails | awk '/Feature Count|Extent/'
ogrinfo -ro -so "$rail_output" railroads | awk '/Feature Count|Extent/'
echo "Pack bytes: $(find "$destination" -type f -exec stat -f %z {} \; | awk '{sum += $1} END {print sum}')"
for file in "$terrain_output" "$preview_output" "$trail_output" "$rail_output" "$boundary_output"; do
    shasum -a 256 "$file"
done
