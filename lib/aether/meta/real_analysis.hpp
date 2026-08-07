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
 * @file real_analysis.hpp
 * @brief Effect / liveness model for RealFunc (analysis-gated morph).
 *
 * Industry phases I–II: PureRax/Rcx/Gpr, Memory barriers, MorphDomain.
 */

#include "aether/meta/real_ir.hpp"

#include <cstdint>

namespace aether {

enum class RealEffectClass : uint8_t {
    Identity = 0, ///< multi-byte nop / xchg reg,reg / lea identity
    PureRax,      ///< clear/mov/inc/dec/push/pop rax family (research pure)
    PureRcx,      ///< rcx mov/xor/cmp family
    PureGpr,      ///< other reg-only (no mem) — industry domain expansion
    Memory,       ///< has memory operand — barrier for free reorder
    Control,      ///< jmp/jcc/call/ret
    Opaque,       ///< everything else — do not reorder across freely
};

/** Analysis domain for industry morph (config). */
enum class MorphDomain : int {
    PureRegs = 0,  ///< RAX/RCX pure research domain
    RegsFlags = 1, ///< GPRs + flags, no memory
    MemSafe = 2,   ///< allow mem but treat as barrier (no reorder across)
    Full = 3,      ///< best-effort full (still no SEH claims)
};

RealEffectClass classify_real_insn(const RealInsn& in);

bool is_real_identity(const RealInsn& in);
bool real_cfg_edges_resolved(const RealFunc& f);
bool real_may_permute_blocks(const RealFunc& f);
bool real_may_insert_after(const RealInsn& in);
bool real_insn_pair_independent(const RealInsn& a, const RealInsn& b);

bool real_func_has_memory(const RealFunc& f);
bool real_func_has_calls(const RealFunc& f);
bool real_func_regs_only(const RealFunc& f);
bool real_may_reencode(const RealInsn& in, MorphDomain domain);

} // namespace aether
