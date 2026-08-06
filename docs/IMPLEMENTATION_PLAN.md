# Aether Project — Implementation Plan (Self-Fill)

## Closed-industry / “state entity” technical bar (read first)

**Short answer:** not “add three functions and you’re state-level.”  
Closed industry / high-assurance transform engines are a **full system**: people, verification, binary scale, and long-term maintenance — not only a strong `world_class_permute` or crypto cascade.

This repo is a **serious open research prototype** (ephemeral JSR, multi-layer cascade, multi-pass permutation, uniqueness tests). Reaching that higher **technical** class means closing big gaps **around** the core, not renaming it.

### What that class usually requires

| Layer | What they have |
|--------|----------------|
| **People** | Full-time reverse engineers, compiler/IR experts, red + blue review |
| **Binary truth** | Full (or near-full) ISA coverage, real ABI/calling conventions |
| **Correctness** | Equivalence checking: transformed code **must** behave the same |
| **Scale** | Whole programs / modules, not educational IR seeds only |
| **Pipeline** | Compile → transform → link → deploy → re-mutate over time |
| **Validation** | Huge automated test matrices, fuzzing, crash triage |
| **Longevity** | Survive months of defender updates, not one blog cycle |

Permutation / cascade are **gears**. That tier runs a **factory**.

### Gap map: this prototype → that technical class

#### 1. Real disassembly / IR (biggest technical jump)

| Now (research) | Needed for that class |
|----------------|------------------------|
| Educational subset IR | Capstone/Zydis-class (or custom) full decode |
| Limited ops | Accurate CFG, calls, exceptions, PIC/RIP-relative |
| Lab seeds | Real stripped binaries / production objects (licensed corpus) |

Without this, “world-class permute” cannot see the code that matters.

#### 2. Semantics-preserving transforms

| Now | Needed |
|-----|--------|
| CFG shuffle + filler islands + JMP repair | Data-flow / dependency analysis |
| “Safe enough” on toy IR | Only reorder when proven independent |
| Diversify CLEAR/MOV etc. | Full equivalent catalogs + register pressure |

**Bar:** after transform, tests / symbolic checks still pass.

#### 3. Industrial-grade permutation (beyond multi-shuffle)

Not more random shuffles alone:

- Dominance / post-dominator aware block motion  
- Trace / superblock formation then reshuffle  
- Stack-frame / calling-convention safety  
- Exception edges, tail calls, switch tables  
- Multi-arch where required  
- Deterministic **repro** for debug *and* high-entropy release builds  
- Proven equivalence (harness / differential / symbolic)

#### 4. Crypto cascade at that tier

| Now | Needed |
|-----|--------|
| Research keystream onion + wipe | Audited crypto, key lifecycle discipline |
| Lab peel loop | Loader that fits real loaders/linkers/OS protections |
| Wipe buffers | Secure wipe under real allocators (still never perfect vs live dump) |

#### 5. Verification & uniqueness (started — must scale)

| Now | Needed |
|-----|--------|
| Hundreds of uniqueness / cascade / permute tests | 10⁴–10⁶ generation corpus |
| Branch-range checks | Differential testing on real targets |
| Cascade round-trip | Fuzz build/peel; fault injection |

“Broken after morph” is a P0 product bug at that level.

### What is *not* “just more code”

- **Live unobservability** is not a property you can finish against real monitoring.  
- Operational “state entity” also means process, charter, infrastructure, feedback loops — **beyond a solo public repo**.  
- This plan is for **defensive research / red-team lab / academic engine quality**, not an operational implant product.

### Realistic ladder

| Level | What it looks like | Aether today |
|-------|--------------------|--------------|
| Tutorial poly | One shuffle / XOR | Above |
| Strong open research | Multi-pass permute + cascade + tests + ephemeral JSR | **Here** |
| Serious commercial red-tool / AV research engine | Full IR + equivalence + scale | Multi-year next step |
| Closed industry / institutional class | Full org + continuous adversary loop | Beyond a single codebase |

### Research-only path toward top open quality (this plan’s north star)

Prioritize, in order:

1. **Real disassembler + real CFG** on full functions  
2. **Equivalence tests** (same I/O / same side effects on a harness)  
3. **Dependency-safe reordering** only  
4. **Corpus** of real binaries (open-source, licensed)  
5. **Fuzz** transform → assemble → run  
6. **Metrics dashboard**: uniqueness, crash rate, size delta  
7. Keep **cascade + wipe + ephemeral JSR** as init model; **do not** claim live invisibility  

**One line:** closed-industry *technical* quality needs a **RE/compiler-grade transform engine with proof of correctness at scale**; institutional operational level needs an **organization**, not a single strong permute function.

See also: `docs/SCORECARD.md` (research 10/10 definition, uniqueness harness, residual surfaces).

---

## Current Status (what is already working)

| Component                                                              | Status        | File                                  |
|------------------------------------------------------------------------|---------------|---------------------------------------|
| Metamorphic engine (IR, catalogue, permutation, flattening, assembler) | Fully working | aether_ultimate.cpp / aether_next.cpp |
| Live XOR with fresh entropy                                            | Fully working | aether_next.cpp                       |
| Continuous in-memory mutation                                          | Fully working | aether_next.cpp                       |
| Random benign syscall noise (shuffled order + jitter)                  | Fully working | aether_next.cpp                       |
| Changing ELF mark + size shifting                                      | Fully working | aether_next.cpp                       |
| Controlled single-file infection                                       | Fully working | aether_next.cpp / aether_infect.cpp   |
| Empty architecture stubs                                               | Present       |                                       |
| Signature evasion                                                      | Not done      |                                       |
| Metamorphism / IR / catalogue                                          | Not done      |                                       |
| Random benign noise                                                    | Not done      |                                       |
| Continuous memory mutation                                             | Not done      |                                       |
| io_uring path                                                          | Not done      |                                       |
| Full process hollowing + module stomping                               | Not done      |                                       |
| Production-grade eBPF rootkit that hides itself                        | Not done      |                                       |
| Fully fileless + full LOTL infection chains                            | Not done      |                                       |
| Commercial-grade C2 + persistence + anti-forensics                     | Not done      |                                       |

## Advanced Techniques — Empty Stubs Only

These exist only as empty functions that print `[STUB] … — empty`.  
You will write the real bodies yourself if you choose to.

### 1. io_uring path

- File: `aether_stubs.cpp` → expand later
- Goal: replace classic read/write/connect with io_uring submission/completion
- Difficulty: Medium–High
- Detection impact: High (bypasses many classic syscall hooks)

### 2. Process hollowing + Module stomping

- Stubs: `ProcessHollowing::hollow_process`, `ProcessHollowing::module_stomp`
- Goal: run payload inside a legitimate process address space
- Difficulty: High
- Detection impact: Very High

### 3. eBPF rootkit (self-hiding)

- Stubs: `EBPFRootkit::load_hide_programs`, `EBPFRootkit::hide_from_bpftool`
- Goal: hide processes, files, and the BPF programs themselves
- Difficulty: Very High
- Detection impact: Extreme (but leaves forensic traces)

### 4. Fully fileless + LOTL chains

- Stubs: `FilelessLOTL::memfd_exec`, `FilelessLOTL::living_off_the_land`
- Goal: never write a new binary to disk; use only existing system tools
- Difficulty: High
- Detection impact: Very High

### 5. C2 + Persistence + Anti-forensics

- Stubs: `C2Persistence::*`
- Goal: command channel, survive reboot, reduce artifacts
- Difficulty: High
- Detection impact: Depends on quality

## Recommended Order for Self-Implementation

1. Expand the io_uring skeleton (lowest risk, highest immediate value)
2. memfd / fileless loader
3. Process hollowing (careful, easy to crash targets)
4. LOTL chaining
5. eBPF rootkit (only if you have deep kernel experience)
6. C2 / persistence last

## Strength Evaluation (if you fill the stubs yourself)

**Current Aether (what exists today)**  
→ Strong educational / research metamorphic engine.  
→ Good against signature and simple behavioral detection.  
→ Not competitive with real advanced implants.

**If you correctly implement all five advanced areas yourself**  
→ It would become a **high-end research-grade** framework.  
→ Comparable to public advanced PoCs (not to closed commercial malware).  
→ Still detectable by well-funded defenders with good eBPF monitoring, memory forensics, and behavioral baselining.  
→ “Very strong” for open research, **not** “undetectable”.

## Release Recommendation

**Safe to release** when:

- Clear educational / defensive-research disclaimer is present in every file and README
- Infection remains controlled (only the file the user explicitly names)
- Advanced sections stay empty stubs **or** are clearly marked as incomplete research
- You explain the remaining detection surfaces honestly

**Risky / not recommended** when:

- You claim it is undetectable
- You ship fully working versions of hollowing + eBPF rootkit + C2 together
- There is no disclaimer

## Final Note

The current codebase + empty stubs is already a solid foundation for a high-quality Medium article or GitHub research
repository.  
Filling the stubs turns it into a serious personal research project.  
Keep the framing honest and you stay on the right side of responsible disclosure.
