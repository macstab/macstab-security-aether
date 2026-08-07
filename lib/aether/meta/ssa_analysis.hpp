/**
 * SPDX-License-Identifier: LicenseRef-Aether-Research
 * Copyright (c) 2024–2026 Macstab GmbH.
 *
 * Licensed under the Aether Research License in the repository root file
 * LICENSE. Authorized lab / lawful private use only as defined there.
 * By using this file you accept LICENSE. See docs/LICENSE_ACCEPTANCE.md.
 */

#pragma once
/**
 * @file ssa_analysis.hpp
 * @brief Function-local SSA-style dataflow for RealFunc (GPR + stack slots).
 *
 * Not LLVM full-program SSA. Per-function:
 *  - def/use sets per instruction
 *  - reaching defs / simple live-out
 *  - independence queries for safe shuffle/reorder
 *
 * Raises real morph from "hand catalogue only" toward analysis-driven domain.
 */

#include "aether/meta/real_ir.hpp"

#include <cstdint>
#include <vector>

namespace aether {

/** One abstract stack slot: RSP-relative offset at entry (bytes). */
struct StackSlot {
    int32_t disp = 0;
    bool valid = false;
};

struct InsnDataflow {
    uint32_t gpr_def = 0;   ///< bits written (kGpr*)
    uint32_t gpr_use = 0;   ///< bits read
    bool def_flags = false;
    bool use_flags = false;
    bool mem_def = false;   ///< stores
    bool mem_use = false;   ///< loads
    bool stack_only_mem = false; ///< mem only via RSP/RBP (frame)
    int32_t stack_disp = 0; ///< if stack_only_mem, approximate disp
    bool is_call = false;
    bool is_control = false;
    bool opaque = false;
};

struct FuncSsa {
    std::vector<InsnDataflow> per_insn;
    uint32_t live_in = 0;   ///< GPRs live at entry (args: rdi,rsi often)
    uint32_t live_out = 0;  ///< GPRs live at exit (rax typically)
    bool has_calls = false;
    bool has_nonstack_mem = false;
    bool analysis_ok = false;
};

/** Build per-instruction dataflow from lift metadata + byte patterns. */
FuncSsa analyze_func_ssa(const RealFunc& f);

/**
 * SSA-refined independence: no def-use / use-def / def-def conflict on GPRs/flags,
 * and no reordering across non-stack mem or calls.
 */
bool ssa_pair_independent(const FuncSsa& ssa, size_t i, size_t j);

/** True if function is safe for aggressive structural morph under Full domain. */
bool ssa_may_aggressive_morph(const FuncSsa& ssa);

} // namespace aether
