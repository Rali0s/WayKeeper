#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
collection_root="$project_root/library/plants-herbs"
manifest="$collection_root/sources.tsv"
pdf_root="$collection_root/pdfs"
text_root="$collection_root/text"
scratch_root="$project_root/tmp/pdfs"

for tool in curl jq pdfinfo pdftotext shasum; do
    command -v "$tool" >/dev/null || {
        echo "Required tool is missing: $tool" >&2
        exit 2
    }
done

mkdir -p "$pdf_root" "$text_root" "$scratch_root"

resolve_who_pdf() {
    local handle="$1"
    local remote_filename="$2"
    local item_uuid original_uuid
    item_uuid="$(curl -fsSLG 'https://iris.who.int/server/api/pid/find' \
        --data-urlencode "id=$handle" | jq -er '.uuid')"
    original_uuid="$(curl -fsSL \
        "https://iris.who.int/server/api/core/items/$item_uuid/bundles?size=100" | \
        jq -er '._embedded.bundles[] | select(.name == "ORIGINAL") | .uuid')"
    curl -fsSL \
        "https://iris.who.int/server/api/core/bundles/$original_uuid/bitstreams?size=100" | \
        jq -er --arg name "$remote_filename" \
        '._embedded.bitstreams[] | select(.name == $name) | ._links.content.href'
}

checksums_temp="$scratch_root/plants-herbs-checksums.tsv"
printf 'id\tsha256\tpages\tdownloaded_on\n' > "$checksums_temp"

while IFS=$'\t' read -r id filename title publisher year trust_tier category scope \
    source_url download_method download_locator remote_filename license_note safety_role; do
    [[ "$id" == "id" ]] && continue
    destination="$pdf_root/$filename"
    temporary="$scratch_root/$filename.part"
    if [[ -f "$destination" && "$(LC_ALL=C head -c 5 "$destination")" == "%PDF-" ]]; then
        echo "Using verified local PDF: $title"
    else
        if [[ "$download_method" == "who_dspace" ]]; then
            download_url="$(resolve_who_pdf "$download_locator" "$remote_filename")"
        elif [[ "$download_method" == "http" ]]; then
            download_url="$download_locator"
        else
            echo "Unknown download method for $id: $download_method" >&2
            exit 3
        fi

        echo "Downloading: $title"
        curl -fL --retry 3 --retry-delay 2 "$download_url" -o "$temporary"
        if [[ "$(LC_ALL=C head -c 5 "$temporary")" != "%PDF-" ]]; then
            echo "Downloaded file is not a PDF: $title" >&2
            exit 4
        fi
        mv "$temporary" "$destination"
    fi
    pages="$(pdfinfo "$destination" | awk -F: '$1 == "Pages" {gsub(/[[:space:]]/, "", $2); print $2; exit}')"
    [[ "$pages" =~ ^[0-9]+$ ]] || {
        echo "Could not verify page count: $title" >&2
        exit 5
    }
    pdftotext -layout "$destination" "$text_root/$id.txt"
    sha256="$(shasum -a 256 "$destination" | awk '{print $1}')"
    printf '%s\t%s\t%s\t%s\n' "$id" "$sha256" "$pages" "$(date +%F)" >> "$checksums_temp"
    echo "  verified: $pages pages / $sha256"
done < "$manifest"

mv "$checksums_temp" "$collection_root/checksums.tsv"
python3 "$project_root/scripts/build_plants_herbs_db.py"
