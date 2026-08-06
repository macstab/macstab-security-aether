/**
 * SPDX-License-Identifier: LicenseRef-Aether-Research
 * Copyright (c) 2024–2026 Aether Research Project contributors.
 *
 * Licensed under the Aether Research License in the repository root file
 * LICENSE. Authorized lab / lawful private use only as defined there.
 * By using this file you accept LICENSE. See docs/LICENSE_ACCEPTANCE.md.
 */

#pragma once
/**
 * @file transforms.hpp
 * @brief IR-level metamorphic transforms (shrink, expand, permute, flatten).
 */

#include "aether/meta/ir.hpp"

namespace aether {

/**
 * Removes some redundant NOP / JUNK / XCHG_RAX_RAX instructions from each
 * block. Keeps the first filler in a run; later fillers in the same run are
 * dropped with probability 2/3. Does not change control-flow edges.
 * @param f IR function modified in place
 */
void shrink(IRFunc& f);

/**
 * Inserts extra NOP and JUNK instructions after non-terminator ops.
 * Roughly 1/4 chance of a NOP and 1/6 chance of a JUNK after each kept insn.
 * Never inserts after JMP, JE, or RET. Increases size and instruction variety.
 * @param f IR function modified in place
 */
void expand(IRFunc& f);

/**
 * Physically reorders basic blocks while preserving control flow.
 * Keeps the entry block first, shuffles the rest, remaps all block ids and
 * edge targets, and inserts an explicit JMP when a block's fallthrough is
 * no longer the next block in memory order.
 * No-op if fewer than 3 blocks.
 * @param f IR function modified in place
 */
void permute_blocks(IRFunc& f);

/**
 * Def-use / effect classes for educational IR (RAX, RCX, ZF, stack).
 * Used to justify intra-block reordering and document permute safety.
 */
enum class EffectBit : uint8_t {
    None = 0,
    Rax = 1 << 0,
    Rcx = 1 << 1,
    Zf = 1 << 2,
    Stack = 1 << 3,
    Ctrl = 1 << 4, ///< JMP/JE/RET — never reorder past
    Opaque = 1 << 5, ///< RAW — conservatively blocks reordering
};

struct InsnEffect {
    uint8_t uses = 0;
    uint8_t defs = 0;
};

/** Semantic uses/defs for one Op (research IR model). */
InsnEffect effect_of(Op op);

/** True if two adjacent non-control insns may be swapped without data races. */
bool insn_pair_independent(Op a, Op b);

/**
 * Intra-block: randomly swap independent adjacent pairs (def-use gated).
 * Never moves JMP/JE/RET/RAW. Call after expand to diversify layout safely.
 */
void safe_permute_insns(IRFunc& f);

/**
 * Replaces the CFG with a classic control-flow-flattened state machine.
 * Layout after transform:
 *  - block 0: dispatcher (cmp rcx,state / je body … / ret)
 *  - block 1: entry stub (mov ecx, entry_state; jmp dispatcher)
 *  - blocks 2..: original bodies ending in set-next-state + jmp dispatcher
 *  - optional JE epilogue blocks for conditional branches
 * Uses rcx as the state register. No-op if fewer than 2 blocks.
 * @param f IR function replaced in place
 */
void flatten(IRFunc& f);

/**
 * Rewrites instructions into alternate semantic implementations at IR level.
 * Example: CLEAR_RAX may become MOV_RAX_IMM(0), or a multi-op sequence;
 * NOP may become XCHG/JUNK; constants may be rebuilt as clear+inc chains.
 * Makes two generations differ in *implementation*, not only encoding bytes.
 * @param f IR function modified in place
 */
void diversify_implementation(IRFunc& f);

/**
 * Aggressive block permutation: runs permute_blocks 1–3 times with optional
 * expand between passes so layout and size keep shifting.
 * @param f IR function modified in place
 */
void heavy_permute(IRFunc& f);

/**
 * Strong metamorphic permutation suite (research-grade):
 *  - split large blocks (more permutation surface)
 *  - multi-strategy layout shuffles (Fisher–Yates, rotate, reverse mids)
 *  - trampoline JMP islands so physical order ≠ logical order
 *  - intra-block shuffle of pure filler islands (NOP/JUNK)
 *  - multiple heavy_permute passes
 * Preserves CFG semantics via fallthrough/JMP repair.
 */
void world_class_permute(IRFunc& f);

/**
 * Applies a random multiset of transforms in random order.
 * Draws k ∈ [min_steps, max_steps] ops from
 * {shrink, expand, diversify, permute, heavy_permute, flatten} and runs them.
 * This is the core of “order is random” per stage.
 * @param f         IR function modified in place
 * @param min_steps minimum transform applications
 * @param max_steps maximum transform applications
 * @return true if flatten was applied at least once
 */
bool apply_random_transform_schedule(IRFunc& f, int min_steps, int max_steps);

/**
 * Named structural design for one generation layer (not only random soup).
 * Each mode biases transforms so layers look qualitatively different.
 */
enum class LayerMode : int {
    PermuteHeavy = 0, ///< many block permutations (+ light expand)
    Flatten,          ///< force CFG flattening
    Wide,             ///< heavy expand / junk, little control-flow change
    Dense,            ///< diversify + shrink (compact re-encoding)
    Minimal,          ///< few transforms, small change
    Mixed,            ///< full random schedule
    Count
};

/** Applies a fixed structural mode to IR (used by probabilistic layer chain). */
void apply_layer_mode(IRFunc& f, LayerMode mode);

/** Picks a random LayerMode uniformly. */
LayerMode random_layer_mode();

/** Human-readable mode name for logs / articles. */
const char* layer_mode_name(LayerMode mode);

} // namespace aether
