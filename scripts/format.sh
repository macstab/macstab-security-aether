#!/usr/bin/env bash
# Format sources with the SAME major as CI (clang-format-18).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

if command -v clang-format-18 >/dev/null 2>&1; then
  CF=clang-format-18
elif [[ -x /opt/homebrew/opt/llvm@18/bin/clang-format ]]; then
  CF=/opt/homebrew/opt/llvm@18/bin/clang-format
elif command -v docker >/dev/null 2>&1; then
  echo "Using docker ubuntu:24.04 clang-format-18 (matches CI)..."
  exec docker run --rm -v "$ROOT":/src -w /src ubuntu:24.04 bash -c '
    apt-get update -qq && apt-get install -y -qq clang-format-18 >/dev/null
    mapfile -t F < <(find lib apps include tests stubs -type f \( -name "*.cpp" -o -name "*.hpp" -o -name "*.h" -o -name "*.c" \) | sort)
    clang-format-18 -i --style=file "${F[@]}"
    clang-format-18 --dry-run --Werror --style=file "${F[@]}"
    echo OK
  '
else
  echo "Need clang-format-18 (CI pin). Install: brew install llvm@18" >&2
  echo "  or: apt install clang-format-18" >&2
  exit 1
fi

mapfile -t FILES < <(find lib apps include tests stubs -type f \( -name '*.cpp' -o -name '*.hpp' -o -name '*.h' -o -name '*.c' \) | sort)
echo "Formatting ${#FILES[@]} files with $($CF --version | head -1)"
$CF -i --style=file "${FILES[@]}"
$CF --dry-run --Werror --style=file "${FILES[@]}"
echo "format-check OK ($CF)"
