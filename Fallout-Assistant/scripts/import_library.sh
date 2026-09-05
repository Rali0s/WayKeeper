#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source_root="${1:-${project_root}/../Survival-Library}"
library_root="${project_root}/library"

for tool in pdftotext pdfinfo; do
    if ! command -v "${tool}" >/dev/null 2>&1; then
        echo "Missing ${tool}. On macOS: brew install poppler" >&2
        exit 1
    fi
done

mkdir -p "${library_root}/pdfs" "${library_root}/text"
catalog_tmp="${library_root}/catalog.tsv.tmp"
printf 'id\ttitle\tcategory\tpages\tpdf\ttext\n' > "${catalog_tmp}"

while IFS= read -r -d '' source_pdf; do
    filename="$(basename "${source_pdf}")"
    stem="${filename%.pdf}"
    id="$(printf '%s' "${stem}" | tr '[:upper:] ' '[:lower:]-' | tr -cd 'a-z0-9._-')"
    category="$(basename "$(dirname "${source_pdf}")")"
    destination_pdf="${library_root}/pdfs/${filename}"
    destination_text="${library_root}/text/${stem}.txt"

    cp -c "${source_pdf}" "${destination_pdf}" 2>/dev/null || cp "${source_pdf}" "${destination_pdf}"
    pdftotext -layout "${destination_pdf}" "${destination_text}"

    title="$(pdfinfo "${destination_pdf}" | awk -F: '$1 == "Title" {sub(/^[[:space:]]+/, "", $2); print $2; exit}')"
    pages="$(pdfinfo "${destination_pdf}" | awk -F: '$1 == "Pages" {gsub(/[[:space:]]/, "", $2); print $2; exit}')"
    if [[ -z "${title}" || "${title}" == "Chapter 1" ]]; then title="${stem//-/ }"; fi
    title="${title//$'\t'/ }"

    printf '%s\t%s\t%s\t%s\tpdfs/%s\ttext/%s.txt\n' \
        "${id}" "${title}" "${category}" "${pages}" "${filename}" "${stem}" >> "${catalog_tmp}"
    echo "Imported: ${title} (${pages} pages)"
done < <(find "${source_root}" -type f -iname '*.pdf' -print0 | sort -z)

mv "${catalog_tmp}" "${library_root}/catalog.tsv"
echo "Library ready: ${library_root}/catalog.tsv"

