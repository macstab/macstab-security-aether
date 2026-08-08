# Aether

**Lab morph engine + proof** by **Macstab GmbH** — generate metamorphic variants under a documented scope and **prove** they do not break semantics. For authorized research, training, and **scanner/detection stress tests**. Not an undetectable implant.

| | |
|--|--|
| **Version** | **3.0.0 Industry Morph Framework** (scoped lab — see `docs/INDUSTRY_STATUS.md`) |
| **Vendor** | Macstab GmbH |
| **License** | Macstab GmbH lab / company research terms — see `LICENSE` |
| **Scope contract** | [`docs/SCOPE.md`](docs/SCOPE.md) |
| **Status** | Final lab product freeze (no C2/spread features) |

---

## For leadership (one paragraph)

Aether demonstrates **multi-strategy code morphing** with a **hard CI gate**: generations must stay **semantically equivalent** (0 breaks) while remaining **practically unique**. It includes real x86-64 disassembly (Zydis), a restricted real-IR morph path, crypto onion cascade research, and controlled single-file ELF demos. Suitable for **internal labs, training, and dual-use detection R&D** — not for unauthorized offensive use or “undetectable malware” claims.

Library focus: [`docs/LIBRARY.md`](docs/LIBRARY.md) · Details: [`docs/PRODUCT.md`](docs/PRODUCT.md) · gates: [`docs/SCORECARD.md`](docs/SCORECARD.md) · install: [`docs/LAB_INSTALL.md`](docs/LAB_INSTALL.md)

---

## CI / release

GitHub Actions pipelines are adapted from the working **Macstab chaos-testing-libraries** release flow (CI + tag release). See `docs/RELEASE.md`.

## Quick start (one path)

```bash
./scripts/bootstrap.sh          # install path
./scripts/industry_end.sh       # full Lab Industry Complete gate
```

That configures, builds, runs unit tests, `prove-shock`, and `prove-bench` (must exit 0).

```bash
./build/aether_shock              # live demo
./build/aether_disasm victim_clean
./build/aether_bench --count 1000 # ≥1k + ELF corpus, JSON report
./build/aether_morph --hex B807000000C3 --out morph.bin --policy safe
```

**Lab native EAX (required for release; x86-64 Docker):**

```bash
docker build -f Dockerfile.lab-x86_64 -t aether-lab .
docker run --rm aether-lab
```

**Air-gap:** [`docs/AIRGAP.md`](docs/AIRGAP.md) · `./scripts/vendor_zydis.sh`

---

## Product surface

| Binary | Purpose |
|--------|---------|
| `aether_next` | Research runtime: idle noise, optional JSR arm, cascade peel |
| `aether_shock` | Spectacle demo: uniqueness, permute, cascade |
| `aether_disasm` | Zydis real `.text` / blob IR dump |
| `aether_core_tests` | Unit tests + selectable gates |
| `aether_stubs` | Empty advanced stubs (training labels only) |

| Gate target | Proves |
|-------------|--------|
| `prove-unique` | Always-different morph / cascade hashes |
| `prove-equiv` | Multi-domain equivalence → `artifacts/equiv_report.json` |
| `prove-shock` | unique + equiv + morph_real |
| `prove-bench` / `aether_bench` | ≥1k corpus, break-rate 0, JSON report |
| `docs/SCOPE.md` | Honest capability scope string |
| C API `include/aether/aether.h` | Stable morph + bench entry points |

---

## Repository layout

```
include/aether/ public C API (aether.h)
lib/aether/     library implementation
apps/           CLI entrypoints
docs/REPOSITORY_LAYOUT.md  full tree
tests/          unit + gate suite
docs/           PRODUCT, LAB_INSTALL, SCORECARD, plans
.github/        CI workflows
Dockerfile.lab-x86_64
```

---

## Company policies (built-in)

1. **Infection** only touches the **single path** named on the CLI.  
2. No automatic spread, directory walk, or network C2 in this product.  
3. Capability claims must match **SCORECARD / JSON gates**.  
4. No warranty of undetectability or production safety.

See `LICENSE`, `SECURITY.md`, `CONTRIBUTING.md`.

---

## Documentation map

| Doc | Audience |
|-----|----------|
| `docs/PRODUCT.md` | PM / leadership / sales engineering |
| `docs/LAB_INSTALL.md` | Lab engineers |
| `docs/SCORECARD.md` | Metrics & residual surfaces |
| `docs/BRAINSTORMING.md` | Roadmap / next leaps |
| `docs/IMPLEMENTATION_PLAN.md` | Technical backlog |
| `AGENTS.md` | Contributors / automation |

---

## License & acceptance (required)

| | |
|--|--|
| **Agreement** | Root file [`LICENSE`](LICENSE) (binding terms) |
| **How to accept** | [`docs/LICENSE_ACCEPTANCE.md`](docs/LICENSE_ACCEPTANCE.md) |
| **CI / shell** | `export AETHER_LICENSE_ACCEPTED=I_ACCEPT_AETHER_LICENSE` |
| **C API** | `aether_accept_license()` after operator review |

By building or running Aether you accept `LICENSE`. Source files carry short SPDX headers pointing to that file.

## Disclaimer

Educational and **authorized** defensive / red-team **lab** use only (lawful private / company use as defined in `LICENSE`).  
You are responsible for legal compliance. Authors provide **no warranty**.
