# AGENTS.md — Aether Research Project

## Project Purpose

**Company / lab** educational–defensive metamorphic research platform.
Demonstrates advanced code mutation, continuous self-modification, and controlled ELF infection techniques for academic and authorized red-team learning.

**This is not production malware.**  
Infection is strictly limited to a single file explicitly named by the user.

Product packaging: `docs/PRODUCT.md`, lab install: `docs/LAB_INSTALL.md`, gates: `docs/SCORECARD.md`.

---

## Repository Layout

See **`docs/REPOSITORY_LAYOUT.md`** (include/ + lib/ + apps/).

## Repository Layout (detail)

```
virus-security/
├── CMakeLists.txt
├── README.md
├── AGENTS.md                 ← this file
├── done.md                   ← status tracker
├── docs/
│   └── IMPLEMENTATION_PLAN.md
├── scripts/
│   └── format.sh
├── src/
│   ├── aether/               ← library modules (clean structure)
│   │   ├── common/           # rng, portable ELF
│   │   ├── noise/            # benign syscalls
│   │   ├── mutate/           # payload entropy
│   │   ├── meta/             # IR → catalogue → decode → transforms → assemble → pipeline
│   │   └── infect/           # controlled single-file infection
│   ├── apps/                 ← CLI mains
│   └── stubs/                ← empty advanced-technique placeholders
└── victim_clean              ← test ELF binary
```

---

## Key Components

| Path | Role | Status |
|------|------|--------|
| `lib/aether/meta/*` | Metamorphic IR, catalogue, permute/flatten, assembler, pipeline | Working |
| `lib/aether/noise/*` | Benign syscall noise | Working |
| `lib/aether/mutate/*` | Continuous payload mutation | Working |
| `lib/aether/infect/*` | Controlled single-file ELF infection | Working |
| `stubs/*` | Empty placeholders for advanced techniques | Stubs only |
| `apps/*` | Thin CLI entrypoints | Working |
| `docs/IMPLEMENTATION_PLAN.md` | Self-implementation roadmap + strength evaluation | Reference |
| `CMakeLists.txt` | Build system (`aether_core` + binaries) | Working |

---

## Build Instructions

```bash
mkdir -p build && cd build
cmake ..
make
```

Produces:
- `aether_next` — the working research binary
- `aether_stubs` — the empty-stub binary

---

## What Is Implemented

- Real disassembler → Intermediate Representation
- Instruction catalogue with semantic equivalents
- Block permutation + control-flow markers
- Real assembler that emits new machine code
- Live XOR with fresh entropy every execution
- Continuous in-memory mutation
- Random benign syscall noise (shuffled order + jitter)
- Changing ELF header mark every generation
- Size and instruction-stream shifting
- Controlled single-file ELF infection (only the file named on CLI)

---

## What Exists Only as Empty Stubs

These functions print `[STUB] … — empty` and return immediately.
No real implementation is present.

- Process hollowing / module stomping
- eBPF rootkit (self-hiding)
- Fully fileless + LOTL chains
- C2 / persistence / anti-forensics
- Production io_uring path (skeleton only)

See `docs/IMPLEMENTATION_PLAN.md` for the recommended order if you decide to implement any of them yourself.

---

## Usage (Research Only)

```bash
./aether_next <path-to-elf>
```

Only the single ELF file you name is modified.
No directory scanning, no spreading, no network activity.

---

## Disclaimer for Agents / Contributors

- Keep infection strictly controlled (single named file).
- Do not add automatic spreading, mass infection, or C2.
- Do not remove or weaken the educational disclaimer.
- Advanced techniques must remain clearly marked as stubs or incomplete research unless fully documented as such.
- Prefer honest statements about remaining detection surfaces over claims of undetectability.

---

## Strength Assessment (Current)

- Strong educational / research metamorphic engine.
- Effective against classic signature-based detection.
- Not competitive with real advanced implants used in the wild.
- Suitable for Medium articles, academic demos, and red-team learning.

---

## License / Intent

Research and educational use only.
