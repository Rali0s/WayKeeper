#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
manifest="$project_root/library/recovery-economy-sources.tsv"
pdf_root="$project_root/library/pdfs/recovery-economy"
text_root="$project_root/library/text/recovery-economy"
scratch_root="$project_root/tmp/recovery-economy"
catalog="$project_root/library/catalog.tsv"
provenance="$project_root/library/recovery-economy-provenance.tsv"

for tool in curl pdfinfo pdftotext shasum awk; do
    command -v "$tool" >/dev/null || { echo "Required tool is missing: $tool" >&2; exit 2; }
done

mkdir -p "$pdf_root" "$text_root" "$scratch_root"
generated="$scratch_root/generated.tsv"
filtered="$scratch_root/filtered.tsv"
ids="$scratch_root/ids.txt"
provenance_tmp="$scratch_root/provenance.tsv"
: > "$generated"
awk -F '\t' 'NR > 1 {print $1}' "$manifest" > "$ids"
printf 'id\tsha256\tpages\tacquired_on\tsource_url\tpublisher\trights_status\tnote\n' > "$provenance_tmp"

while IFS=$'\t' read -r id filename title category source_url download_url publisher note; do
    [[ "$id" == "id" ]] && continue
    destination="$pdf_root/$filename"
    temporary="$scratch_root/$filename.part"
    if [[ ! -f "$destination" || "$(LC_ALL=C head -c 5 "$destination")" != "%PDF-" ]]; then
        curl -fL --http1.1 --retry 4 --retry-all-errors --retry-delay 2 \
            --connect-timeout 20 --max-time 300 \
            "$download_url" -o "$temporary"
        [[ "$(LC_ALL=C head -c 5 "$temporary")" == "%PDF-" ]] || {
            echo "Acquired file is not a PDF: $title" >&2; exit 3;
        }
        mv "$temporary" "$destination"
    fi
    pages="$(pdfinfo "$destination" | awk -F: '$1 == "Pages" {gsub(/[[:space:]]/, "", $2); print $2; exit}')"
    [[ "$pages" =~ ^[0-9]+$ ]] || { echo "Invalid page count: $title" >&2; exit 4; }
    pdftotext -layout "$destination" "$text_root/$id.txt"
    sha256="$(shasum -a 256 "$destination" | awk '{print $1}')"
    rights_status='U.S. GOVERNMENT PUBLICATION / VERIFY EMBEDDED THIRD-PARTY MATERIAL'
    if [[ "$id" == "stlouis-fed-yen-to-trade" ]]; then
        rights_status='COPYRIGHT FRB ST. LOUIS / EDUCATIONAL REPRINT OR PHOTOCOPY PERMISSION WITH CREDIT'
    fi
    printf '%s\t%s\t%s\t%s\tpdfs/recovery-economy/%s\ttext/recovery-economy/%s.txt\n' \
        "$id" "$title" "$category" "$pages" "$filename" "$id" >> "$generated"
    printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
        "$id" "$sha256" "$pages" "$(date +%F)" "$source_url" "$publisher" \
        "$rights_status" "$note" >> "$provenance_tmp"
    echo "Verified $title: $pages pages"
done < "$manifest"

awk -F '\t' 'NR==FNR {remove[$1]=1; next} FNR==1 || !($1 in remove)' \
    "$ids" "$catalog" > "$filtered"
cat "$generated" >> "$filtered"
mv "$filtered" "$catalog"
mv "$provenance_tmp" "$provenance"
echo "Recovery/economy library ready: $(wc -l < "$generated" | tr -d ' ') PDFs"
