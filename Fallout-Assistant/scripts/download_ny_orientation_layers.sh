#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
destination="$repo_root/maps/ny"
boundary="$destination/USGS-National-Map-New-York-boundary.geojson"
work_dir="$(mktemp -d /tmp/waykeeper-ny-orientation.XXXXXX)"
trap 'rm -rf "$work_dir"' EXIT

for command in curl jq ogr2ogr; do
    command -v "$command" >/dev/null || { echo "missing required command: $command" >&2; exit 2; }
done
[[ -f "$boundary" ]] || { echo "missing state boundary: $boundary" >&2; exit 2; }

bbox='-79.7624,40.4961,-71.8562,45.0159'
page_size=2000

download_layer() {
    local endpoint="$1" where="$2" fields="$3" layer="$4" output="$5"
    local count offset page page_count
    count="$(curl -fsSLG "$endpoint/query" \
        --data-urlencode "where=$where" \
        --data-urlencode "geometry=$bbox" \
        --data-urlencode 'geometryType=esriGeometryEnvelope' \
        --data-urlencode 'inSR=4326' \
        --data-urlencode 'spatialRel=esriSpatialRelIntersects' \
        --data-urlencode 'returnCountOnly=true' \
        --data-urlencode 'f=json' | jq -er '.count')"
    echo "$layer: $count source features"
    for ((offset = 0; offset < count; offset += page_size)); do
        page="$work_dir/$layer-$offset.geojson"
        curl -fsSLG "$endpoint/query" \
            --data-urlencode "where=$where" \
            --data-urlencode "geometry=$bbox" \
            --data-urlencode 'geometryType=esriGeometryEnvelope' \
            --data-urlencode 'inSR=4326' \
            --data-urlencode 'spatialRel=esriSpatialRelIntersects' \
            --data-urlencode "outFields=$fields" \
            --data-urlencode 'returnGeometry=true' \
            --data-urlencode 'outSR=4326' \
            --data-urlencode "resultOffset=$offset" \
            --data-urlencode "resultRecordCount=$page_size" \
            --data-urlencode 'f=geojson' > "$page"
        page_count="$(jq -er '.features | length' "$page")"
        ((page_count > 0)) || { echo "empty $layer page at $offset" >&2; exit 3; }
        if [[ ! -f "$output" ]]; then
            ogr2ogr -f GPKG "$output" "$page" -nln "$layer" -nlt PROMOTE_TO_MULTI \
                -lco SPATIAL_INDEX=YES
        else
            ogr2ogr -f GPKG -update -append "$output" "$page" -nln "$layer" \
                -nlt PROMOTE_TO_MULTI
        fi
        printf '  %d / %d\n' "$((offset + page_count))" "$count"
    done
}

road_raw="$work_dir/roads-raw.gpkg"
download_layer \
    'https://carto.nationalmap.gov/arcgis/rest/services/transportation/MapServer/8' \
    '1=1' \
    'OBJECTID,PRIME_NAME,NAME,INTERSTATE,US_ROUTE,ST_ROUTE,TYPE,MILES' \
    'major_roads' "$road_raw"
road_output="$destination/USGS-National-Map-New-York-primary-roads.gpkg"
ogr2ogr -f GPKG "$work_dir/roads.gpkg" "$road_raw" major_roads \
    -nln major_roads -clipsrc "$boundary" -simplify 0.0002 \
    -nlt PROMOTE_TO_MULTI \
    -lco SPATIAL_INDEX=YES -lco DESCRIPTION='USGS major roads clipped to New York State'
ogr2ogr -f GPKG "$work_dir/road-markers.gpkg" "$work_dir/roads.gpkg" \
    -dialect SQLite \
    -sql "SELECT PRIME_NAME, ST_PointOnSurface(geom) AS geom FROM major_roads WHERE PRIME_NAME IS NOT NULL" \
    -nln road_markers -nlt POINT -lco SPATIAL_INDEX=YES
ogr2ogr -f GPKG -update "$work_dir/roads.gpkg" "$work_dir/road-markers.gpkg" road_markers \
    -nln road_markers -nlt POINT -lco SPATIAL_INDEX=YES
cp "$work_dir/roads.gpkg" "$road_output"

town_raw="$work_dir/towns-raw.gpkg"
download_layer \
    'https://carto.nationalmap.gov/arcgis/rest/services/geonames/MapServer/1' \
    "state_alpha='NY'" \
    'OBJECTID,gaz_id,gaz_name,state_alpha,county_name' \
    'towns' "$town_raw"
town_output="$destination/USGS-GNIS-New-York-towns.gpkg"
ogr2ogr -f GPKG "$work_dir/towns.gpkg" "$town_raw" towns \
    -nln towns -clipsrc "$boundary" \
    -nlt PROMOTE_TO_MULTI \
    -lco SPATIAL_INDEX=YES -lco DESCRIPTION='USGS GNIS incorporated-place landmarks in New York'
cp "$work_dir/towns.gpkg" "$town_output"

hydro_raw="$work_dir/hydrography-raw.gpkg"
download_layer \
    'https://hydro.nationalmap.gov/arcgis/rest/services/nhd/MapServer/4' \
    'GNIS_NAME IS NOT NULL AND StreamOrde >= 6' \
    'OBJECTID,GNIS_ID,GNIS_NAME,LENGTHKM,FTYPE,StreamOrde' \
    'rivers' "$hydro_raw"
download_layer \
    'https://hydro.nationalmap.gov/arcgis/rest/services/nhd/MapServer/10' \
    'AREASQKM >= 5' \
    'OBJECTID,GNIS_ID,GNIS_NAME,AREASQKM,FTYPE' \
    'waterbodies' "$hydro_raw"
download_layer \
    'https://hydro.nationalmap.gov/arcgis/rest/services/nhd/MapServer/7' \
    'AREASQKM >= 5' \
    'OBJECTID,GNIS_ID,GNIS_NAME,AREASQKM,FTYPE' \
    'water_areas' "$hydro_raw"
hydro_output="$destination/USGS-NHD-New-York-hydrography.gpkg"
first=1
for layer in rivers waterbodies water_areas; do
    if ((first)); then
        ogr2ogr -f GPKG "$work_dir/hydrography.gpkg" "$hydro_raw" "$layer" \
            -nln "$layer" -clipsrc "$boundary" -simplify 0.0002 \
            -nlt PROMOTE_TO_MULTI -lco SPATIAL_INDEX=YES
        first=0
    else
        ogr2ogr -f GPKG -update "$work_dir/hydrography.gpkg" "$hydro_raw" "$layer" \
            -nln "$layer" -clipsrc "$boundary" -simplify 0.0002 \
            -nlt PROMOTE_TO_MULTI -lco SPATIAL_INDEX=YES
    fi
done
cp "$work_dir/hydrography.gpkg" "$hydro_output"

for file in "$road_output" "$town_output" "$hydro_output"; do
    shasum -a 256 "$file"
done
