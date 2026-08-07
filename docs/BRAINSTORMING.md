# Aether — Brainstorming: base quality & path forward

**Question:** Is the base good?  
**Answer: Yes.** The base is a strong open-research polymorphic **initialization** core. It is not a finished
closed-industry engine, and it does not need to be one to be valuable. This file maps **what exists in code today** to *
*what would be needed** to push each pillar further (Medium spectacle → specialist-credible research).

Related: `docs/IMPLEMENTATION_PLAN.md` (closed-industry bar at top), `docs/SCORECARD.md` (metrics + residual surfaces),
`done.md` (status).

---

## 1. Base verdict (keep this honest)

| Statement                                                     | Verdict                                                      |
|---------------------------------------------------------------|--------------------------------------------------------------|
| Base architecture is good                                     | **Yes**                                                      |
| Strong for Medium / open research / demos                     | **Yes** (`aether_shock`, cascade, multi-pass permute, tests) |
| “Always different” is a real design goal and largely achieved | **Yes** (harness + cascade keys + multi-pass streams)        |
| Already closed-industry / state-entity complete               | **No**                                                       |
| Good foundation to build the next level on                    | **Yes — this is the point of a prototype**                   |

**One line:**  
The base is **good enough that the hard problems are now the right ones** (real IR, equivalence, scale) — not “do we
even have an idea?”

---

## 2. What the current system *is* (mental model)

```
IDLE (shit only)
  time + senseless syscalls — no morph body, nothing to validate offline
       │
TRIGGER arms JSR → bootstrap          (main never rewritten)
       │
BOOTSTRAP clears JSR first            (edge ~nanoseconds)
       │
INIT CORE
  • research leaf (scaffold + marker)
  • crypto cascade build (onion encrypt, multi-layer)
  • crypto cascade peel (decrypt → wipe each layer)
  • structural diversity via morph / world_class_permute / layer modes
       │
WIPE residue → back to idle
```

**Roles:**

| Role                      | Job                                                                        |
|---------------------------|----------------------------------------------------------------------------|
| **Permutation**           | Make control-flow *layout* alien across runs (multi-pass, not one shuffle) |
| **Catalogue / diversify** | Make *encodings / micro-implementation* alien                              |
| **Cascade**               | Real layered *encrypt/decrypt*; peel wipes layer when done                 |
| **Runtime JSR**           | When init is allowed to run at all; thin call edge                         |
| **Tests / scorecard**     | Prove uniqueness & round-trips; document residual surfaces                 |

---

## 3. Current code map (what we have)

### 3.1 Layout

```
src/aether/
  common/     rng, hash64, portable ELF bits
  noise/      benign syscall noise (idle “shit”)
  mutate/     continuous XOR entropy (post-demo / payload toys)
  meta/       IR, decode, catalogue, transforms, assemble, stages, cascade, pipeline
  runtime/    ephemeral JSR + cascade init orchestration
  infect/     controlled single-file ELF append (research only)
src/apps/
  aether_next_main.cpp    CLI: idle / fire / idle-only
  aether_shock_main.cpp   spectacle demo for Medium
  aether_stubs_main.cpp   stub architecture demo
src/stubs/    empty advanced-technique placeholders
tests/        unit + uniqueness harness
docs/         IMPLEMENTATION_PLAN, SCORECARD, BRAINSTORMING (this file)
```

### 3.2 Module → responsibility → status

| Module         | Path                         | What it does                                                        | Strength today                     |
|----------------|------------------------------|---------------------------------------------------------------------|------------------------------------|
| RNG            | `common/rng.*`               | seed_rng, seed_rng_u64, rnd, hash64                                 | Good for demos/tests               |
| Noise          | `noise/*`                    | Idle syscalls + time                                                | Good for “empty at rest” story     |
| IR             | `meta/ir.hpp`                | Op / block / func model                                             | Educational, limited               |
| Decode         | `meta/decode.*`              | Bytes → IR + CFG leaders                                            | Subset x86-64 only                 |
| Catalogue      | `meta/catalogue.*`           | Weighted encodings                                                  | Solid research catalogue           |
| Transforms     | `meta/transforms.*`          | shrink/expand/diversify/permute/flatten/modes/`world_class_permute` | Strong research; not industrial    |
| Assemble       | `meta/assemble.*`            | IR → bytes + rel32 fixups                                           | Good for supported ops             |
| Stages         | `meta/stages.*`              | morph_stage / morph_stage_mode / multi-stage                        | Good generator path                |
| Crypto cascade | `meta/crypto_cascade.*`      | Onion build/peel, keystream crypt, wipe packages                    | **Real cascade** (research cipher) |
| Pipeline       | `meta/pipeline.*`            | Orchestration + stats                                               | Demo glue                          |
| Lazy JSR       | `runtime/lazy_jsr.*`         | Idle → arm → clear → cascade init                                   | Core control-plane story           |
| Infect         | `infect/*`                   | Single named ELF append                                             | Controlled, not full virus         |
| Stubs          | `stubs/*`                    | Hollowing, eBPF, LOTL, C2                                           | Architecture only                  |
| Tests          | `tests/*`                    | Unit + 200-run uniqueness                                           | Evidence, not vibes                |
| Shock demo     | `apps/aether_shock_main.cpp` | Live spectacle                                                      | Medium-facing                      |

### 3.3 What is already “done” as a base

- [x] Ephemeral activation model (trigger / JSR clear)
- [x] Idle path with no real body
- [x] Multi-pass permutation suite (not a single shuffle)
- [x] Named structural modes (incl. **permute-heavy** → `world_class_permute`)
- [x] Real multi-layer **encrypt/decrypt cascade** with wipe-on-peel
- [x] Probabilistic depth (min/max/grow) for cascade wraps
- [x] Research impl leaf (marker, not weaponized payload)
- [x] Uniqueness + cascade + transform tests
- [x] Shock demo for public impact
- [x] Honest scorecard (live unobservability **not** claimed)

---

## 4. Brainstorm: what would make closed specialists care

They are not shocked by demos alone. They react to:

| Shock type            | Need                                                  |
|-----------------------|-------------------------------------------------------|
| Correctness           | Transforms don’t break real functions                 |
| Scale                 | Whole binaries / large corpus                         |
| Measurement           | Public metrics: uniqueness, crash rate, size delta    |
| Novelty or standard   | New technique **or** the open benchmark everyone uses |
| Reproducible artifact | One command, Docker, paper-quality figures            |

**Implication for this repo:** keep the base; add **real IR + equivalence + corpus + numbers**.

---

## 5. Gap analysis → implementation needs

### 5.1 Real disassembly / IR (highest leverage)

| Now                       | Needed to implement                           |
|---------------------------|-----------------------------------------------|
| Hand-rolled subset decode | Integrate **Zydis** or **Capstone**           |
| Limited `Op` enum         | Richer IR or keep CFG at “decoded insn” level |
| Lab seed blobs            | Lift real functions from open ELF corpus      |

**Work items:**

1. Add Zydis (or Capstone) dependency via CMake/`FetchContent`
2. `meta/decode_zydis.cpp`: function bytes → instruction list + CFG edges
3. Decide: morph at **instruction list** level vs expand educational IR
4. Round-trip: disasm → (optional transform) → re-encode (asm engine or encoder API)

**Depends on:** build system, license of corpus binaries.

---

### 5.2 Equivalence harness (correctness bar)

| Now                     | Needed                      |
|-------------------------|-----------------------------|
| Branch-range checks     | Run original vs transformed |
| Cascade leaf round-trip | Same for *code* transforms  |

**Work items:**

1. Harness: map code RWX (lab x86-64 only) or interpreter for subset
2. Fixed input vectors → compare outputs / exit codes
3. CI job: fail on any semantic break
4. Fuzz: random seeds → morph → execute → crash?

**Bar:** “aggressive permute, 0 breaks on corpus C.”

---

### 5.3 Permutation → specialist-grade (build on `world_class_permute`)

| Now                                | Needed                                         |
|------------------------------------|------------------------------------------------|
| Multi-pass layout + filler shuffle | Dependency-aware reordering                    |
| Educational IR                     | Real CFG + data-flow                           |
| Uniqueness tests on small seeds    | Uniqueness **and** equivalence on large corpus |

**Work items:**

1. Def-use chains on IR / decoded insns
2. Only shuffle independent nodes
3. Dominator-aware block groups
4. Preserve stack/ABI for real functions
5. Metrics: diversity score vs break rate tradeoff curve

**Keep:** multi-pass streams (`seed_rng_u64` per pass) — already the right idea (“never one shuffle”).

---

### 5.4 Crypto cascade → harder research artifact

| Now                           | Needed                                                                         |
|-------------------------------|--------------------------------------------------------------------------------|
| Custom keystream onion + wipe | Optional audited lib (e.g. libsodium) **or** stick with research cipher + fuzz |
| Package `AeC1`                | Formal format doc + versioning                                                 |
| Peel wipes package            | Tests that buffers are zeroed (canary / ASAN poisoning optional)               |

**Work items:**

1. `docs/CASCADE_FORMAT.md` (bytes layout)
2. Fuzz invalid packages
3. Measure peak RSS / layer lifetime in demo mode
4. Optional: cascade over **real function bytes** once Zydis path exists

---

### 5.5 Runtime model (JSR) — keep story, deepen fidelity

| Now                          | Needed (optional research)                                   |
|------------------------------|--------------------------------------------------------------|
| Atomic function-pointer slot | Document as intentional abstraction                          |
| Clear-on-entry               | Optional: native x86 call-site patch lab-only (Linux x86-64) |

**Work items (optional):**

1. Lab-only trampoline with real `CALL` then restore NOPs after return
2. Never claim this equals live unobservability

---

### 5.6 “Shock closed people” track (pick one flagship)

| Option                     | Implement                                            |
|----------------------------|------------------------------------------------------|
| **A. Open benchmark**      | Corpus + leaderboard JSON: uniqueness / crash / size |
| **B. Detector research**   | Heuristics for onion peel / init burst (defensive)   |
| **C. Differential fuzzer** | Find breakages in transforms; publish bugs fixed     |

**Recommended for this project:** **A** first (fits Medium + credibility), then **C**.

---

### 5.7 Explicitly out of scope (do not put on the critical path)

- “Impossible to observe live during init” as a finished property
- Full eBPF hide + C2 + mass infection as default shipping goal
- Claiming state-entity operational capability from a public repo

Those destroy either **honesty** or **safety** (or both).

---

## 6. Suggested roadmap (phased)

### Phase 0 — Base (DONE)

Prototype core: idle, JSR, cascade, multi-pass permute, tests, shock demo.

### Phase 1 — Specialist credibility foundation

1. Zydis decode on real functions
2. Equivalence harness (x86-64 lab)
3. Small open ELF corpus (e.g. coreutils subset / own test bins)
4. CI: uniqueness + cascade + equivalence

### Phase 2 — Stronger transforms

1. Dependency-safe reordering
2. Permute diversity vs break-rate metrics
3. Cascade on real function blobs

### Phase 3 — Public shock artifact

1. Benchmark suite + published numbers
2. Paper/Medium figures from `aether_shock` + corpus graphs
3. Optional defensive detector chapter

### Phase 4 — Composition (stubs → research modules)

Only after Phase 1–2: memfd demo, controlled PHDR experiment, etc. — still single-target, documented, non-spread.

---

## 7. Brainstorm backlog (ideas, not commitments)

| Idea                                               | Value                       | Risk / cost          |
|----------------------------------------------------|-----------------------------|----------------------|
| Interpreter for educational IR (equiv without RWX) | Portable tests              | Limited realism      |
| Docker lab image x86-64                            | Repro for readers           | CI complexity        |
| Property-based tests (RapidCheck)                  | Stronger fuzz               | Dependency           |
| Parallel multi-seed uniqueness job                 | Stronger “always different” | Flaky if not careful |
| Formal package schema for cascade                  | Clarity                     | Doc debt             |
| Visual HTML report of 100 generations              | Medium gold                 | Frontend time        |
| Safe region markers in source (`// aether:morph`)  | Controlled demos            | Not full-binary      |

---

## 8. Decision log (working assumptions)

1. **Base stays** — do not rewrite the control-plane story; extend it.
2. **Research 10/10** = verified uniqueness + cascade + permute diversity + honesty — see SCORECARD.
3. **Always different** = practical + tested, not metaphysical.
4. **Multiple shuffles** = intentional product requirement; single shuffle is a bug.
5. **Cascade** = real encrypt/decrypt; morph is for structure, crypto for secrecy between layers.
6. **Live invisibility** = non-goal; short lifetime + wipe = goal.
7. **Public repo** = educational / defensive research framing only.

---

## 9. Immediate next implementation candidates

If starting coding tomorrow, prefer this order:

| # | Task                                            | Why                                  |
|---|-------------------------------------------------|--------------------------------------|
| 1 | Zydis (or Capstone) behind `meta/decode_real.*` | Unlocks everything specialist-facing |
| 2 | Equivalence harness for morph_stage outputs     | Turns “strong” into “trusted”        |
| 3 | Tiny open corpus + CI job                       | Scale signal                         |
| 4 | Benchmark JSON exporter                         | Medium + closed-reader bait          |
| 5 | Optional native CALL trampoline (lab x86-64)    | Fidelity to JSR story                |

---

## 10. Bottom line

| Question                   | Answer                                                                 |
|----------------------------|------------------------------------------------------------------------|
| Is the base good?          | **Yes.**                                                               |
| Is it finished?            | **No — prototype by design.**                                          |
| What’s the next real leap? | **Real binaries + equivalence + public metrics.**                      |
| What do we keep?           | Idle / JSR / cascade / multi-pass permute / wipe / tests / shock demo. |

**Closing:**  
You already have a **mind-boggling public demo** and a **coherent init architecture**.  
To move from “stunning Medium” toward “closed people cannot dismiss this,” implement **correctness on real code at scale
** — this brainstorming file is the map.

---

## 11. Next steps — make the base so strong that pros (and serious buyers) drop their jaw

**How to make the base even better in one sentence:**  
Stop adding theater; add **proof on real binaries at scale**, then **own a public standard** others must cite.

Jaw-drop for **professors / industry pros / institutional buyers** does not come from “undetectable.”  
It comes from: *“We cannot ignore these numbers / this artifact / this benchmark.”*

### North-star outcome

| Audience                                            | What makes them lose their jaw                                                    |
|-----------------------------------------------------|-----------------------------------------------------------------------------------|
| Professors                                          | Formal or near-formal **correctness + open corpus + reproducible paper artifact** |
| Industry RE / AV                                    | **0-break morph on real code** + diversity metrics that beat toys                 |
| Serious buyers (defense / red-team / research labs) | **Licensable lab tool**: Docker, API, reports, support story — not a blog binary  |

**Non-goals (kill credibility):** claiming live invisibility, shipping weaponized C2, “state malware clone.”

---

### The 12 next steps (do in order) + §13 jaw-drop sprint

#### Step 1 — Real binary IR (kill “toy” forever) ✅ decode + 1B morph path

- **Shipped:** Zydis lift (`disasm_real` / `RealFunc`).
- **Shipped (path 1B):** `RealFunc` **is** the IR — `real_expand_nops`, `real_permute_blocks`,
  `assemble_real` (rel32 edge re-encode, fallthrough seal), `morph_real_restricted`,
  `interpret_real_pure`; gated in equivalence campaign domain `RealRestricted`.
- **Done when:** morph path runs on real bytes, not only educational `Op` seeds. → **met for restricted set.**
- Still open: full `.text` corpus scale, arbitrary insn substitution, lab native EAX CI.

#### Step 2 — Equivalence oracle (the professional bar) ✅ COMPLETE gate

- **Shipped:** EduPureRax + multi_stage (`normalize_reachable`) + Real 1B + CascadeLeaf + RealText.
- CI: `prove-equiv` / `prove-shock` — **0 breaks**; JSON; lab native: `Dockerfile.lab-x86_64`.
- Honest bound: pure RAX + restricted real morph — not full x86 SMT/ABI.
- **Done when:** aggressive paths report **0 breaks** on defined domains. → **met.**

#### Step 3 — Dependency-safe permutation only

- Build def-use / side-effect classes; only reorder when safe.
- Keep multi-pass streams (never one shuffle) — but **prove** safety.
- **Done when:** “world_class_permute” is justified by **break-rate = 0** under max aggression settings.

#### Step 4 — Corpus at embarrassing scale

- ≥ **10k functions** (or full small userspace packages).
- Nightly: uniqueness, size delta, crash rate, timeout rate.
- **Done when:** one public JSON/CSV of results regenerable with one command.

#### Step 5 — Cascade on real payloads

- Onion wrap/peel **real function blobs** (not only research markers).
- Document format; fuzz invalid packages; measure peel lifetime / peak RSS.
- **Done when:** 1000 random build/peel round-trips + wipe tests green.

#### Step 6 — Own the open benchmark (this is the jaw-drop)

- Publish **Aether Morph Bench**: others can submit engines.
- Leaderboard dimensions: uniqueness, equivalence, size, speed, crash rate.
- **Done when:** third parties run your harness without you in the room.

#### Step 7 — Differential fuzzer against your own pipeline

- Random seeds → transform → execute → minimize failures.
- Public “bugs found & fixed” log (shows maturity).
- **Done when:** fuzzer runs unattended and shrinks counterexamples.

#### Step 8 — Lab-only native fidelity (x86-64 VM)

- Optional real `CALL` trampoline + execute peel stages in a **documented lab image**.
- Never required for Mac arm64 host; CI uses qemu or labeled `lab-x86_64`.
- **Done when:** README shows one Docker/qemu path that executes transformed code safely.

#### Step 9 — Buyer-grade packaging

- Versioned releases, SBOM, `CHANGELOG`, stable CLI flags, machine-readable reports (`--json`).
- Clear license + research-use terms.
- **Done when:** a lab can install without reading the whole Medium post.

#### Step 10 — Defense chapter (makes professors and blue teams care)

- Publish detectors / heuristics for: idle-noise+burst init, onion peel loops, ephemeral arm patterns — **with
  false-positive rates**.
- Frame as dual-use research: offense model + defense.
- **Done when:** a detection paper section has numbers, not slogans.

#### Step 11 — External validation

- Independent review (academic, bug bounty style, or conference artifact evaluation).
- Cite SCORECARD + bench results.
- **Done when:** someone outside the repo reproduces uniqueness + equivalence claims.

#### Step 12 — Flagship result (pick one and finish it)

Choose **one** headline result and make it undeniable:

| Pick  | Jaw-drop line                                                                   |
|-------|---------------------------------------------------------------------------------|
| **A** | “First open metamorphic stress corpus with equivalence-enforced leaderboard”    |
| **B** | “0 semantic breaks on N functions under multi-strategy permute + cascade init”  |
| **C** | “Open detector for layered-init patterns with ≤X% FP on benign Debian packages” |

Without a finished **A/B/C**, pros nod. With one finished, they **bookmark and brief others**.

---

### §13 — Exact steps to make people go crazy (pros stunned)

**Intent:** not more demos, not more stubs, not “undetectable.”  
One **closed proof loop** so a professional’s first reaction is: *“this is real — and they measured it.”*

Spectacle (`aether_shock`) already exists. This section is the **authority sprint** that turns nods into bookmarks.

---

#### 13.0 — The one sentence they must be able to repeat

> *“Aggressive multi-strategy morphs change almost every byte hash, preserve observable semantics on a public corpus,
and CI fails on a single break — reproducible with one command.”*

If you cannot print that from CI, you are not done. If you can, pros brief others.

---

#### 13.1 — What is already closed (do not redo)

| Piece                                     | Status                    | Pro takeaway                     |
|-------------------------------------------|---------------------------|----------------------------------|
| Educational IR morph + multi-pass permute | Shipped                   | Structure moves                  |
| Uniqueness harness (200-run)              | Shipped                   | Always-different *in practice*   |
| Crypto cascade + wipe story               | Shipped                   | Layered init research model      |
| Equivalence oracle on pure RAX corpus     | Shipped (Step 2 lite→pro) | **0 breaks**, many unique hashes |
| `prove-unique` / `prove-equiv`            | Shipped                   | One-command falsifiers           |

**Honest bound still open:** educational IR ≠ full x86; pure ret-RAX ≠ ABI; arm64 host ≠ native EAX.

---

#### 13.2 — The jaw-drop ladder (exact order — do not skip)

Execute **13.2.A → 13.2.E** in order. Each step has **code size**, **command**, and **done when**.

##### 13.2.A — Seal the byte pipeline (flatten survives re-lift)

**Why pros care:** you already admitted multi-stage re-disasm of flatten is fragile. Fixing it is integrity, not
cosmetics.

|               |                                                                                                                                                                 |
|---------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Work**      | Fix assemble order / leaders / `CMP rcx` round-trip so `multi_stage_morph` after flatten still returns golden RAX; re-enable path in `run_equivalence_campaign` |
| **Code**      | ~150–400 LOC (`assemble`, `decode`, `build_cfg`, `equiv`)                                                                                                       |
| **Command**   | `ctest -R equivalence` — multi_stage path back, **breaks == 0**                                                                                                 |
| **Done when** | Campaign prints multi_stage checks with 0 breaks for ≥200 trials                                                                                                |
| **Stun line** | *“Nested morph stages do not silently kill the state machine.”*                                                                                                 |

##### 13.2.B — Dual oracle: interpret + native EAX (lab x86-64)

**Why pros care:** software interpreters lie; one real `CALL` ends the argument.

|               |                                                                                                   |
|---------------|---------------------------------------------------------------------------------------------------|
| **Work**      | Docker/qemu `linux/amd64` CI job; run `try_exec_x64_eax` on seed + morph; fail on native mismatch |
| **Code**      | ~20–80 LOC + ~30–80 lines CI/Dockerfile                                                           |
| **Command**   | `cmake --build build --target prove-equiv` on label `lab-x86_64` (or qemu)                        |
| **Done when** | Report shows `native_checked > 0` and `native_breaks == 0` in CI logs                             |
| **Stun line** | *“Not only IR — the CPU returns the same EAX.”*                                                   |

##### 13.2.C — Real `.text` under the same gate (kill “toy” forever)

**Why pros care:** Step 1 Zydis decode without morph+equiv is a museum exhibit.

|               |                                                                                                                                                                                               |
|---------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Work**      | Lift N real functions from open ELFs (`victim_clean` + small licensed bins); **restricted** morph set first (expand/nop/junk/encode, then safe permute); equivalence via oracle and/or native |
| **Code**      | ~400–900 LOC (Real→safe morph path, corpus loader, campaign extension)                                                                                                                        |
| **Command**   | `aether_core_tests equiv_real` / `prove-equiv-real`                                                                                                                                           |
| **Done when** | ≥50 real functions, aggressive-but-restricted morph, **0 breaks**, diversity metrics printed                                                                                                  |
| **Stun line** | *“We morph real ELF .text and prove it — not demo seeds only.”*                                                                                                                               |

##### 13.2.D — One gate for the whole story (cascade + morph + JSR scaffold)

**Why pros care:** orchestration without the equivalence gate is theater.

|               |                                                                                                                         |
|---------------|-------------------------------------------------------------------------------------------------------------------------|
| **Work**      | Campaign paths: morph_stage → interpret; cascade build/peel → leaf interpret; optional arm/clear JSR still pure on leaf |
| **Code**      | ~100–250 LOC glue in `equiv` + tests                                                                                    |
| **Command**   | single `prove-all` = uniqueness + equivalence (+ real if present)                                                       |
| **Done when** | One CI job fails if **any** of uniqueness / equiv / cascade peel breaks                                                 |
| **Stun line** | *“The Medium story and the CI gate are the same object.”*                                                               |

##### 13.2.E — Public number + one-command artifact (bookmark fuel)

**Why pros care:** they only brief what they can re-run without you.

|               |                                                                                                                                       |
|---------------|---------------------------------------------------------------------------------------------------------------------------------------|
| **Work**      | Machine-readable report (`--json`): seeds, trials, breaks, unique hashes, size delta, timing; regenerate corpus slice with one target |
| **Code**      | ~200–500 LOC harness/IO + SCORECARD/README block                                                                                      |
| **Command**   | `cmake --build build --target prove-shock` → writes `artifacts/equiv_report.json`                                                     |
| **Done when** | Stranger clones repo, runs one command, gets the same verdict + JSON                                                                  |
| **Stun line** | *“Artifact evaluation in one command — no author on the call.”*                                                                       |

---

#### 13.3 — After the ladder (only if 13.2 is green)

| Step                          | Stun multiplier      | Code (ballpark)  | Skip until…             |
|-------------------------------|----------------------|------------------|-------------------------|
| Safe def-use permute (Step 3) | High for RE          | ~300–800 LOC     | 13.2.C green            |
| 1k→10k corpus (Step 4)        | High for AV research | harness + data   | 13.2.E green            |
| Public Morph Bench (Step 6)   | Maximum open shock   | ~500–1.5k + site | 13.2.E + corpus         |
| Thin multi-arg oracle         | Medium               | ~200–500 LOC     | optional                |
| Full ABI / mem / syscalls     | Do **not** promise   | 5k–20k+          | never as “finally done” |

---

#### 13.4 — Total code to “pros stunned” (research closed)

| Scope                                       | Approx new/changed C++ |
|---------------------------------------------|------------------------|
| **13.2.A–E only** (recommended finish line) | **~1.0–2.0k LOC**      |
| + Step 3 safe permute + 1k corpus MVP       | **~1.5–3k LOC**        |
| Full industrial x86 + side effects          | **Not a finish line**  |

This is **not** a rewrite. Current core stays; you **seal holes and publish numbers**.

---

#### 13.5 — The demo script (what you show a pro in 5 minutes)

1. `cmake --build build --target prove-unique` → always-different hashes.
2. `cmake --build build --target prove-equiv` → banner: **VERDICT PASS (0 breaks)**, unique hashes ≫ 1.
3. (When ready) `prove-equiv-real` → real `.text` line in the banner.
4. (Lab) native_checked > 0.
5. Open `artifacts/equiv_report.json` — same numbers, no story time.
6. **Say out loud the bound:** pure/restricted domain; not live stealth; not full x86 SMT.

Pros stun on **(2)+(5)+(6)** together. Spectacle alone is forgotten by dinner.

---

#### 13.6 — Shock metrics (print these or lose)

| Metric                       | Pass bar (research)                                    |
|------------------------------|--------------------------------------------------------|
| Semantic breaks              | **0**                                                  |
| Unique morph hashes / trials | **high** (e.g. ≥50% unique under load)                 |
| Paths exercised              | permute, flatten, layer modes, morph_stage, multi-pass |
| Native breaks (lab)          | **0** when native_checked > 0                          |
| Real-text funcs under gate   | **≥50** then raise                                     |
| One-command reproduce        | stranger succeeds                                      |

---

#### 13.7 — Anti-patterns that kill the stun

- More ASCII / more stubs before 13.2.A–C
- Claiming “undetectable” or “closed industry implant”
- Equivalence only on hand-picked seeds with no CI
- Hiding the educational-IR / pure-RAX bound
- Shipping leaderboard before 0-break gate is real

---

#### 13.8 — Definition of “people go crazy / pros stunned”

| Audience          | They do this when stunned                                                    |
|-------------------|------------------------------------------------------------------------------|
| Internet / Medium | Share the **0-break + unique hashes** screenshot, not the JSR metaphor alone |
| RE / AV pros      | Re-run your target; file issues on edge cases (usage, not mockery)           |
| Professors        | Cite SCORECARD + JSON artifact; ask about corpus license                     |
| Labs / buyers     | Ask for workshop, Docker label, license — not “is this a toy?”               |

**You are stunned-done when:** a stranger reproduces **0 breaks + diversity** without you, and argues metrics — not
vibes.

---

#### 13.9 — Monday checklist (copy-paste)

```
[ ] 13.2.A  multi_stage + flatten re-lift → 0 breaks in campaign
[ ] 13.2.B  lab-x86_64 / qemu native EAX in CI
[ ] 13.2.C  ≥50 real .text funcs under restricted morph + equiv
[ ] 13.2.D  prove-all = unique + equiv (+ cascade leaf)
[ ] 13.2.E  artifacts/*.json one-command regenerate
[ ] README: exact commands + honest bounds (no stealth claims)
[ ] SCORECARD: metrics A–E all green with links to logs
[ ] Only then: Step 3 safe permute → corpus scale → public bench
```

---

### Sequence (what to do next Monday)

```
§13.2.A  Seal multi-stage flatten re-lift
§13.2.B  Native EAX in lab CI
§13.2.C  Real .text under equivalence gate
§13.2.D  One prove-all gate (unique + equiv + cascade)
§13.2.E  JSON artifact + stranger reproduce
→ then Steps 3–6 (safe permute, scale, public bench)
→ then 7–12 (fuzzer, packaging, defense, external validation, flagship A/B/C)
```

Do **not** start with more ASCII demos or more stubs.  
`aether_shock` already covers spectacle; **§13.2** covers **authority**.

---

### How you’ll know pros / serious buyers “lost their jaw”

| Signal                                         | Meaning                                     |
|------------------------------------------------|---------------------------------------------|
| They reproduce your bench without asking you   | Artifact is real                            |
| They argue with your metrics (not your vibes)  | You’re in their league                      |
| They request lab license / workshop / citation | Buyer/academic traction                     |
| They open issues about edge-case ABI bugs      | They’re using it, not mocking the seed demo |

---

### Bottom line

| Make base better? | **Yes — by proof on real code, not more mystique.** |
| Jaw-drop closed people? | **§13.2 ladder: seal pipeline → native → real .text → one gate → JSON.** |
| Code to research-stunned | **~1–2k LOC (13.2.A–E), not a rewrite.** |
| First concrete step now | **13.2.A (multi-stage flatten re-lift), then 13.2.B–E.** |
