# Support — Macstab GmbH / Support

## How to report bugs

1. Include **VERSION** (`cat VERSION` or `aether_version()`).
2. Host OS / arch (`uname -a`).
3. Exact command that failed (`prove-bench`, `aether_morph_buffer`, etc.).
4. Logs / JSON: `artifacts/bench_report.json`, `artifacts/equiv_report.json` if present.
5. Minimal input bytes or ELF path (lab samples only — **no third-party malware dumps** unless you own the rights).

Open an issue on your company mirror or contact the maintainer email on the release.

## What we fix

| In support                            | Out of support                                 |
|---------------------------------------|------------------------------------------------|
| Build/install on documented platforms | Custom proprietary toolchains without a repro  |
| `break_rate != 0` on shipped gates    | Morph of out-of-scope code (SSE, PE, full ABI) |
| Crashes in SAFE morph / bench / tests | “Make it undetectable” requests                |
| Docs mismatches with SCOPE            | Feature requests for C2 / spread / implant     |
| CI false failures on clean machine    | Legal advice for your deployment               |

## Response targets (best effort unless contract)

| Severity                  | Target          |
|---------------------------|-----------------|
| Gate red on clean install | 5 business days |
| API regression            | next patch      |
| Docs / confusion          | best effort     |

Enterprise SLA only under a **paid support contract** (see LICENSE §3).

## Out of scope requests (will be closed)

- Auto-spread, directory infection, network C2
- Claims or features marketed as undetectable
- Support for unauthorized testing of third-party systems

## Scope contract

All support assumes `docs/SCOPE.md`. If it is not in scope there, it is not a defect.
