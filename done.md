# Done — project status

Living changelog of what exists, what was completed, and the current research state.  
Educational / defensive-security metamorphic engine for a Medium article — **not production malware**.

Last updated: 2026-08-07 (lazy generate-on-JSR + multi-stage)

---

## Current state (summary)

| Area | State | Notes |
|------|--------|--------|
| Metamorphic engine (IR, catalogue, permute, flatten, assembler) | **Done (multi-stage)** | Random transform order, IR diversify, heavy permute, multi-stage feed-forward, nested XOR child packaging |
| Multi-stage feed-forward | **Done** | Stage *N* output is re-disassembled and re-morphed as stage *N+1* |
| Nested generation (code carries generated code) | **Done (generation-time)** | Outer morph packs an independently morph’d XOR-wrapped child; child is not re-disassembled |
| Lazy idle → generate-on-need → JSR → wipe | **Done (research)** | Idle: time + senseless syscalls only; on need: multi-stage morph, JSR, wipe. Native JSR on x86-64; simulated on arm64 |
| Full fileless memfd / production wipe tradecraft | **Stub / partial** | Lazy path wipes the morph buffer; not a full commercial fileless chain |
| Benign syscall noise | **Done** | Shuffled order + jitter every run |
| Continuous in-memory mutation | **Done** | XOR entropy stream; post-demo / payload path, **not** on live pipeline code |
| Controlled single-file ELF infection | **Done** | CLI path only; append + `EI_PAD` mark + size growth |
| Empty architecture stubs | **Done (stubs only)** | Hollowing, eBPF, fileless/LOTL, C2 — print `[STUB]` and return |
| Code style (clang-format) | **Done** | `.clang-format` + `format` / `format-check` / `scripts/format.sh` |
| Full ELF entry-point hijack | **Not done** | No PHDR / `e_entry` rewrite |
| C2 / persistence / anti-forensics | **Stub only** | |

**Overall:** **Prototype core** — multi-stage morph + ephemeral JSR arm/disarm.  
Not undetectable alone; designed to **compose with other mechanisms** (stubs / future layers).  
Suitable for Medium + GitHub if framed as a building block, not a finished implant.

---

## Multi-stage + nested generation (what we actually have)

### The idea (why it is hardcore research)

“**Code that creates code that creates code**” is the pure form of metamorphism / poly-generation:

1. Generation *G0* is not a fixed binary blob forever.
2. *G1* is produced by transforming *G0* (different order, encodings, CFG).
3. *G2* is produced by transforming *G1* again — so the *parent already is generated code*.
4. Nesting: an outer body **carries** an inner body that was itself produced by a full morph lineage.

That is qualitatively harder for **static signatures** than a single packer layer: there is no one stable byte pattern that all children share if the pipeline is strong enough.

### What Aether implements today

Implemented in `src/aether/meta/stages.*` + transforms/catalogue/assemble:

| Mechanism | What it does | When it runs |
|-----------|----------------|--------------|
| **Random transform order** | Each stage applies a random multiset of shrink / expand / diversify / permute / heavy_permute / flatten | At **generation** time (`aether_next` run) |
| **Random machine code** | Weighted catalogue encodings + trailing NOP pad | Every assemble |
| **Different implementations** | IR rewrite (e.g. clear rax → mov 0 / inc chains / junk forms) | Every diversify pass |
| **Multi-stage feed-forward** | `bytes → disasm → transforms → assemble → bytes` repeated 2–4 times; output of stage *k* is input of stage *k+1* | Generation time |
| **Nested child** | Independent multi-stage morph of the seed = *child*; another multi-stage = *outer*; child XOR’d with random key; packaged as `outer + jmp + magic + key + len + child + ret` | Generation time |
| **Child integrity** | Nested child is **never** put back through the disassembler after packaging (that would destroy it) | By design |

So: **yes, multi-generation morph + lazy runtime path.**  
Commercial-grade fileless tradecraft is still out of scope.

### What people often mean by “pure” vs Aether

```
on execute (pure ideal):
  idle / decoy
  when needed: allocate, generate/decrypt stage in RAM
  JSR into it
  wipe pages after return
```

| Property | Aether today | Ideal implant stack |
|----------|--------------|---------------------|
| Multi-stage code creates code | **Yes** | Yes |
| Idle = time + senseless syscalls only | **Yes** (`run_lazy_jsr`) | Often |
| Generate **only when needed** | **Yes** (lazy path) | Yes |
| JSR into fresh body | **Yes** (native x86-64 / simulated arm64) | Yes |
| Wipe after JSR | **Yes** (buffer + mapped page wipe) | Yes |
| Full memfd / process hollowing / EDR evasion | **No** (stubs / not claimed) | Sometimes |
| Impossible to detect | **No** | No (honest) |

### Initialization poly stack (generate on exec, wipe when layer done)

This is **only the initialization** of a polymorphic system — not a durable payload.

Implemented in `src/aether/runtime/lazy_jsr.*`:

```
main → slot → idle_fn     // ONLY "shit": time + senseless syscalls
                          // practically nothing real to validate offline

trigger SETS jsr → bootstrap   (main never rewritten)
bootstrap CLEARS jsr first

then for each layer i = 1..N (N∈[10,20]):
    GENERATE layer_i     // random structure; only exists now
    finish layer init step
    WIPE layer_i         // destroyed when done
    keep tiny handoff only (~48–256 B) for next morph

// no full multi-layer body ever sits around for static analysis
```

| Claim | Prototype reality |
|-------|-------------------|
| Only generated on execution | **Yes** |
| Different random structure per layer | **Yes** |
| Wiped when that layer is done | **Yes** (full body; only handoff remains) |
| Practically impossible to validate *offline* as one fixed blob | **Mostly yes** — nothing durable to reverse as “the” poly image |
| Impossible to observe *while init runs* | **No** — live tracing still sees generation |

**Strong as init architecture:** empty-at-rest + ephemeral JSR + poly→poly + wipe-per-layer.  
**Not** a claim of total undetectability; live sensors still apply.

| CLI | Behavior |
|-----|----------|
| (default) | Idle → arm → clear → init layers (wipe each) |
| `--fire` | Arm immediately |
| `--idle-only` | Never generate |

---

## Is this “practically impossible to get / detect”?

### Short answer: **No.**

Strong against some things. Far from invisible.

### What it is *good* against

| Defense class | Effect of multi-stage + nest |
|---------------|------------------------------|
| Fixed byte signatures on the payload | **Often breaks** — each run differs in size, opcodes, CFG, nested blob |
| Simple YARA on one generation sample | **Weak match** across generations if rules are too static |
| Hash-based IOC of one sample | **Useless** for the next run |
| Teaching “why packers/metamorphics exist” | **Excellent** |

### What still catches it (and catches “pure” ephemeral code too)

| Defense class | Why it still works |
|---------------|-------------------|
| **Behavioral EDR / eBPF** | Process actions, unusual mprotect/RWX, memfd_create, weird ELF appends |
| **Memory forensics** | Even wiped pages may leave traces; live dump during execution sees the stage |
| **Control-flow / ML models** | Flattened dispatchers and morph noise have statistical shape |
| **Reputation / provenance** | Unsigned odd binaries, infection of user-named files in a lab |
| **Human + sandbox** | Detonate once; observe size growth, marks, nested magic `Ae` |

So: multi-generation code is **hard to signature**, not **hard to notice**.

### How good is “generate only at execute, then delete”?

If fully implemented (fileless + mem-only stage + wipe):

| Axis | Rating (research honesty) |
|------|---------------------------|
| vs disk AV signature | **Very strong** |
| vs memory scanners during run | **Medium** — still visible while alive |
| vs EDR behavior | **Medium–weak** depending on quality of ops |
| vs skilled IR / DFIR after the fact | **Medium** — “deleted” ≠ “never observed” |
| Complexity to build correctly | **Very high** (crashes, RWX policy, CET, landlock, etc.) |
| Ethics / release risk | **High** if shipped complete |

That model is **stronger than static multi-stage alone** for disk forensics, but it is **not magic** and is **exactly** what modern EDR is built to watch.

Aether’s nested design is a **teaching scaffold** for that story:  
*outer carries a child that was already a product of a morph pipeline* — the conceptual step before “child only materializes in RAM at runtime.”

---

## Repository layout (clean modules)

```
src/aether/common/   rng, portable ELF types
src/aether/noise/    benign syscall noise
src/aether/mutate/   payload XOR entropy
src/aether/meta/     IR, catalogue, decode, transforms, assemble, stages, pipeline
src/aether/runtime/  lazy idle → generate → JSR → wipe
src/aether/infect/   controlled single-file infection
src/apps/            thin CLI mains
src/stubs/           empty advanced-technique headers
```

### Metamorphic building blocks

1. **Decode** — educational x86-64 → IR (unknown → RAW).  
2. **CFG** — blocks, fallthrough, branch targets.  
3. **Transforms (random order per stage)** — shrink, expand, diversify, permute, heavy_permute, flatten.  
4. **Assemble** — catalogue encodings + rel32 fixups.  
5. **Stages** — multi-stage feed-forward + nested XOR package.  
6. **Pipeline** — noise + `engine_pipeline` + demo stats / stage log.

### Infection (controlled)

- Only the path on the CLI; append payload; bump `EI_PAD`; no scan/spread/network.

### Stubs (empty)

Process hollowing, eBPF hide, fileless/LOTL, C2/persistence — architecture only.

---

## Honest limits (do not oversell)

- Educational disassembler (not Capstone/Zydis).
- Flatten uses `rcx` as state; demo-grade if bodies clobber `rcx`.
- Nested child is generation-time packaging, **not** a runtime decrypt-execute-wipe loop.
- Infection is **not** a full virus (no `e_entry` / PHDR hijack).
- Continuous mutation after demo destroys code validity on purpose (entropy demo).
- **Nothing here is undetectable.** Prefer that sentence in the Medium article.

---

## Build & run

```bash
cmake -S . -B build
cmake --build build
./build/aether_next                 # idle → generate → JSR → wipe
./build/aether_next --fire          # minimal idle, then generate+JSR+wipe
./build/aether_next --idle-only     # never generate (time + syscalls only)
./build/aether_next --fire victim_clean  # + controlled infect (copy before wipe)
./build/aether_stubs

./scripts/format.sh
./scripts/format.sh --check
```
---

## Strength assessment (for the article)

| Claim you might want to write | Accurate? |
|------------------------------|-----------|
| “Every run is unique” | **Yes** (with high probability) |
| “Code creates code across multiple stages” | **Yes** (at generation time) |
| “Nested code carries a previously generated generation” | **Yes** |
| “Practically impossible to detect / recover” | **No — false** |
| “Idle until needed; then generate + JSR + wipe” | **Yes (lazy path)** |
| “Native JSR on every laptop” | **No — x86-64 native; arm64 simulated** |
| “Hardcore pure undetectable ephemeral” | **No — still detectable** |
| “Great for Medium + red-team learning” | **Yes** |
| “On par with top-tier commercial malware” | **No** |

**Verdict:**  
Multi-stage + nested metamorphism is **legitimately advanced as a research generator** and a sharp article hook.  
Calling it **practically impossible to get** would be wrong and would hurt credibility.  
The pure “execute-only, then delete” model is **harder still**, **stubbed here**, and **still detectable** by serious defenders.

---

## Suggested next steps (optional)

1. Educational **runtime** decrypt stub in IR (loop over nested child with key) — still no spread.  
2. Optional memfd demo behind a clear stub/flag — document wipe as research only.  
3. Linux CI: build + format-check + two-run uniqueness test.  
4. Keep infection single-file; never auto-spread or ship C2.

---

## Prototype framing (project intent)

This repository is a **prototype of the hard middle**:

| Layer | Role | Status here |
|-------|------|-------------|
| **This core** | Unique generations + deferred activation + thin call edge | **Implemented (research)** |
| Other mechanisms | Fileless load, hollowing, eBPF hide, LOTL, C2, anti-forensics… | **Stubs / not claimed complete** |
| Combined stack | Raise *cost* of detection; still never “impossible” | **Vision only** |

**Author framing (accurate):**

- Alone: **not** undetectable — and we do not claim that.
- As a prototype: the piece you iterate on first (morph + when/how control flows).
- With other mechanisms (later / separate research): becomes a **stack**, where each layer covers a different sensor (disk signatures, call graph, memory lifetime, network, …).

That is the right way to present a Medium series:

1. **Part 1 (this repo):** metamorphic multi-stage + ephemeral JSR prototype.  
2. **Later parts:** compose stubs (memfd, hollowing, …) one at a time, still controlled, still honest.

**Do say:** “prototype core for a layered research architecture.”  
**Don’t say:** “undetectable once you bolt the stubs on.” Detection cost goes up; guarantees do not appear.

---

## Intent

Research and educational use only. Infection remains limited to a single user-named file.  
Advanced offensive techniques stay stubs or incomplete research unless fully documented as such.  
Prefer honest residual detection surfaces over claims of undetectability.  
This is a **prototype** meant to grow by composition, not a finished product.
