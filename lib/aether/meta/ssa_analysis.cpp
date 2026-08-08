/**
 * SPDX-License-Identifier: LicenseRef-Aether-Research
 * Copyright (c) 2024–2026 Macstab GmbH.
 *
 * Licensed under the Aether Research License in the repository root file
 * LICENSE. Authorized lab / lawful private use only as defined there.
 * By using this file you accept LICENSE. See docs/LICENSE_ACCEPTANCE.md.
 */

#include "aether/meta/ssa_analysis.hpp"

#include "aether/meta/real_analysis.hpp"

namespace aether {
namespace {

void fill_from_insn(const RealInsn& in, InsnDataflow& d) {
    d.gpr_def = in.gpr_write;
    d.gpr_use = in.gpr_read;
    d.def_flags = in.writes_flags;
    d.use_flags = in.reads_flags;
    d.is_call = in.is_call;
    d.is_control =
        in.is_ret || in.is_uncond_jump || in.is_cond_jump || in.is_call || in.stops_fallthrough;
    d.mem_use = in.has_memory;
    d.mem_def = in.has_memory; // conservative: treat mem as both if unknown
    d.stack_only_mem = false;
    d.opaque = false;

    const auto& b = in.bytes;
    // Stack-only patterns refine mem
    if (b.size() >= 4 && b[0] == 0x48 && (b[1] == 0x89 || b[1] == 0x8B) && b[2] == 0x04 &&
        b[3] == 0x24) {
        d.stack_only_mem = true;
        d.stack_disp = 0;
        d.mem_def = (b[1] == 0x89);
        d.mem_use = (b[1] == 0x8B);
    }
    if (b.size() >= 5 && b[0] == 0x48 && (b[1] == 0x89 || b[1] == 0x8B) && b[2] == 0x44 &&
        b[3] == 0x24) {
        d.stack_only_mem = true;
        d.stack_disp = (int8_t)b[4];
        d.mem_def = (b[1] == 0x89);
        d.mem_use = (b[1] == 0x8B);
    }
    // push/pop: stack mem + gpr
    if (b.size() == 1 && b[0] >= 0x50 && b[0] <= 0x57) {
        d.mem_def = true;
        d.stack_only_mem = true;
        d.gpr_use |= (1u << (b[0] - 0x50));
        d.gpr_def |= kGprRsp;
        d.gpr_use |= kGprRsp;
    }
    if (b.size() == 1 && b[0] >= 0x58 && b[0] <= 0x5F) {
        d.mem_use = true;
        d.stack_only_mem = true;
        d.gpr_def |= (1u << (b[0] - 0x58));
        d.gpr_def |= kGprRsp;
        d.gpr_use |= kGprRsp;
    }

    auto cls = classify_real_insn(in);
    if (cls == RealEffectClass::Opaque)
        d.opaque = true;
    if (cls == RealEffectClass::Memory && !d.stack_only_mem)
        d.opaque = false; // keep mem barrier but not opaque

    // If lift had no gpr masks, fall back class bits
    if (d.gpr_def == 0 && d.gpr_use == 0) {
        if (cls == RealEffectClass::PureRax) {
            d.gpr_def |= kGprRax;
            d.gpr_use |= kGprRax;
        } else if (cls == RealEffectClass::PureRcx) {
            d.gpr_def |= kGprRcx;
            d.gpr_use |= kGprRcx;
        }
    }
}

} // namespace

FuncSsa analyze_func_ssa(const RealFunc& f) {
    FuncSsa s;
    s.per_insn.resize(f.insns.size());
    s.live_in = kGprRdi | kGprRsi; // SysV args commonly live
    s.live_out = kGprRax;
    s.analysis_ok = !f.insns.empty();

    for (size_t i = 0; i < f.insns.size(); i++) {
        fill_from_insn(f.insns[i], s.per_insn[i]);
        if (s.per_insn[i].is_call)
            s.has_calls = true;
        if (s.per_insn[i].mem_def || s.per_insn[i].mem_use) {
            if (!s.per_insn[i].stack_only_mem)
                s.has_nonstack_mem = true;
        }
    }
    return s;
}

bool ssa_pair_independent(const FuncSsa& ssa, size_t i, size_t j) {
    if (i >= ssa.per_insn.size() || j >= ssa.per_insn.size())
        return false;
    const InsnDataflow& a = ssa.per_insn[i];
    const InsnDataflow& b = ssa.per_insn[j];
    if (a.is_control || b.is_control || a.is_call || b.is_call)
        return false;
    if (a.opaque || b.opaque)
        return false;
    // Non-stack memory is a full barrier
    if ((a.mem_def || a.mem_use) && !a.stack_only_mem)
        return false;
    if ((b.mem_def || b.mem_use) && !b.stack_only_mem)
        return false;
    // Stack mem: conflict if same disp and either defs
    if (a.stack_only_mem && b.stack_only_mem) {
        if (a.stack_disp == b.stack_disp && (a.mem_def || b.mem_def))
            return false;
    }
    // Classic RAW/WAR/WAW on GPRs
    if ((a.gpr_def & b.gpr_use) != 0)
        return false;
    if ((b.gpr_def & a.gpr_use) != 0)
        return false;
    if ((a.gpr_def & b.gpr_def) != 0)
        return false;
    if (a.def_flags && b.use_flags)
        return false;
    if (b.def_flags && a.use_flags)
        return false;
    if (a.def_flags && b.def_flags)
        return false;
    return true;
}

bool ssa_may_aggressive_morph(const FuncSsa& ssa) {
    if (!ssa.analysis_ok)
        return false;
    if (ssa.has_calls)
        return false;
    // Non-stack mem still ok for structural morph, not free reorder
    return true;
}

} // namespace aether
