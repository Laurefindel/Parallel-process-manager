#!/usr/bin/env bash
set -euo pipefail

input_file="$1"
output_dir="$2"

mkdir -p "$output_dir"

base_name="$(basename "$input_file")"
name_no_ext="${base_name%.*}"
out_djvu="$output_dir/$name_no_ext.djvu"

if command -v gm >/dev/null 2>&1 && command -v cjb2 >/dev/null 2>&1; then
    tmp_pbm="$output_dir/$name_no_ext.pbm"
    gm convert "$input_file" "$tmp_pbm"
    cjb2 "$tmp_pbm" "$out_djvu"
    rm -f "$tmp_pbm"
else
    cp "$input_file" "$out_djvu"
fi
