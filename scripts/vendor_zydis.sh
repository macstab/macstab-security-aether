#!/usr/bin/env bash
# Vendor Zydis for air-gapped builds (one command online).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="$ROOT/third_party/zydis"
TAG=v4.1.1

mkdir -p "$ROOT/third_party"
if [[ -d "$DEST/.git" ]]; then
  echo "updating existing $DEST"
  git -C "$DEST" fetch --depth 1 origin tag "$TAG"
  git -C "$DEST" checkout "$TAG"
else
  rm -rf "$DEST"
  git clone --depth 1 --branch "$TAG" https://github.com/zyantific/zydis.git "$DEST"
fi

# Zydis needs Zycore submodule
git -C "$DEST" submodule update --init --depth 1

echo "Vendored Zydis at $DEST"
echo "Configure with:"
echo "  cmake -S . -B build -DAETHER_ZYDIS_ROOT=$DEST"
