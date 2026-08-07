# Aether as a pure library (Macstab GmbH)

**Primary product:** embeddable **x86-64 morph + proof** library.  
**VERSION standing:** 3.0.0 (see `VERSION`).

## What to use (library surface)

| API                                             | Role                                      |
|-------------------------------------------------|-------------------------------------------|
| `aether_morph_buffer` / `_ex`                   | Morph code buffers (Lab / Industry modes) |
| `aether_morph_binary_file`                      | ELF/PE function-level rewrite             |
| `aether_run_bench` / `aether_industry_selftest` | Proof gates                               |
| `aether_version` / `aether_scope`               | Contract                                  |

Headers: `include/aether/aether.h`  
Link: `libaether_core` + Zydis.

## Core engine (C++)

- `MorphEngine` — staged morph pipeline
- `analyze_func_ssa` — function-local SSA-style dataflow
- `x64_sim` — host-independent multi-input oracle
- `binary_rewrite` — size-fit + trampoline grow

## Research extras (optional demos — not the library product)

| Extra                     | Status   | Positioning                    |
|---------------------------|----------|--------------------------------|
| Controlled ELF infect     | Demo     | Not required for library users |
| Crypto cascade            | Research | Optional                       |
| Lazy JSR                  | Research | Optional                       |
| Empty stubs (C2/hollow/…) | Empty    | Must not be sold as features   |

Build demos: `AETHER_BUILD_EXTRAS=ON` (default).  
Core CLIs always: `aether_morph`, `aether_bench`, `aether_disasm`.

## Scores under library contract

| Weakness closed     | To 10 under contract                              |
|---------------------|---------------------------------------------------|
| Analysis-gated only | SSA-style def/use drives shuffle; stack-aware mem |
| PE research-only    | Validate + checksum + section bookkeeping         |
| Rough edges         | Modular ssa/pe/rewrite/sim                        |
| Extras distract     | This doc + CMake extras flag + API focus          |

**Still non-goal:** commercial SEH/TLS/CFG packer for every PE on Earth.
