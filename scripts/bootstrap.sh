export AETHER_LICENSE_ACCEPTED=I_ACCEPT_AETHER_LICENSE
#!/usr/bin/env bash
# One install path: configure → build → unit tests → prove-shock → prove-bench
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

SKIP_CFG=0
if [[ "${1:-}" == "--skip-configure" ]]; then
  SKIP_CFG=1
fi

echo "== Aether bootstrap (final install path) =="
echo "version: $(cat VERSION 2>/dev/null || echo unknown)"

# Optional third-party corpus (best-effort; offline ok if already present)
if [[ -x ./scripts/fetch_third_party_corpus.sh ]]; then
  ./scripts/fetch_third_party_corpus.sh || echo "(third-party corpus skip)"
fi
# Ensure generated corpora exist
python3 scripts/gen_corpus_elf.py 1000 >/dev/null 2>&1 || true
python3 scripts/gen_corpus_pe.py 400 >/dev/null 2>&1 || true

if [[ "$SKIP_CFG" -eq 0 ]]; then
  echo "== cmake configure =="
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
fi

echo "== build =="
cmake --build build -j

echo "== ctest =="
ctest --test-dir build --output-on-failure

echo "== prove-shock =="
mkdir -p artifacts
cmake --build build --target prove-shock

echo "== prove-bench =="
cmake --build build --target prove-bench

echo "== OK: install path green =="
if [[ -f artifacts/bench_report.json ]]; then
  echo "--- bench_report.json ---"
  cat artifacts/bench_report.json
fi
