/**
 * SPDX-License-Identifier: LicenseRef-Aether-Research
 * Copyright (c) 2024–2026 Aether Research Project contributors.
 *
 * Licensed under the Aether Research License in the repository root file
 * LICENSE. Authorized lab / lawful private use only as defined there.
 * By using this file you accept LICENSE. See docs/LICENSE_ACCEPTANCE.md.
 */

#include "aether/meta/assemble.hpp"

#include "aether/meta/catalogue.hpp"

#include <cstring>

namespace aether {
namespace {

enum class FixKind { REL8, REL32 };

struct Fixup {
    size_t at;     ///< offset of relative displacement field
    int target_id; ///< destination block id
    FixKind kind;
    size_t instr_end; ///< end of instruction (rel base)
};

/**
 * Writes the relative displacement for one jump fixup into @p code.
 * Computes target_block_offset - instr_end and stores it as rel8 or rel32.
 * @return false if target id is out of range or rel8 does not fit in one byte
 */
bool patch_rel(std::vector<uint8_t>& code, const Fixup& fx, const std::vector<size_t>& block_off) {
    if (fx.target_id < 0 || fx.target_id >= (int)block_off.size())
        return false;
    int64_t rel = (int64_t)block_off[fx.target_id] - (int64_t)fx.instr_end;
    if (fx.kind == FixKind::REL8) {
        if (rel < -128 || rel > 127)
            return false;
        code[fx.at] = (uint8_t)(int8_t)rel;
    } else {
        int32_t r32 = (int32_t)rel;
        memcpy(code.data() + fx.at, &r32, 4);
    }
    return true;
}

/**
 * Lowers a single IR instruction to machine bytes.
 * Catalogue-backed ops (NOP/JUNK/CLEAR) call pick(); JMP/JE emit E9/0F84 with
 * a zero displacement and push a Fixup so pass 2 can patch the target.
 * @param in       IR instruction
 * @param fixups   out-list of jump fixups to patch later
 * @param base_off current output offset of this instruction's first byte
 * @return opcode bytes for this instruction only
 */
std::vector<uint8_t> emit_insn(const IRInsn& in, std::vector<Fixup>& fixups, size_t base_off) {
    std::vector<uint8_t> out;
    switch (in.op) {
    case Op::NOP:
        emit_form(out, pick(cat_nop()));
        break;
    case Op::JUNK:
    case Op::XCHG_RAX_RAX:
        emit_form(out, pick(cat_junk()));
        break;
    case Op::CLEAR_RAX:
        emit_form(out, pick(cat_clear_rax()));
        break;
    case Op::CLEAR_RCX:
        emit_form(out, pick(cat_clear_rcx()));
        break;
    case Op::MOV_RAX_IMM:
        emit_mov_rax_imm(out, (uint32_t)in.imm);
        break;
    case Op::MOV_RCX_IMM:
    case Op::SET_STATE:
        emit_mov_rcx_imm(out, (uint32_t)in.imm);
        break;
    case Op::PUSH_RAX:
        out.push_back(0x50);
        break;
    case Op::POP_RAX:
        out.push_back(0x58);
        break;
    case Op::INC_RAX:
        out.insert(out.end(), {0x48, 0xFF, 0xC0});
        break;
    case Op::DEC_RAX:
        out.insert(out.end(), {0x48, 0xFF, 0xC8});
        break;
    case Op::CMP_STATE:
        if (in.imm >= -128 && in.imm <= 127) {
            out.push_back(0x83);
            out.push_back(0xF9);
            out.push_back((uint8_t)(int8_t)in.imm);
        } else {
            out.push_back(0x81);
            out.push_back(0xF9);
            emit_u32(out, (uint32_t)in.imm);
        }
        break;
    case Op::JMP: {
        out.push_back(0xE9);
        size_t at = base_off + out.size();
        emit_u32(out, 0);
        fixups.push_back(Fixup{at, in.target, FixKind::REL32, base_off + out.size()});
        break;
    }
    case Op::JE: {
        out.push_back(0x0F);
        out.push_back(0x84);
        size_t at = base_off + out.size();
        emit_u32(out, 0);
        fixups.push_back(Fixup{at, in.target, FixKind::REL32, base_off + out.size()});
        break;
    }
    case Op::RET:
        out.push_back(0xC3);
        break;
    case Op::RAW:
        out.insert(out.end(), in.raw.begin(), in.raw.end());
        break;
    }
    return out;
}

} // namespace

namespace {
/** True if @p op ends a basic block (no implicit fallthrough into next bytes). */
bool is_block_terminator(Op op) {
    return op == Op::JMP || op == Op::JE || op == Op::RET;
}
} // namespace

/**
 * Lowers the whole IR function to x86-64: emit entry first, then other blocks,
 * patch JMP/JE, and **seal fallthrough** with explicit E9 when the next
 * physical block is not the logical fallthrough (required for multi-stage
 * re-lift after flatten / permute).
 */
std::vector<uint8_t> assemble(const IRFunc& f) {
    std::vector<int> order;
    order.reserve(f.blocks.size());
    if (f.entry >= 0 && f.entry < (int)f.blocks.size())
        order.push_back(f.entry);
    for (size_t i = 0; i < f.blocks.size(); i++) {
        if ((int)i != f.entry)
            order.push_back((int)i);
    }

    std::vector<uint8_t> code;
    std::vector<Fixup> fixups;
    std::vector<size_t> block_off(f.blocks.size(), 0);

    for (size_t oi = 0; oi < order.size(); oi++) {
        const int id = order[oi];
        if (id < 0 || id >= (int)f.blocks.size())
            continue;
        const IRBlock& b = f.blocks[(size_t)id];
        block_off[(size_t)id] = code.size();
        for (const auto& in : b.insns) {
            auto bytes = emit_insn(in, fixups, code.size());
            code.insert(code.end(), bytes.begin(), bytes.end());
        }

        // Seal fallthrough / JE-not-taken when layout ≠ CFG order.
        Op last = Op::NOP;
        if (!b.insns.empty())
            last = b.insns.back().op;

        const int next_phys = (oi + 1 < order.size()) ? order[oi + 1] : -1;

        if (last == Op::JE) {
            // Not-taken path must land on fallthrough.
            if (b.fallthrough >= 0 && b.fallthrough != next_phys) {
                IRInsn j{Op::JMP, 0, b.fallthrough, {}};
                auto bytes = emit_insn(j, fixups, code.size());
                code.insert(code.end(), bytes.begin(), bytes.end());
            }
        } else if (!is_block_terminator(last)) {
            if (b.fallthrough >= 0 && b.fallthrough != next_phys) {
                IRInsn j{Op::JMP, 0, b.fallthrough, {}};
                auto bytes = emit_insn(j, fixups, code.size());
                code.insert(code.end(), bytes.begin(), bytes.end());
            }
        }
    }

    for (const auto& fx : fixups) {
        if (!patch_rel(code, fx, block_off))
            return {}; // refuse silently-wrong relocs (native would diverge)
    }

    return code;
}

} // namespace aether
