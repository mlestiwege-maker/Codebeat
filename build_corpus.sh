#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RAW_DIR="$ROOT_DIR/data/raw"
KNOW_DIR="$RAW_DIR/knowledge"
OUT_FILE="$RAW_DIR/corpus.txt"

if [[ ! -d "$KNOW_DIR" ]]; then
  echo "[corpus] knowledge directory not found: $KNOW_DIR" >&2
  exit 1
fi

tmp_file="$(mktemp)"
trap 'rm -f "$tmp_file"' EXIT

# Keep existing base corpus lines first if present.
if [[ -f "$OUT_FILE" ]]; then
  cat "$OUT_FILE" >> "$tmp_file"
fi

# Merge all knowledge files in deterministic order.
while IFS= read -r -d '' f; do
  cat "$f" >> "$tmp_file"
  echo >> "$tmp_file"
done < <(find "$KNOW_DIR" -maxdepth 1 -type f -name '*.txt' -print0 | sort -z)

# Normalize and dedupe (case-insensitive), keep first occurrence order.
awk '
  {
    gsub(/^[[:space:]]+|[[:space:]]+$/, "", $0)
    if ($0 == "") next
    key = tolower($0)
    if (!(key in seen)) {
      seen[key] = 1
      print $0
    }
  }
' "$tmp_file" > "$OUT_FILE"

echo "[corpus] rebuilt: $OUT_FILE"
echo "[corpus] lines: $(wc -l < "$OUT_FILE" | tr -d ' ')"
