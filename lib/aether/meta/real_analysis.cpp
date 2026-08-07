/**
 * SPDX-License-Identifier: LicenseRef-Aether-Research
 * Copyright (c) 2024–2026 Macstab GmbH.
 *
 * Licensed under the Aether Research License in the repository root file
 * LICENSE. Authorized lab / lawful private use only as defined there.
 * By using this file you accept LICENSE. See docs/LICENSE_ACCEPTANCE.md.
 */

#include "aether/meta/real_analysis.hpp"

namespace aether {

RealEffectClass classify_real_insn(const RealInsn& in) {
    if (in.is_ret || in.is_uncond_jump || in.is_cond_jump || in.is_call || in.stops_fallthrough)
        return RealEffectClass::Control;
    if (in.bytes.empty())
        return RealEffectClass::Opaque;

    // Memory barrier (Phase II)
    if (in.has_memory)
        return RealEffectClass::Memory;

    const auto& b = in.bytes;
    // identity nops
    if (b[0] == 0x90)
        return RealEffectClass::Identity;
    if (b.size() >= 2 && b[0] == 0x66 && b[1] == 0x90)
        return RealEffectClass::Identity;
    if (b.size() >= 3 && b[0] == 0x0F && b[1] == 0x1F)
        return RealEffectClass::Identity;
    if (b.size() >= 3 && b[0] == 0x48 && b[1] == 0x87 && b[2] == 0xC0)
        return RealEffectClass::Identity;
    if (b.size() >= 2 && b[0] == 0x87 && b[1] == 0xC0)
        return RealEffectClass::Identity;
    if (b.size() >= 3 && b[0] == 0x48 && b[1] == 0x8D && b[2] == 0x00)
        return RealEffectClass::Identity;
    if (b.size() >= 5 && b[0] == 0x48 && b[1] == 0x8D && b[2] == 0x64 && b[3] == 0x24 && b[4] == 0x00)
        return RealEffectClass::Identity;

    // rax family
    if (b[0] == 0x50 || b[0] == 0x58)
        return RealEffectClass::PureRax;
    if (b[0] == 0xB8)
        return RealEffectClass::PureRax;
    if (b.size() >= 2 && b[0] == 0x89 && (b[1] == 0xF8 || b[1] == 0xF0))
        return RealEffectClass::PureRax;
    if (b.size() >= 2 && b[0] == 0x01 && (b[1] == 0xF8 || b[1] == 0xF0))
        return RealEffectClass::PureRax;
    if (b.size() >= 3 && b[0] == 0x48 && b[1] == 0x31 && b[2] == 0xC0)
        return RealEffectClass::PureRax;
    if (b.size() >= 3 && b[0] == 0x48 && b[1] == 0x29 && b[2] == 0xC0)
        return RealEffectClass::PureRax;
    if (b.size() >= 2 && b[0] == 0x31 && b[1] == 0xC0)
        return RealEffectClass::PureRax;
    if (b.size() >= 3 && b[0] == 0x48 && b[1] == 0xFF && (b[2] == 0xC0 || b[2] == 0xC8))
        return RealEffectClass::PureRax;
    if (b.size() >= 7 && b[0] == 0x48 && b[1] == 0xC7 && b[2] == 0xC0)
        return RealEffectClass::PureRax;
    if (b.size() >= 3 && b[0] == 0x6A && b[2] == 0x58)
        return RealEffectClass::PureRax;

    // rcx family
    if (b[0] == 0xB9)
        return RealEffectClass::PureRcx;
    if (b.size() >= 3 && b[0] == 0x48 && b[1] == 0x31 && b[2] == 0xC9)
        return RealEffectClass::PureRcx;
    if (b.size() >= 2 && b[0] == 0x31 && b[1] == 0xC9)
        return RealEffectClass::PureRcx;
    if (b.size() >= 3 && b[0] == 0x83 && b[1] == 0xF9)
        return RealEffectClass::PureRcx;
    if (b.size() >= 6 && b[0] == 0x81 && b[1] == 0xF9)
        return RealEffectClass::PureRcx;
    if (b.size() >= 2 && b[0] == 0x51 && b[1] == 0x59)
        return RealEffectClass::Identity;

    // other reg-only: push/pop r64 short forms, xor reg,reg, mov reg,imm
    if (b[0] >= 0x50 && b[0] <= 0x57)
        return RealEffectClass::PureGpr; // push r64
    if (b[0] >= 0x58 && b[0] <= 0x5F)
        return RealEffectClass::PureGpr; // pop r64
    if (b[0] >= 0xB8 && b[0] <= 0xBF && b.size() >= 5)
        return RealEffectClass::PureGpr; // mov r32, imm32
    if (b.size() >= 2 && b[0] == 0x31 && (b[1] & 0xC0) == 0xC0)
        return RealEffectClass::PureGpr; // xor r/m, r (reg-reg)
    if (b.size() >= 3 && b[0] == 0x48 && b[1] == 0x31 && (b[2] & 0xC0) == 0xC0)
        return RealEffectClass::PureGpr;

    if (!in.text.empty() && in.text.find("nop") == 0)
        return RealEffectClass::Identity;

    // Zydis said no memory and not control → PureGpr if only GPRs touched
    if (!in.has_memory && (in.gpr_read | in.gpr_write) != 0 &&
        (in.gpr_write & kGprRsp) == 0) {
        return RealEffectClass::PureGpr;
    }

    return RealEffectClass::Opaque;
}

bool is_real_identity(const RealInsn& in) {
    return classify_real_insn(in) == RealEffectClass::Identity;
}

bool real_cfg_edges_resolved(const RealFunc& f) {
    for (const auto& b : f.blocks) {
        if (b.end_idx == 0 || b.start_idx >= b.end_idx)
            continue;
        const RealInsn& last = f.insns[b.end_idx - 1];
        if (last.is_uncond_jump && b.branch < 0)
            return false;
        if (last.is_cond_jump && b.branch < 0)
            return false;
        if (last.is_call)
            return false;
    }
    return true;
}

bool real_may_permute_blocks(const RealFunc& f) {
    if (f.blocks.size() < 3)
        return false;
    if (!real_cfg_edges_resolved(f))
        return false;
    for (const auto& in : f.insns) {
        if (in.is_call)
            return false;
        if ((in.is_uncond_jump || in.is_cond_jump) && !in.has_imm_target)
            return false;
    }
    return true;
}

bool real_may_insert_after(const RealInsn& in) {
    if (in.is_ret || in.is_uncond_jump || in.is_cond_jump || in.is_call || in.stops_fallthrough)
        return false;
    auto c = classify_real_insn(in);
    // Identity/pure always; Memory allows NOP islands (no reorder across mem).
    return c == RealEffectClass::Identity || c == RealEffectClass::PureRax ||
           c == RealEffectClass::PureRcx || c == RealEffectClass::PureGpr ||
           c == RealEffectClass::Memory;
}

bool real_insn_pair_independent(const RealInsn& a, const RealInsn& b) {
    const auto ca = classify_real_insn(a);
    const auto cb = classify_real_insn(b);
    if (ca == RealEffectClass::Control || cb == RealEffectClass::Control ||
        ca == RealEffectClass::Opaque || cb == RealEffectClass::Opaque ||
        ca == RealEffectClass::Memory || cb == RealEffectClass::Memory)
        return false;
    // Call never reorders (already Control if is_call)
    if (a.is_call || b.is_call)
        return false;
    if (ca == RealEffectClass::Identity || cb == RealEffectClass::Identity)
        return true;
    if (ca == RealEffectClass::PureRax && cb == RealEffectClass::PureRax)
        return false;
    if (ca == RealEffectClass::PureRcx && cb == RealEffectClass::PureRcx)
        return false;
    // GPR masks: independent if no overlap write/read
    if ((a.gpr_write & (b.gpr_read | b.gpr_write)) != 0)
        return false;
    if ((b.gpr_write & (a.gpr_read | a.gpr_write)) != 0)
        return false;
    if (a.writes_flags && b.reads_flags)
        return false;
    if (b.writes_flags && a.reads_flags)
        return false;
    if (ca == RealEffectClass::PureRax && cb == RealEffectClass::PureRcx)
        return true;
    if (ca == RealEffectClass::PureRcx && cb == RealEffectClass::PureRax)
        return true;
    if (ca == RealEffectClass::PureGpr || cb == RealEffectClass::PureGpr)
        return (a.gpr_write & b.gpr_write) == 0 && (a.gpr_write & b.gpr_read) == 0 &&
               (b.gpr_write & a.gpr_read) == 0;
    return true;
}

bool real_func_has_memory(const RealFunc& f) {
    for (const auto& in : f.insns)
        if (in.has_memory)
            return true;
    return false;
}

bool real_func_has_calls(const RealFunc& f) {
    for (const auto& in : f.insns)
        if (in.is_call)
            return true;
    return false;
}

bool real_func_regs_only(const RealFunc& f) {
    for (const auto& in : f.insns) {
        if (in.has_memory || in.has_rip_rel)
            return false;
        auto c = classify_real_insn(in);
        if (c == RealEffectClass::Opaque || c == RealEffectClass::Memory)
            return false;
    }
    return true;
}

bool real_may_reencode(const RealInsn& in, MorphDomain domain) {
    auto c = classify_real_insn(in);
    if (c == RealEffectClass::Control || c == RealEffectClass::Opaque)
        return false;
    if (c == RealEffectClass::Memory)
        return domain == MorphDomain::Full; // still conservative: no reencode mem yet
    if (domain == MorphDomain::PureRegs)
        return c == RealEffectClass::Identity || c == RealEffectClass::PureRax ||
               c == RealEffectClass::PureRcx;
    return c == RealEffectClass::Identity || c == RealEffectClass::PureRax ||
           c == RealEffectClass::PureRcx || c == RealEffectClass::PureGpr;
}

} // namespace aether
