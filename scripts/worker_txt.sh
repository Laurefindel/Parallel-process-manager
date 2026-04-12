#!/usr/bin/env bash
set -euo pipefail

input_file="$1"
output_dir="$2"

mkdir -p "$output_dir"
cp "$input_file" "$output_dir/$(basename "$input_file")"
