export AETHER_LICENSE_ACCEPTED=I_ACCEPT_AETHER_LICENSE
#!/usr/bin/env bash
# Lab-Industry-Complete gate for Aether (product "end" of this generation).
# This is NOT closed-industry malware-engine parity — see docs/INDUSTRY_STATUS.md
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

echo "======== AETHER LAB INDUSTRY COMPLETE GATE ========"
echo "version: $(cat VERSION)"

./scripts/fetch_third_party_corpus.sh || true
python3 scripts/gen_corpus_elf.py 1000
python3 scripts/gen_corpus_pe.py 400 || true

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

ctest --test-dir build --output-on-failure
mkdir -p artifacts
cmake --build build --target prove-shock
cmake --build build --target prove-industry

# Validate bench JSON
python3 - <<'PY'
import json, sys
from pathlib import Path
p = Path("artifacts/bench_report.json")
assert p.exists(), "missing bench_report.json"
r = json.loads(p.read_text())
assert r.get("pass") is True, r
assert r.get("break_rate", 1) == 0, r
assert r.get("corpus_total", 0) >= 1000, r
assert r.get("corpus_elf", 0) >= 200, r
print("bench JSON OK:", r.get("corpus_total"), "funcs, elf=", r.get("corpus_elf"))
PY

# Smoke morph CLI
./build/aether_morph --hex B807000000C3 --out artifacts/smoke_morph.bin --policy safe
./build/aether_morph --hex B807000000C3 --out artifacts/smoke_morph_lab.bin --policy lab

echo "======== LAB INDUSTRY COMPLETE: PASS ========"
echo "Closed-industry full morpher: NOT claimed (docs/INDUSTRY_STATUS.md)"
cat artifacts/bench_report.json
