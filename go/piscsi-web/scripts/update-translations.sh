#!/usr/bin/env bash
# Extract, update, and validate the Go web interface gettext catalogs.

set -euo pipefail

readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
readonly GO_ROOT="$(cd "${PROJECT_DIR}/.." && pwd)"

for required_command in go msgfmt msgmerge; do
  if ! command -v "${required_command}" >/dev/null 2>&1; then
    echo "Error: ${required_command} was not found. Install GNU gettext." >&2
    exit 1
  fi
done

shopt -s nullglob
po_files=("${PROJECT_DIR}"/web/translations/*/LC_MESSAGES/messages.po)
shopt -u nullglob

if ((${#po_files[@]} == 0)); then
  echo "Error: no gettext catalogs found under ${PROJECT_DIR}/web/translations." >&2
  exit 1
fi

work_dir="$(mktemp -d)"
trap 'rm -rf "${work_dir}"' EXIT
generated_pot="${work_dir}/messages.pot"
source_pot="${PROJECT_DIR}/messages.pot"

(
  cd "${GO_ROOT}"
  go run ./piscsi-web/cmd/extract-translations --output "${generated_pot}"
)
msgfmt --check-format --output-file=/dev/null "${generated_pot}"

updated_files=()
updated_count=0
for index in "${!po_files[@]}"; do
  source_po="${po_files[$index]}"
  updated_po="${work_dir}/messages-${index}.po"

  msgmerge "${source_po}" "${generated_pot}" --output-file="${updated_po}"
  msgfmt --check --check-format --output-file=/dev/null "${updated_po}"
  if cmp -s "${source_po}" "${updated_po}"; then
    updated_po="${source_po}"
  else
    updated_count=$((updated_count + 1))
  fi
  updated_files+=("${updated_po}")
done

# Do not replace the POT or any source catalog until every merged file has
# passed validation, avoiding a partially updated catalog set on errors.
if ! cmp -s "${generated_pot}" "${source_pot}"; then
  cp "${generated_pot}" "${source_pot}"
fi
for index in "${!po_files[@]}"; do
  if [[ "${updated_files[$index]}" != "${po_files[$index]}" ]]; then
    cp "${updated_files[$index]}" "${po_files[$index]}"
  fi
done

for po_file in "${po_files[@]}"; do
  locale="$(basename "$(dirname "$(dirname "${po_file}")")")"
  printf '%s: ' "${locale}"
  msgfmt --statistics --output-file=/dev/null "${po_file}"
done

echo "Extracted messages to ${source_pot}."
echo "Updated ${updated_count} and validated ${#po_files[@]} gettext catalogs."
