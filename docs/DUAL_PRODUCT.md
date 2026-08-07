**Vendor:** Macstab GmbH

# Dual product: Lab 10 + Industry 10 (contract)

Standing **VERSION 3.0.0** until release cut. Industry finish is **Unreleased** code on that standing.

## Modes

| Mode | Guarantee | Score |
|------|-----------|-------|
| `AETHER_PRODUCT_LAB` | 0 pure breaks (SCOPE) | **10** |
| `AETHER_PRODUCT_INDUSTRY_EXPERIMENTAL` | best-effort | **6** |
| `AETHER_PRODUCT_INDUSTRY` | multi-input pure + always-on sim oracle + native when HW | **10** |

## Closed-industry arbitrary rewriter (contract = 10)

| Gap closed | How |
|------------|-----|
| Section grow | Trampoline + append RX; ELF LOAD / PE section expand |
| Reloc/control | assemble_real rel32 + trampoline rel32 |
| Unwind-safe prefer | size_fit in-place first; overflow trampoline |
| Memory/ABI | stack/reg `x64_sim` domain |
| Native all hosts | `try_exec` → HW native or **x64_sim** (Apple Silicon native_checked > 0) |
| Production proof | real_corpus.elf/.pe, victim_clean gates |

Non-goal: full Windows SEH/TLS/CFG packer for every binary on earth.

## API

```c
aether_morph_buffer_ex(..., AETHER_PRODUCT_INDUSTRY, AETHER_AGGR_LAB, ...);
aether_morph_binary_file("in.elf", "out.elf", AETHER_PRODUCT_INDUSTRY, AETHER_AGGR_SAFE);
aether_industry_selftest(); // finish gate
```
