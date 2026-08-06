#!/usr/bin/env bash
# Local company CI mirror — run before push / release.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

echo "== format check =="
if [[ -x ./scripts/format.sh ]]; then
  ./scripts/format.sh --check || true
fi

echo "== configure + build =="
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

echo "== ctest =="
ctest --test-dir build --output-on-failure

echo "== prove-shock =="
mkdir -p artifacts
cmake --build build --target prove-shock

echo "== prove-bench (≥1k, break-rate 0) =="
cmake --build build --target prove-bench

echo "== OK: local company gate passed =="
if [[ -f artifacts/equiv_report.json ]]; then
  cat artifacts/equiv_report.json
fi
if [[ -f artifacts/bench_report.json ]]; then
  cat artifacts/bench_report.json
fi
