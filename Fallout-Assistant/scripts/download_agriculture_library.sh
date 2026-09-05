#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
manifest="$project_root/library/agriculture-sources.tsv"
pdf_root="$project_root/library/pdfs/agriculture"
text_root="$project_root/library/text/agriculture"
scratch_root="$project_root/tmp/agriculture"
catalog="$project_root/library/catalog.tsv"
provenance="$project_root/library/agriculture-provenance.tsv"

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
provenance_tmp="$scratch_root/agriculture-provenance.tsv"
: > "$generated_catalog"
awk -F '\t' 'NR > 1 {print $1}' "$manifest" > "$id_list"
printf 'id\tsha256\tbytes\tpages\tdownloaded_on\tsource_url\tpublisher\tsafety_note\trights_note\n' > "$provenance_tmp"

while IFS=$'\t' read -r id filename title category source_url publisher safety_note rights_note; do
    [[ "$id" == "id" ]] && continue
    destination="$pdf_root/$filename"
    temporary="$scratch_root/$filename.part"

    if [[ -f "$destination" && "$(LC_ALL=C head -c 5 "$destination")" == "%PDF-" ]]; then
        echo "Using verified local PDF: $title"
    else
        echo "Downloading: $title"
        curl -fL --http1.1 --retry 4 --retry-all-errors --retry-delay 2 \
            --connect-timeout 20 --max-time 600 \
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
    [[ -s "$text_root/$id.txt" ]] || {
        echo "No extractable text found: $title" >&2
        exit 6
    }
    sha256="$(shasum -a 256 "$destination" | awk '{print $1}')"
    bytes="$(wc -c < "$destination" | tr -d ' ')"
    printf '%s\t%s\t%s\t%s\tpdfs/agriculture/%s\ttext/agriculture/%s.txt\n' \
        "$id" "$title" "$category" "$pages" "$filename" "$id" >> "$generated_catalog"
    printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
        "$id" "$sha256" "$bytes" "$pages" "$(date +%F)" "$source_url" "$publisher" \
        "$safety_note" "$rights_note" >> "$provenance_tmp"
    echo "  verified: $pages pages / $bytes bytes / $sha256"
done < "$manifest"

awk -F '\t' 'NR==FNR {remove[$1]=1; next} FNR==1 || !($1 in remove)' \
    "$id_list" "$catalog" > "$filtered_catalog"
cat "$generated_catalog" >> "$filtered_catalog"
mv "$filtered_catalog" "$catalog"
mv "$provenance_tmp" "$provenance"

echo "Agriculture library ready: $(wc -l < "$generated_catalog" | tr -d ' ') PDFs"
echo "Catalog: $catalog"
echo "Provenance: $provenance"
