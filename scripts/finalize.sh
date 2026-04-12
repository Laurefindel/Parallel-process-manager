#!/usr/bin/env bash
set -euo pipefail

output_dir="$1"
result_file="$2"

mapfile -t djvu_files < <(find "$output_dir" -maxdepth 1 -type f -name '*.djvu' | sort)

if [[ ${#djvu_files[@]} -eq 0 ]]; then
    echo "No .djvu files for finalization" >&2
    exit 1
fi

can_use_djvm=false
if command -v djvm >/dev/null 2>&1 && command -v djvudump >/dev/null 2>&1; then
    can_use_djvm=true
    for file in "${djvu_files[@]}"; do
        if ! djvudump "$file" >/dev/null 2>&1; then
            can_use_djvm=false
            break
        fi
    done
fi

if [[ "$can_use_djvm" == "true" ]]; then
    djvm -c "$result_file" "${djvu_files[@]}"
else
    cat "${djvu_files[@]}" > "$result_file"
fi
