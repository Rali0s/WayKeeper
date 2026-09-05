#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
manifest="$project_root/library/society-sources.tsv"
pdf_root="$project_root/library/pdfs/society"
text_root="$project_root/library/text/society"
scratch_root="$project_root/tmp/society"
catalog="$project_root/library/catalog.tsv"
provenance="$project_root/library/society-provenance.tsv"

for tool in curl pdfinfo pdftotext shasum awk perl pandoc tectonic pdfunite; do
    command -v "$tool" >/dev/null || {
        echo "Required tool is missing: $tool" >&2
        exit 2
    }
done

mkdir -p "$pdf_root" "$text_root" "$scratch_root"
generated_catalog="$scratch_root/generated-catalog.tsv"
filtered_catalog="$scratch_root/filtered-catalog.tsv"
id_list="$scratch_root/ids.txt"
provenance_tmp="$scratch_root/society-provenance.tsv"
: > "$generated_catalog"
awk -F '\t' 'NR > 1 {print $1}' "$manifest" > "$id_list"
printf 'id\tsha256\tpages\tdownloaded_on\tsource_url\tpublisher\tsafety_note\n' > "$provenance_tmp"

download() {
    local source="$1"
    local destination="$2"
    curl -fL --http1.1 --retry 4 --retry-all-errors --retry-delay 2 \
        --connect-timeout 20 --max-time 300 \
        -A 'WAYKEEPER/0.1 (personal offline reference library)' \
        "$source" -o "$destination"
}

while IFS=$'\t' read -r id filename title category acquisition source_url download_url publisher safety_note; do
    [[ "$id" == "id" ]] && continue
    destination="$pdf_root/$filename"
    temporary="$scratch_root/$filename.part.pdf"

    if [[ -f "$destination" && "$(LC_ALL=C head -c 5 "$destination")" == "%PDF-" ]]; then
        echo "Using verified local PDF: $title"
    else
        echo "Acquiring: $title"
        case "$acquisition" in
            pdf)
                download "$download_url" "$temporary"
                ;;
            gutenberg-text)
                raw_text="$scratch_root/$id.txt"
                download "$download_url" "$raw_text"
                perl -CSDA -pi -e 's/\x{2122}/(TM)/g; s/\*{6,}/***/g' "$raw_text"
                pandoc "$raw_text" --from=markdown_strict --pdf-engine=tectonic \
                    -V papersize=letter -V geometry:margin=0.55in -V fontsize=8pt \
                    -M title="$title" \
                    -M subtitle="Public-domain text acquired from Project Gutenberg" \
                    -M date="Offline WAYKEEPER edition" \
                    -o "$temporary"
                ;;
            pdf-parts)
                part_number=0
                part_paths=()
                IFS='|' read -r -a part_urls <<< "$download_url"
                for part_url in "${part_urls[@]}"; do
                    part_number=$((part_number + 1))
                    part_path="$scratch_root/$id-part-$part_number.pdf"
                    download "$part_url" "$part_path"
                    part_paths+=("$part_path")
                done
                pdfunite "${part_paths[@]}" "$temporary"
                ;;
            *)
                echo "Unknown acquisition type for $title: $acquisition" >&2
                exit 3
                ;;
        esac
        if [[ "$(LC_ALL=C head -c 5 "$temporary")" != "%PDF-" ]]; then
            echo "Acquired file is not a PDF: $title" >&2
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
    printf '%s\t%s\t%s\t%s\tpdfs/society/%s\ttext/society/%s.txt\n' \
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

echo "Society library ready: $(wc -l < "$generated_catalog" | tr -d ' ') PDFs"
echo "Catalog: $catalog"
echo "Provenance: $provenance"
