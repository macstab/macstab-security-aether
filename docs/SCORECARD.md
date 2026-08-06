# Aether research scorecard (10/10 prototype definition)

**Scope:** open research / Medium-grade polymorphic **initialization** core.  
**Not in scope:** undetectable live stealth, full implant OPSEC, commercial metamorphic engines.

---

## Architecture diagram

```
┌─────────────────────────────────────────────────────────────────┐
│ main  ──always──►  dispatch_slot()     (never rewritten)         │
└───────────────────────────┬─────────────────────────────────────┘
                            │
              default: idle_fn  (time + senseless syscalls only)
                            │
         external trigger SETS slot → bootstrap
                            │
              bootstrap FIRST: clear slot → idle   (JSR edge gone)
                            │
              ┌─────────────▼─────────────┐
              │  CRYPTO CASCADE INIT       │
              │  1. leaf = research impl   │
              │  2. onion wrap × depth     │
              │     (keystream encrypt)    │
              │  3. peel outside-in        │
              │     decrypt → WIPE layer   │
              │  4. wipe final plaintext   │
              └─────────────┬─────────────┘
                            │
              later: main → slot → idle only
```

**Permutation** feeds structural diversity of IR scaffolds (`world_class_permute`,
`permute-heavy` mode): multi-pass shuffles, not a single `std::shuffle`.

---

## Metrics (automated)

| Check | How | Pass criteria |
|-------|-----|----------------|
| **A Uniqueness** | `ctest -R uniqueness` / `aether_core_tests uniqueness` | 200 morph hashes unique; 200 onion hashes unique; permute ≥90% unique / 200 |
| **B Permute** | `ctest -R transforms` | branch targets valid; multi-stream hashes diverge; permute-heavy named |
| **C Cascade** | `ctest -R crypto_cascade` | round-trip depths 1–8; stream involution; 50 unique onions |
| **E Equivalence** | `ctest -R equivalence` / `prove-equiv` | **0** breaks: Edu + multi_stage + Real 1B + cascade peel + real `.text`; JSON `artifacts/equiv_report.json` |
| **F Real 1B morph** | `ctest -R morph_real` / `prove-shock` | Zydis RealFunc-as-IR; pure interpret holds |
| **G Lab native** | `Dockerfile.lab-x86_64` | On amd64: `native_checked > 0`, `native_breaks == 0` |
| **H Morph bench** | `prove-bench` / `aether_bench` | ≥1000 funcs, **break_rate = 0**, JSON `artifacts/bench_report.json` |
| **I Scope** | `docs/SCOPE.md` | Honest arch/format/transform/non-goals string |
| **D Docs** | this file + `done.md` | architecture + residual surfaces documented |

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
cmake --build build --target prove-unique
cmake --build build --target prove-equiv
cmake --build build --target prove-shock
# → artifacts/equiv_report.json
```

---

## What “ALWAYS different” means

| Claim | Meaning |
|-------|---------|
| Practical uniqueness | New entropy each `seed_rng()`; cascade keys/nonces random; multi-pass permute streams |
| Proven | Uniqueness harness fails CI on duplicate hashes |
| Not claimed | Mathematical impossibility of any collision in all possible universes |

---

## Residual detection surfaces (honesty)

| Sensor | Still observes |
|--------|----------------|
| Live debugger / single-step | bootstrap + peel loop |
| EDR / eBPF | CPU burst, allocations, timing vs idle |
| Memory dump mid-peel | current onion layer (until wiped) |
| Static sample of *running* process during init | short-lived buffers |
| After init completes | idle path only; no durable multi-layer body |

**Not a goal:** “impossible to observe live.”  
**Goal met:** short layer lifetime, wipe-on-done, hard offline pre-validation of a fixed poly image.

---

## Score targets (research prototype)

| Pillar | Target |
|--------|--------|
| Multi-strategy permutation | 10/10 *research* (verified diversity) |
| Real crypto cascade | 10/10 *research* (round-trip + wipe semantics) |
| Ephemeral JSR + idle | 10/10 *research* |
| Uniqueness evidence | 10/10 when harness green |
| Live unobservability | out of scope (score N/A) |

When all automated checks pass and this scorecard is kept current, the project may claim:

> **10/10 as a verified open research polymorphic-init prototype**  
> (not 10/10 as an unobservable implant).
