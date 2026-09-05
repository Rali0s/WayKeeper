#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
manifest="$project_root/library/fieldcraft-sources.tsv"
pdf_root="$project_root/library/pdfs/fieldcraft"
text_root="$project_root/library/text/fieldcraft"
scratch_root="$project_root/tmp/fieldcraft"
catalog="$project_root/library/catalog.tsv"
provenance="$project_root/library/fieldcraft-provenance.tsv"

for tool in curl pdfinfo pdftotext shasum awk; do
    command -v "$tool" >/dev/null || {
        echo "Required tool is missing: $tool" >&2
        exit 2
    }
done

mkdir -p "$pdf_root" "$text_root" "$scratch_root"
generated_catalog="$scratch_root/generated-catalog.tsv"
filtered_catalog="$scratch_root/filtered-catalog.tsv"
id_list="$scratch_root/ids.txt"
provenance_tmp="$scratch_root/fieldcraft-provenance.tsv"
: > "$generated_catalog"
awk -F '\t' 'NR > 1 {print $1}' "$manifest" > "$id_list"
printf 'id\tsha256\tpages\tdownloaded_on\tsource_url\tpublisher\tsafety_note\n' > "$provenance_tmp"

while IFS=$'\t' read -r id filename title category source_url publisher safety_note; do
    [[ "$id" == "id" ]] && continue
    destination="$pdf_root/$filename"
    temporary="$scratch_root/$filename.part"

    if [[ -f "$destination" && "$(LC_ALL=C head -c 5 "$destination")" == "%PDF-" ]]; then
        echo "Using verified local PDF: $title"
    else
        echo "Downloading: $title"
        curl -fL --http1.1 --retry 4 --retry-all-errors --retry-delay 2 \
            --connect-timeout 20 --max-time 180 \
            -A 'OFF-GRID-Assistant/0.1 (personal offline reference library)' \
            "$source_url" -o "$temporary"
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
    printf '%s\t%s\t%s\t%s\tpdfs/fieldcraft/%s\ttext/fieldcraft/%s.txt\n' \
        "$id" "$title" "$category" "$pages" "$filename" "$id" >> "$generated_catalog"
    printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
        "$id" "$sha256" "$pages" "$(date +%F)" "$source_url" "$publisher" "$safety_note" \
        >> "$provenance_tmp"
    echo "  verified: $pages pages / $sha256"
done < "$manifest"

awk -F '\t' 'NR==FNR {remove[$1]=1; next} FNR==1 || !($1 in remove)' \
    "$id_list" "$catalog" > "$filtered_catalog"
cat "$generated_catalog" >> "$filtered_catalog"
mv "$filtered_catalog" "$catalog"
mv "$provenance_tmp" "$provenance"

echo "Fieldcraft library ready: $(($(wc -l < "$generated_catalog"))) PDFs"
echo "Catalog: $catalog"
echo "Provenance: $provenance"
