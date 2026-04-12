#!/usr/bin/env bash
set -euo pipefail

output_dir="$1"
result_file="$2"

mapfile -t txt_files < <(find "$output_dir" -maxdepth 1 -type f -name '*.txt' | sort)

if [[ ${#txt_files[@]} -eq 0 ]]; then
    echo "No .txt files for finalization" >&2
    exit 1
fi

: > "$result_file"
for file in "${txt_files[@]}"; do
    printf '===== %s =====\n' "$(basename "$file")" >> "$result_file"
    cat "$file" >> "$result_file"
    printf '\n\n' >> "$result_file"
done
