# Aether — product scope (contract)

**This file is the scope contract for licensees, CI, and marketing.**  
**Version:** 3.0.0 (keep in sync with `VERSION` / `aether.h`)

**Product:** Aether — lab metamorphic research engine  
**Claim level:** research / company lab (not closed-industry full binary rewriter)

---

## One-line scope (use externally)

> Aether morphs **x86-64** code under an **analysis-gated** policy, proves **0 semantic breaks** on a **≥1k
pure-function corpus** plus **structural integrity** on **≥200 ELF-extracted functions**, with **native EAX** dual
> oracle on lab x86-64 — **lab morph + proof only**, not “undetectable.”

---

## In scope

| Item                   | Detail                                                                                                  |
|------------------------|---------------------------------------------------------------------------------------------------------|
| **ISA**                | x86-64 only                                                                                             |
| **Formats**            | ELF64 + **PE32+** `.text` extract; raw code buffers for morph                                           |
| **IR**                 | (1) Educational `Op` IR (2) Zydis `RealFunc` as IR                                                      |
| **Transforms (edu)**   | expand/shrink, diversify, block permute, flatten, multi-stage (relift-safe)                             |
| **Transforms (real)**  | SSA-style dataflow-gated morph: NOPs + permute + encoding diversify + safe shuffle + rel32; stack-aware |
| **Equivalence domain** | Pure RAX-return (+ multi-arg RDI/RSI pure); interpret + native no-JE on x86-64                          |
| **Structural domain**  | ELF-extracted functions: non-empty morph, re-liftable CFG                                               |
| **Corpus gate**        | ≥1000 total functions; **≥200 from ELF** (`victim_clean` + `corpus/real_corpus.elf`)                    |
| **Native dual oracle** | x86-64 host **or** `Dockerfile.lab-x86_64` / CI `lab-x86_64` (**required** for release)                 |
| **API**                | Stable C API `include/aether/aether.h` — no breaking changes without major VERSION bump                 |

---

## Out of scope (non-goals) — do not sell as

- Full arbitrary x86 semantic rewriting (mem, SSE, syscalls, exceptions)
- PE / Mach-O production packers
- Multi-arch (ARM / RISC-V)
- Mass infection, C2, process injection
- **“Undetectable”, “FUD-proof”, “APT implant”** claims
- Ambient-flag-correct JE native without flag setup (native skips JE bodies)

---

## Aggression levels

| Level | Name         | Allowed                                   | CI                         |
|-------|--------------|-------------------------------------------|----------------------------|
| **0** | identity     | re-layout only                            | —                          |
| **1** | safe         | analysis-gated nops + safe block permute  | **enforced** on real       |
| **2** | lab          | + educational diversify/flatten on edu IR | **enforced** on pure gates |
| **3** | experimental | may raise break-rate                      | **not** release-gated      |

---

## Feature freeze (final product)

**Stop adding:** C2, hollowing, eBPF, auto-spread, new stub “features.”  
**Allowed after freeze:** bugfixes, corpus growth, docs, CI, performance, SAFE morph only.

See `SUPPORT.md` for what maintainers fix vs reject.

---

## Install / prove (one path)

```bash
./scripts/bootstrap.sh
```

Must exit 0: configure, build, unit tests, `prove-shock`, `prove-bench`.

Lab native (release requirement):

```bash
docker build -f Dockerfile.lab-x86_64 -t aether-lab .
docker run --rm aether-lab
```
