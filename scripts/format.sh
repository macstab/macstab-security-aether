#!/usr/bin/env bash
# Format or check C++ sources (no CMake cache required).
# Usage:
#   ./scripts/format.sh           # apply
#   ./scripts/format.sh --check   # dry-run, exit 1 on drift

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

if ! command -v clang-format >/dev/null 2>&1; then
  echo "error: clang-format not found (e.g. brew install clang-format)" >&2
  exit 1
fi

SOURCES=()
while IFS= read -r f; do
  SOURCES+=("$f")
done < <(find src -type f \( -name '*.cpp' -o -name '*.hpp' \) | sort)

if [[ ${#SOURCES[@]} -eq 0 ]]; then
  echo "error: no sources under src/" >&2
  exit 1
fi

if [[ "${1:-}" == "--check" ]]; then
  clang-format --dry-run --Werror --style=file "${SOURCES[@]}"
  echo "format-check: OK (${#SOURCES[@]} files)"
else
  clang-format -i --style=file "${SOURCES[@]}"
  echo "format: applied to ${#SOURCES[@]} files"
fi
