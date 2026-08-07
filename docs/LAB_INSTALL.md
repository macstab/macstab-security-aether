# Lab install (company / authorized red-team)

Target audience: internal security labs, authorized evaluation hosts, CI runners.

## Requirements

| Item                      | Notes                                                  |
|---------------------------|--------------------------------------------------------|
| CMake ≥ 3.14              |                                                        |
| C++17 compiler            | clang or gcc                                           |
| Network (first configure) | FetchContent pulls Zydis v4.1.1; air-gap: vendor Zydis |
| Linux x86-64 (optional)   | Native EAX dual oracle                                 |
| Docker (optional)         | `Dockerfile.lab-x86_64` for amd64 proof                |

## Quick start

```bash
git clone <your-company-mirror>/aether.git
cd aether
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## Gate commands (use in CI)

```bash
cmake --build build --target prove-unique   # uniqueness
cmake --build build --target prove-equiv    # equivalence JSON
cmake --build build --target prove-shock    # unique + equiv + morph_real
```

Artifact: `artifacts/equiv_report.json` (created when tests run from repo root).

## Lab native (x86-64)

On Apple Silicon / arm64 hosts, native EAX is skipped. For dual oracle:

```bash
docker build -f Dockerfile.lab-x86_64 -t aether-lab .
docker run --rm aether-lab
```

Expect report line: `native x64 checks : N  breaks=0` with `N > 0`.

## Binaries

| Binary              | Role                                     |
|---------------------|------------------------------------------|
| `aether_next`       | Research runtime (idle / fire / cascade) |
| `aether_shock`      | Live uniqueness / permute / cascade demo |
| `aether_disasm`     | Zydis real `.text` dump                  |
| `aether_core_tests` | Unit + gate suite                        |
| `aether_stubs`      | Empty advanced stubs (training only)     |

## Operator rules (non-negotiable)

1. Infection only against files you own or have **written** authorization to modify.
2. Pass a **single explicit path** — no directory walk.
3. Do not deploy to production customer systems as an implant.
4. Record scorecard/JSON when publishing capability claims.

## Install prefix (optional)

```bash
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/opt/aether
cmake --build build --target install
```

## Support model

| Tier            | Expectation                                  |
|-----------------|----------------------------------------------|
| Internal lab    | Build + gates + docs in this repo            |
| Enterprise OEM  | Separate commercial license (see LICENSE §3) |
| Public research | Cite SCORECARD; no “undetectable” claims     |
