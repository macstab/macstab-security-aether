#!/usr/bin/env bash
# Fetch third-party x86-64 ELFs for real-function morph corpus (lab use only).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DIR="$ROOT/corpus/third_party"
mkdir -p "$DIR"
URL="https://busybox.net/downloads/binaries/1.35.0-x86_64-linux-musl/busybox"
OUT="$DIR/busybox"
if [[ -f "$OUT" ]]; then
  echo "already have $OUT"
  file "$OUT"
  exit 0
fi
echo "fetching $URL"
curl -fsSL -o "$OUT" "$URL"
chmod +x "$OUT"
file "$OUT"
echo "ok: third-party corpus ready"
