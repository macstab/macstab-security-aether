# Changelog

All notable changes to Aether are documented in this file.
Format based on [Keep a Changelog](https://keepachangelog.com/).

## [Unreleased]

### Changed
- Copyright / vendor branding: **Macstab GmbH**


### Changed
- Repository restructure: `include/aether/` (public API), `lib/aether/` (impl), `apps/`, `stubs/`
- See `docs/REPOSITORY_LAYOUT.md`


### Added (library polish to contract 10)
- Function-local **SSA-style dataflow** (`ssa_analysis`) driving safe shuffle
- PE rewrite hygiene: `pe64_validate`, `pe64_fix_checksum`, richer `PeTextRegion`
- `docs/LIBRARY.md` pure-library positioning; `AETHER_BUILD_EXTRAS` for demo binaries
- prove-industry includes `ssa_analysis` unit gate


### Added (industry path — no VERSION bump until finish)
- Industry phases I–V (in tree): effect model, Memory/PureGpr, multi-input Industry product + native multi-input
- Function-level ELF/PE rewrite: size_fit + trampoline section grow (`binary_rewrite`)
- Host-independent `x64_sim` multi-input oracle (Apple Silicon native_checked > 0)
- `industry_finish_selftest` / `aether_industry_selftest`; prove-industry includes rewrite gate
- MorphDomain + MorphEngineConfig (`size_fit`, `verify_native`, multi-input, size ratio)

### Notes
- Standing product version remains **1.0.0** until an explicit release cut
- Closed-industry **10** under industry contract (trampoline grow, x64_sim oracle, mem/stack domain)
- Absolute any-binary SEH/TLS packer remains non-goal

## [1.0.0] — 2026-08-07

### Added
- **MorphEngine framework**: staged pipeline (lift→analyze→transforms→assemble→verify), batch API
- `prove-industry` gate; `aether_framework_selftest`; multi-policy pure verify
- Industry morph framework raises architecture to product-grade pipeline design

### Notes
- 1.0.0 = industry-**style framework** for scoped x86-64 lab morph — still not arbitrary closed-industry rewriter

### Added
- **Lab Industry Complete** generation: `scripts/industry_end.sh`, third-party busybox corpus fetch
- Multi-arg native `try_exec_x64_eax_args`; PE corpus generator; INDUSTRY_STATUS end definition
- API major **1.0.0** (Lab Complete — not closed-industry morph parity)

### Notes
- 1.0.0 freezes “lab product end.” Closed-industry full rewriter remains a future generation.

### Added
- Multi-arg pure oracle (mov/add via RDI/RSI) in corpus + interpret
- `real_split_blocks` for larger CFG permute surface
- PE32+ `.text` loader (`pe_view`) for multi-format extract
- C API `aether_morph_file`; version **1.0.0**

## [1.0.0] — 2026-08-07

### Added
- Industry morph path: encoding diversify, safe insn shuffle, `MorphPolicy` identity/safe/lab
- CLI `aether_morph` (--in/--hex/--out/--policy)
- Bench metrics: avg_size_ratio, elapsed_ms; larger shipped ELF corpus (~600+)

### Changed
- `morph_real()` is the main entry; `morph_real_restricted` = Safe policy
- C API 1.0.0: LAB aggression runs extra passes

### Added
- Final product freeze path: `scripts/bootstrap.sh`, `SUPPORT.md`, `docs/RELEASE.md`, `docs/AIRGAP.md`
- Shipped ELF corpus `corpus/real_corpus.elf` (~400 funcs) + multi-ELF extract
- Bench pass requires **elf≥200** and **break_rate=0**
- API: `aether_api_version` / `aether_api_compatible`; version **1.2.0**
- Lab Docker / CI: native EAX must be non-zero; prove-bench in image

### Changed
- SCOPE.md is the binding product contract (sell: lab morph + proof only)
- Feature freeze: no new C2/stub offense features

## [1.0.0] — 2026-08-07

### Added
- Morph bench M1: ≥1k pure corpus + ELF extract, break-rate gate, `aether_bench` / `prove-bench`
- Analysis-gated real morph (`real_analysis`), stable C API (`aether/api/aether.h`)
- Honest scope: `docs/SCOPE.md`; air-gap Zydis via `AETHER_ZYDIS_ROOT`
- Equivalence oracle campaign (EduPureRax, RealRestricted 1B, CascadeLeaf, RealText)
- Zydis-backed real IR morph path (`morph_real_*`)
- Def-use gated intra-block permute (`safe_permute_insns`)
- Multi-stage re-lift normalize (`normalize_reachable`)
- Assemble fallthrough sealing for CFG layout
- `prove-equiv` / `prove-shock` CMake targets + JSON artifact
- Lab Docker image `Dockerfile.lab-x86_64` (native EAX dual oracle)
- Company packaging: LICENSE, SECURITY, CONTRIBUTING, CI, lab install guide

### Changed
- Morph stages strip unreachable tails before re-disasm between generations
- SCORECARD metrics E/F/G for equivalence, real morph, lab native

### Fixed
- Linux build: include `time.h` for `nanosleep`
- Silent jump fixup failures now refuse empty assemble output

## [1.0.0] — 2026

### Added
- Educational metamorphic pipeline, cascade, uniqueness harness
- Controlled single-file ELF infection
- Step 1 Zydis real disassembly + ELF `.text` loader
- `aether_shock` spectacle demo
