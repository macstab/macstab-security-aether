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
 * @file morph_real.hpp
 * @brief Industry-path real morph: Zydis RealFunc IR + analysis-gated transforms.
 *
 * Policies (SCOPE.md aggression):
 *  - Identity: re-layout only
 *  - Safe: nops + safe block permute + encoding diversify + insn shuffle
 *  - Lab: Safe + extra passes
 *
 * Not claimed: full ABI, mem ops, PE, arbitrary x86 rewrite.
 */

#include "aether/meta/real_ir.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace aether {

enum class MorphPolicy : int {
    Identity = 0,
    Safe = 1,
    Lab = 2,
};

/**
 * Re-layout RealFunc to a contiguous buffer: entry block first, then others.
 * Rewrites relative JMP/Jcc at block ends to rel32 forms; seals fallthrough
 * with E9 when physical order ≠ logical fallthrough.
 */
std::vector<uint8_t> assemble_real(const RealFunc& f);

/** Insert random multi-byte NOPs between analysis-safe insns. */
void real_expand_nops(RealFunc& f, int min_nops = 1, int max_nops = 3);

/** Permute blocks when real_may_permute_blocks. */
void real_permute_blocks(RealFunc& f);

/**
 * Replace pure/identity encodings with semantic equivalents (mov/xor/nop forms).
 * Never touches control-flow or opaque insns.
 */
void real_diversify_encodings(RealFunc& f);

/** Analysis-gated adjacent swaps of independent pure/identity insns. */
void real_safe_shuffle_insns(RealFunc& f);

/**
 * Full real morph under @p policy.
 * disasm_real → policy transforms → assemble_real.
 */
std::vector<uint8_t> morph_real(const uint8_t* code,
                                size_t len,
                                uint64_t base_address = 0x1000,
                                MorphPolicy policy = MorphPolicy::Safe);

/** Alias: Safe policy (backward compatible). */
std::vector<uint8_t> morph_real_restricted(const uint8_t* code,
                                           size_t len,
                                           uint64_t base_address = 0x1000);

/**
 * Pure-function subset interpreter over RealFunc.
 * @param rdi RSI-style: initial RDI (SysV first arg), used for mov eax,edi etc.
 * @param rsi initial RSI (second arg)
 */
std::optional<uint32_t> interpret_real_pure(const RealFunc& f,
                                            uint64_t rdi = 0,
                                            uint64_t rsi = 0);

/** Split large blocks to grow CFG surface for permute (fallthrough chain). */
void real_split_blocks(RealFunc& f, size_t max_insns = 6);

} // namespace aether
