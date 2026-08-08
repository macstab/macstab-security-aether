/**
 * SPDX-License-Identifier: LicenseRef-Aether-Research
 * Copyright (c) 2024–2026 Macstab GmbH.
 *
 * Licensed under the Aether Research License in the repository root file
 * LICENSE. Authorized lab / lawful private use only as defined there.
 * By using this file you accept LICENSE. See docs/LICENSE_ACCEPTANCE.md.
 */

#include "aether/meta/morph_real.hpp"

#include "aether/common/rng.hpp"
#include "aether/meta/decode_real.hpp"
#include "aether/meta/morph_engine.hpp"
#include "aether/meta/real_analysis.hpp"
#include "aether/meta/ssa_analysis.hpp"

#include <algorithm>
#include <cstring>
#include <optional>

namespace aether {
namespace {

void emit_u32(std::vector<uint8_t>& o, uint32_t v) {
    o.push_back((uint8_t)v);
    o.push_back((uint8_t)(v >> 8));
    o.push_back((uint8_t)(v >> 16));
    o.push_back((uint8_t)(v >> 24));
}

/** Multi-byte NOP catalogue (identity). */
std::vector<uint8_t> pick_nop_bytes() {
    const int k = rnd(0, 4);
    switch (k) {
    case 0:
        return {0x90};
    case 1:
        return {0x66, 0x90};
    case 2:
        return {0x0F, 0x1F, 0x00};
    case 3:
        return {0x0F, 0x1F, 0x40, 0x00};
    default:
        return {0x48, 0x87, 0xC0}; // xchg rax, rax
    }
}

RealInsn make_nop_insn() {
    RealInsn in;
    in.bytes = pick_nop_bytes();
    in.length = in.bytes.size();
    in.text = "nop";
    in.is_branch = false;
    return in;
}

bool ends_block(const RealInsn& in) {
    return in.is_ret || in.is_uncond_jump || (in.is_cond_jump) || in.stops_fallthrough;
}

/**
 * Recompute relative branch targets from raw bytes after layout/address changes.
 * Without this, expand_nops leaves stale absolute targets and CFG edges die.
 */
void refresh_branch_targets(RealFunc& f) {
    for (auto& in : f.insns) {
        if (in.bytes.empty())
            continue;
        const uint8_t b0 = in.bytes[0];
        if (b0 == 0xEB && in.bytes.size() >= 2) {
            int8_t rel = (int8_t)in.bytes[1];
            in.is_uncond_jump = true;
            in.is_branch = true;
            in.stops_fallthrough = true;
            in.has_imm_target = true;
            in.branch_target = in.address + in.length + (int64_t)rel;
        } else if (b0 == 0xE9 && in.bytes.size() >= 5) {
            int32_t rel;
            std::memcpy(&rel, in.bytes.data() + 1, 4);
            in.is_uncond_jump = true;
            in.is_branch = true;
            in.stops_fallthrough = true;
            in.has_imm_target = true;
            in.branch_target = in.address + 5 + (int64_t)rel;
        } else if (b0 >= 0x70 && b0 <= 0x7F && in.bytes.size() >= 2) {
            int8_t rel = (int8_t)in.bytes[1];
            in.is_cond_jump = true;
            in.is_branch = true;
            in.has_imm_target = true;
            in.branch_target = in.address + 2 + (int64_t)rel;
        } else if (b0 == 0x0F && in.bytes.size() >= 6 && in.bytes[1] >= 0x80 &&
                   in.bytes[1] <= 0x8F) {
            int32_t rel;
            std::memcpy(&rel, in.bytes.data() + 2, 4);
            in.is_cond_jump = true;
            in.is_branch = true;
            in.has_imm_target = true;
            in.branch_target = in.address + 6 + (int64_t)rel;
        }
    }
    build_real_cfg(f);
}

/** Encode unconditional jmp to block target as E9 rel32 (disp 0; patch later). */
std::vector<uint8_t> encode_jmp_rel32_placeholder() {
    return {0xE9, 0x00, 0x00, 0x00, 0x00};
}

/**
 * Encode jcc as 0F 8x rel32. Condition from original short/near jcc bytes.
 */
std::vector<uint8_t> encode_jcc_rel32_placeholder(const RealInsn& orig) {
    uint8_t cc = 0x84; // default JE
    if (!orig.bytes.empty()) {
        const uint8_t b0 = orig.bytes[0];
        if (b0 >= 0x70 && b0 <= 0x7F)
            cc = static_cast<uint8_t>(0x80 + (b0 - 0x70));
        else if (orig.bytes.size() >= 2 && b0 == 0x0F && orig.bytes[1] >= 0x80 &&
                 orig.bytes[1] <= 0x8F)
            cc = orig.bytes[1];
    }
    return {0x0F, cc, 0x00, 0x00, 0x00, 0x00};
}

struct EdgeFix {
    size_t disp_at = 0;   ///< offset of rel32 field in output
    size_t instr_end = 0; ///< IP after instruction
    int target_block = -1;
};

void patch_rel32(std::vector<uint8_t>& code,
                 const EdgeFix& fx,
                 const std::vector<size_t>& block_off) {
    if (fx.target_block < 0 || fx.target_block >= (int)block_off.size())
        return;
    int32_t rel = (int32_t)((int64_t)block_off[(size_t)fx.target_block] - (int64_t)fx.instr_end);
    std::memcpy(code.data() + fx.disp_at, &rel, 4);
}

} // namespace

std::vector<uint8_t> assemble_real(const RealFunc& f) {
    if (f.blocks.empty() || f.insns.empty())
        return {};

    std::vector<int> order;
    order.reserve(f.blocks.size());
    if (f.entry >= 0 && f.entry < (int)f.blocks.size())
        order.push_back(f.entry);
    for (size_t i = 0; i < f.blocks.size(); i++) {
        if ((int)i != f.entry)
            order.push_back((int)i);
    }

    std::vector<uint8_t> code;
    std::vector<EdgeFix> fixes;
    std::vector<size_t> block_off(f.blocks.size(), 0);

    for (size_t oi = 0; oi < order.size(); oi++) {
        const int bid = order[oi];
        if (bid < 0 || bid >= (int)f.blocks.size())
            continue;
        const RealBlock& b = f.blocks[(size_t)bid];
        block_off[(size_t)bid] = code.size();

        for (size_t ii = b.start_idx; ii < b.end_idx && ii < f.insns.size(); ii++) {
            const RealInsn& in = f.insns[ii];
            const bool is_last = (ii + 1 == b.end_idx);

            if (is_last && in.is_uncond_jump) {
                // Re-encode from CFG edge when resolved; else keep original bytes.
                if (b.branch < 0) {
                    code.insert(code.end(), in.bytes.begin(), in.bytes.end());
                } else {
                    auto enc = encode_jmp_rel32_placeholder();
                    size_t start = code.size();
                    code.insert(code.end(), enc.begin(), enc.end());
                    fixes.push_back(EdgeFix{start + 1, start + enc.size(), b.branch});
                }
            } else if (is_last && in.is_cond_jump) {
                if (b.branch < 0) {
                    code.insert(code.end(), in.bytes.begin(), in.bytes.end());
                } else {
                    auto enc = encode_jcc_rel32_placeholder(in);
                    size_t start = code.size();
                    code.insert(code.end(), enc.begin(), enc.end());
                    fixes.push_back(EdgeFix{start + 2, start + enc.size(), b.branch});
                }
            } else if (in.is_uncond_jump || in.is_cond_jump) {
                // Mid-block CF: keep original bytes (expand must not alter distances
                // inside this block — see real_expand_nops gate).
                code.insert(code.end(), in.bytes.begin(), in.bytes.end());
            } else {
                code.insert(code.end(), in.bytes.begin(), in.bytes.end());
            }
        }

        const int next_phys = (oi + 1 < order.size()) ? order[oi + 1] : -1;
        const RealInsn* last = nullptr;
        if (b.end_idx > b.start_idx && b.end_idx <= f.insns.size())
            last = &f.insns[b.end_idx - 1];

        // Seal fallthrough / jcc-not-taken
        if (last && last->is_cond_jump) {
            if (b.fallthrough >= 0 && b.fallthrough != next_phys) {
                auto enc = encode_jmp_rel32_placeholder();
                size_t start = code.size();
                code.insert(code.end(), enc.begin(), enc.end());
                fixes.push_back(EdgeFix{start + 1, start + enc.size(), b.fallthrough});
            }
        } else if (last && !last->is_ret && !last->is_uncond_jump && !last->stops_fallthrough) {
            if (b.fallthrough >= 0 && b.fallthrough != next_phys) {
                auto enc = encode_jmp_rel32_placeholder();
                size_t start = code.size();
                code.insert(code.end(), enc.begin(), enc.end());
                fixes.push_back(EdgeFix{start + 1, start + enc.size(), b.fallthrough});
            }
        }
    }

    for (const auto& fx : fixes)
        patch_rel32(code, fx, block_off);

    return code;
}

void real_expand_nops(RealFunc& f, int min_nops, int max_nops) {
    if (f.insns.empty() || f.blocks.empty())
        return;
    if (min_nops < 0)
        min_nops = 0;
    if (max_nops < min_nops)
        max_nops = min_nops;

    // Expand per-block so CFG edges (block ids) stay valid. Never rebuild
    // edges from stale rel8 in raw bytes after size changes.
    std::vector<RealInsn> out;
    std::vector<RealBlock> nb;
    out.reserve(f.insns.size() * 2);
    nb.reserve(f.blocks.size());

    for (const auto& b : f.blocks) {
        RealBlock ob;
        ob.id = b.id;
        ob.fallthrough = b.fallthrough;
        ob.branch = b.branch;
        ob.start_idx = out.size();
        // Do not expand blocks that contain mid-block jumps (relative distances).
        bool mid_cf = false;
        for (size_t i = b.start_idx; i + 1 < b.end_idx && i < f.insns.size(); i++) {
            if (f.insns[i].is_uncond_jump || f.insns[i].is_cond_jump || f.insns[i].is_call) {
                mid_cf = true;
                break;
            }
        }
        for (size_t i = b.start_idx; i < b.end_idx && i < f.insns.size(); i++) {
            const bool last = (i + 1 == b.end_idx);
            if (!mid_cf && !last && real_may_insert_after(f.insns[i]) && rnd(0, 2) == 0) {
                out.push_back(f.insns[i]);
                const int n = rnd(min_nops, max_nops);
                for (int k = 0; k < n; k++)
                    out.push_back(make_nop_insn());
            } else {
                out.push_back(f.insns[i]);
            }
        }
        ob.end_idx = out.size();
        nb.push_back(ob);
    }

    f.insns.swap(out);
    f.blocks.swap(nb);
    size_t off = 0;
    for (auto& in : f.insns) {
        in.address = f.base_address + off;
        in.offset = off;
        in.length = in.bytes.size();
        off += in.length;
    }
}

void real_permute_blocks(RealFunc& f) {
    // Analysis gate: unresolved edges / calls refuse permute.
    if (!real_may_permute_blocks(f))
        return;
    if (f.blocks.size() < 3)
        return;

    // Materialize per-block insn vectors
    std::vector<std::vector<RealInsn>> bodies(f.blocks.size());
    for (const auto& b : f.blocks) {
        for (size_t i = b.start_idx; i < b.end_idx && i < f.insns.size(); i++)
            bodies[(size_t)b.id].push_back(f.insns[i]);
    }
    std::vector<int> fall(f.blocks.size(), -1);
    std::vector<int> br(f.blocks.size(), -1);
    for (const auto& b : f.blocks) {
        fall[(size_t)b.id] = b.fallthrough;
        br[(size_t)b.id] = b.branch;
    }
    const int entry_old = f.entry;

    // Shuffle non-entry ids, entry stays id 0 after remap
    std::vector<int> order;
    order.push_back(entry_old);
    for (int i = 0; i < (int)f.blocks.size(); i++)
        if (i != entry_old)
            order.push_back(i);
    // Fisher–Yates on tail
    for (int i = (int)order.size() - 1; i > 1; i--) {
        int j = rnd(1, i);
        std::swap(order[(size_t)i], order[(size_t)j]);
    }

    std::vector<int> old_to_new(f.blocks.size(), -1);
    for (int ni = 0; ni < (int)order.size(); ni++)
        old_to_new[(size_t)order[(size_t)ni]] = ni;

    RealFunc nf;
    nf.base_address = f.base_address;
    nf.entry = 0;
    size_t off = 0;
    nf.blocks.resize(order.size());
    for (int ni = 0; ni < (int)order.size(); ni++) {
        const int oid = order[(size_t)ni];
        RealBlock nb;
        nb.id = ni;
        nb.start_idx = nf.insns.size();
        for (auto& in : bodies[(size_t)oid]) {
            in.offset = off;
            in.address = nf.base_address + off;
            in.length = in.bytes.size();
            off += in.length;
            nf.insns.push_back(std::move(in));
        }
        nb.end_idx = nf.insns.size();
        nb.fallthrough = fall[(size_t)oid] >= 0 ? old_to_new[(size_t)fall[(size_t)oid]] : -1;
        nb.branch = br[(size_t)oid] >= 0 ? old_to_new[(size_t)br[(size_t)oid]] : -1;
        nf.blocks[(size_t)ni] = nb;
    }
    f = std::move(nf);
}

void real_diversify_encodings(RealFunc& f) {
    for (auto& in : f.insns) {
        const auto cls = classify_real_insn(in);
        if (cls == RealEffectClass::Identity) {
            in.bytes = pick_nop_bytes();
            in.length = in.bytes.size();
            in.text = "nop";
            continue;
        }
        if (in.bytes.empty())
            continue;
        const auto& b = in.bytes;

        if (cls == RealEffectClass::PureRax) {
            // mov eax, imm32 (B8) → alternate forms
            if (b[0] == 0xB8 && b.size() >= 5) {
                uint32_t imm;
                std::memcpy(&imm, b.data() + 1, 4);
                if (rnd(0, 1) == 0) {
                    in.bytes = {0x48, 0xC7, 0xC0};
                    emit_u32(in.bytes, imm);
                } else {
                    in.bytes = {0xB8};
                    emit_u32(in.bytes, imm);
                }
                in.length = in.bytes.size();
                continue;
            }
            if (b.size() >= 7 && b[0] == 0x48 && b[1] == 0xC7 && b[2] == 0xC0) {
                uint32_t imm;
                std::memcpy(&imm, b.data() + 3, 4);
                if (rnd(0, 1) == 0) {
                    in.bytes = {0xB8};
                    emit_u32(in.bytes, imm);
                } else {
                    in.bytes = {0x48, 0xC7, 0xC0};
                    emit_u32(in.bytes, imm);
                }
                in.length = in.bytes.size();
                continue;
            }
            // xor rax,rax / sub rax,rax / xor eax,eax
            if ((b.size() >= 3 && b[0] == 0x48 && b[1] == 0x31 && b[2] == 0xC0) ||
                (b.size() >= 3 && b[0] == 0x48 && b[1] == 0x29 && b[2] == 0xC0) ||
                (b.size() >= 2 && b[0] == 0x31 && b[1] == 0xC0)) {
                const int mode = rnd(0, 2);
                if (mode == 0)
                    in.bytes = {0x48, 0x31, 0xC0};
                else if (mode == 1)
                    in.bytes = {0x48, 0x29, 0xC0};
                else
                    in.bytes = {0x31, 0xC0};
                in.length = in.bytes.size();
                in.text = "xor";
                continue;
            }
            continue;
        }

        if (cls == RealEffectClass::PureRcx) {
            if ((b.size() >= 3 && b[0] == 0x48 && b[1] == 0x31 && b[2] == 0xC9) ||
                (b.size() >= 2 && b[0] == 0x31 && b[1] == 0xC9)) {
                in.bytes = (rnd(0, 1) == 0) ? std::vector<uint8_t>{0x48, 0x31, 0xC9}
                                            : std::vector<uint8_t>{0x31, 0xC9};
                in.length = in.bytes.size();
                in.text = "xor ecx";
            }
            continue;
        }

        if (cls == RealEffectClass::PureGpr && b.size() == 2 && b[0] == 0x31 &&
            (b[1] & 0xC0) == 0xC0) {
            uint8_t modrm = b[1];
            if ((modrm & 7) == ((modrm >> 3) & 7) && rnd(0, 1) == 0) {
                in.bytes = {0x48, 0x31, modrm};
                in.length = 3;
                in.text = "xor";
            }
        }
    }
    size_t off = 0;
    for (auto& in : f.insns) {
        in.offset = off;
        in.address = f.base_address + off;
        in.length = in.bytes.size();
        off += in.length;
    }
}

void real_safe_shuffle_insns(RealFunc& f) {
    FuncSsa ssa = analyze_func_ssa(f);
    for (auto& b : f.blocks) {
        if (b.end_idx < b.start_idx + 2)
            continue;
        const int passes = rnd(1, 3);
        for (int p = 0; p < passes; p++) {
            for (size_t i = b.start_idx; i + 1 < b.end_idx && i + 1 < f.insns.size(); i++) {
                // Never move last insn of block (may be terminator)
                if (i + 2 >= b.end_idx)
                    break;
                // Classic class gate (must pass) + SSA refinement when available
                if (!real_insn_pair_independent(f.insns[i], f.insns[i + 1]))
                    continue;
                if (ssa.analysis_ok && !ssa_pair_independent(ssa, i, i + 1))
                    continue;
                if (rnd(0, 1) == 0)
                    std::swap(f.insns[i], f.insns[i + 1]);
            }
        }
    }
    size_t off = 0;
    for (auto& in : f.insns) {
        in.offset = off;
        in.address = f.base_address + off;
        in.length = in.bytes.size();
        off += in.length;
    }
}

std::vector<uint8_t>
morph_real(const uint8_t* code, size_t len, uint64_t base_address, MorphPolicy policy) {
    // Delegate to industry MorphEngine (single pipeline implementation).
    MorphEngineConfig cfg;
    cfg.policy = policy;
    cfg.base_address = base_address;
    cfg.verify_pure = false; // raw morph_real does not require pure verify
    cfg.allow_identity_fallback = true;
    MorphEngine eng(cfg);
    auto r = eng.morph(code, len);
    return r.bytes;
}

std::vector<uint8_t> morph_real_restricted(const uint8_t* code, size_t len, uint64_t base_address) {
    return morph_real(code, len, base_address, MorphPolicy::Safe);
}

void real_split_blocks(RealFunc& f, size_t max_insns) {
    if (f.blocks.empty() || max_insns < 3)
        return;
    std::vector<RealBlock> nb;
    std::vector<RealInsn> ni;
    ni.reserve(f.insns.size() + 8);
    std::vector<int> old_to_first(f.blocks.size(), -1);
    std::vector<int> old_to_last(f.blocks.size(), -1);
    for (size_t bi = 0; bi < f.blocks.size(); bi++) {
        const auto& b = f.blocks[bi];
        size_t i = b.start_idx;
        int first_id = -1;
        int last_id = -1;
        while (i < b.end_idx && i < f.insns.size()) {
            size_t remain = b.end_idx - i;
            size_t take = remain > max_insns ? max_insns : remain;
            RealBlock ob;
            ob.id = (int)nb.size();
            if (first_id < 0)
                first_id = ob.id;
            last_id = ob.id;
            ob.start_idx = ni.size();
            for (size_t k = 0; k < take && i < b.end_idx && i < f.insns.size(); k++, i++)
                ni.push_back(f.insns[i]);
            ob.end_idx = ni.size();
            ob.fallthrough = -1;
            ob.branch = -1;
            nb.push_back(ob);
        }
        old_to_first[bi] = first_id;
        old_to_last[bi] = last_id;
        for (int id = first_id; id >= 0 && id < last_id; id++)
            nb[(size_t)id].fallthrough = id + 1;
    }
    for (size_t bi = 0; bi < f.blocks.size(); bi++) {
        const auto& b = f.blocks[bi];
        int last = old_to_last[bi];
        if (last < 0)
            continue;
        if (b.fallthrough >= 0 && b.fallthrough < (int)old_to_first.size())
            nb[(size_t)last].fallthrough = old_to_first[(size_t)b.fallthrough];
        else
            nb[(size_t)last].fallthrough = -1;
        if (b.branch >= 0 && b.branch < (int)old_to_first.size())
            nb[(size_t)last].branch = old_to_first[(size_t)b.branch];
        else
            nb[(size_t)last].branch = -1;
    }
    f.insns.swap(ni);
    f.blocks.swap(nb);
    f.entry =
        (f.entry >= 0 && f.entry < (int)old_to_first.size() && old_to_first[(size_t)f.entry] >= 0)
            ? old_to_first[(size_t)f.entry]
            : 0;
    size_t off = 0;
    for (auto& in : f.insns) {
        in.offset = off;
        in.address = f.base_address + off;
        in.length = in.bytes.size();
        off += in.length;
    }
}

std::optional<uint32_t> interpret_real_pure(const RealFunc& f, uint64_t rdi, uint64_t rsi) {
    if (f.insns.empty() || f.blocks.empty())
        return std::nullopt;

    // Linear-ish interpreter following CFG block edges (not raw addresses).
    uint64_t rax = 0, rcx = 0;
    bool zf = false;
    uint64_t stack[64];
    int sp = 0;
    int bi = f.entry;
    if (bi < 0 || bi >= (int)f.blocks.size())
        return std::nullopt;

    for (int steps = 0; steps < 100000; steps++) {
        if (bi < 0 || bi >= (int)f.blocks.size())
            return std::nullopt;
        const RealBlock& b = f.blocks[(size_t)bi];
        for (size_t ii = b.start_idx; ii < b.end_idx && ii < f.insns.size(); ii++) {
            const RealInsn& in = f.insns[ii];
            const auto& by = in.bytes;
            if (by.empty())
                continue;

            // nop / xchg rax,rax / multi-byte nop
            if (by[0] == 0x90 || (by.size() >= 2 && by[0] == 0x66 && by[1] == 0x90) ||
                (by.size() >= 3 && by[0] == 0x0F && by[1] == 0x1F) ||
                (by.size() >= 3 && by[0] == 0x48 && by[1] == 0x87 && by[2] == 0xC0) ||
                (by.size() >= 2 && by[0] == 0x87 && by[1] == 0xC0) || in.text.find("nop") == 0) {
                continue;
            }
            // ret
            if (in.is_ret || by[0] == 0xC3)
                return (uint32_t)rax;
            // push/pop rax
            if (by[0] == 0x50) {
                if (sp >= 64)
                    return std::nullopt;
                stack[sp++] = rax;
                continue;
            }
            if (by[0] == 0x58) {
                if (sp <= 0)
                    return std::nullopt;
                rax = stack[--sp];
                continue;
            }
            // xor rax,rax / sub rax,rax / xor eax,eax
            if ((by.size() >= 3 && by[0] == 0x48 && by[1] == 0x31 && by[2] == 0xC0) ||
                (by.size() >= 3 && by[0] == 0x48 && by[1] == 0x29 && by[2] == 0xC0) ||
                (by.size() >= 2 && by[0] == 0x31 && by[1] == 0xC0)) {
                rax = 0;
                continue;
            }
            // mov eax, imm32 / mov rax, imm32
            if (by[0] == 0xB8 && by.size() >= 5) {
                uint32_t imm;
                std::memcpy(&imm, by.data() + 1, 4);
                rax = imm;
                continue;
            }
            if (by.size() >= 7 && by[0] == 0x48 && by[1] == 0xC7 && by[2] == 0xC0) {
                uint32_t imm;
                std::memcpy(&imm, by.data() + 3, 4);
                rax = imm;
                continue;
            }
            // mov eax, edi / mov eax, esi (SysV args)
            if (by.size() >= 2 && by[0] == 0x89 && by[1] == 0xF8) {
                rax = (uint32_t)rdi;
                continue;
            }
            if (by.size() >= 2 && by[0] == 0x89 && by[1] == 0xF0) {
                rax = (uint32_t)rsi;
                continue;
            }
            // add eax, edi / add eax, esi
            if (by.size() >= 2 && by[0] == 0x01 && by[1] == 0xF8) {
                rax = (uint32_t)(rax + rdi);
                continue;
            }
            if (by.size() >= 2 && by[0] == 0x01 && by[1] == 0xF0) {
                rax = (uint32_t)(rax + rsi);
                continue;
            }
            // inc/dec rax
            if (by.size() >= 3 && by[0] == 0x48 && by[1] == 0xFF && by[2] == 0xC0) {
                rax = (uint32_t)(rax + 1);
                continue;
            }
            if (by.size() >= 3 && by[0] == 0x48 && by[1] == 0xFF && by[2] == 0xC8) {
                rax = (uint32_t)(rax - 1);
                continue;
            }
            // mov ecx / xor ecx (state noise for pure seeds rarely)
            if (by[0] == 0xB9 && by.size() >= 5) {
                uint32_t imm;
                std::memcpy(&imm, by.data() + 1, 4);
                rcx = imm;
                continue;
            }
            if ((by.size() >= 3 && by[0] == 0x48 && by[1] == 0x31 && by[2] == 0xC9) ||
                (by.size() >= 2 && by[0] == 0x31 && by[1] == 0xC9)) {
                rcx = 0;
                continue;
            }
            // cmp rcx, imm
            if (by.size() >= 3 && by[0] == 0x83 && by[1] == 0xF9) {
                zf = ((uint32_t)rcx == (uint32_t)(int8_t)by[2]);
                continue;
            }
            if (by.size() >= 6 && by[0] == 0x81 && by[1] == 0xF9) {
                uint32_t imm;
                std::memcpy(&imm, by.data() + 2, 4);
                zf = ((uint32_t)rcx == imm);
                continue;
            }

            // Control transfer at end of block handled below via edges
            if (in.is_uncond_jump) {
                if (b.branch < 0)
                    return std::nullopt;
                bi = b.branch;
                goto next_block;
            }
            if (in.is_cond_jump) {
                if (zf) {
                    if (b.branch < 0)
                        return std::nullopt;
                    bi = b.branch;
                    goto next_block;
                }
                // not taken: continue in block or fallthrough after loop
                continue;
            }

            // lea rax,[rax] identity junk
            if (by.size() >= 3 && by[0] == 0x48 && by[1] == 0x8D && by[2] == 0x00)
                continue;
            if (by.size() >= 5 && by[0] == 0x48 && by[1] == 0x8D && by[2] == 0x64)
                continue;
            if (by.size() >= 2 && by[0] == 0x51 && by[1] == 0x59) // push rcx; pop rcx — need both
                continue;

            // Unsupported on this pure path
            return std::nullopt;
        }
        if (b.fallthrough >= 0) {
            bi = b.fallthrough;
            continue;
        }
        return std::nullopt;
    next_block:;
    }
    return std::nullopt;
}

} // namespace aether
