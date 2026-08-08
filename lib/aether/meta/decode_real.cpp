/**
 * SPDX-License-Identifier: LicenseRef-Aether-Research
 * Copyright (c) 2024–2026 Macstab GmbH.
 *
 * Licensed under the Aether Research License in the repository root file
 * LICENSE. Authorized lab / lawful private use only as defined there.
 * By using this file you accept LICENSE. See docs/LICENSE_ACCEPTANCE.md.
 */

#include "aether/meta/decode_real.hpp"

#include <Zydis/Zydis.h>

#include <cstdio>
#include <map>
#include <sstream>

namespace aether {
namespace {

uint32_t zydis_reg_mask(ZydisRegister reg) {
    switch (reg) {
    case ZYDIS_REGISTER_AL:
    case ZYDIS_REGISTER_AH:
    case ZYDIS_REGISTER_AX:
    case ZYDIS_REGISTER_EAX:
    case ZYDIS_REGISTER_RAX:
        return kGprRax;
    case ZYDIS_REGISTER_CL:
    case ZYDIS_REGISTER_CH:
    case ZYDIS_REGISTER_CX:
    case ZYDIS_REGISTER_ECX:
    case ZYDIS_REGISTER_RCX:
        return kGprRcx;
    case ZYDIS_REGISTER_DL:
    case ZYDIS_REGISTER_DH:
    case ZYDIS_REGISTER_DX:
    case ZYDIS_REGISTER_EDX:
    case ZYDIS_REGISTER_RDX:
        return kGprRdx;
    case ZYDIS_REGISTER_BL:
    case ZYDIS_REGISTER_BH:
    case ZYDIS_REGISTER_BX:
    case ZYDIS_REGISTER_EBX:
    case ZYDIS_REGISTER_RBX:
        return kGprRbx;
    case ZYDIS_REGISTER_SPL:
    case ZYDIS_REGISTER_SP:
    case ZYDIS_REGISTER_ESP:
    case ZYDIS_REGISTER_RSP:
        return kGprRsp;
    case ZYDIS_REGISTER_BPL:
    case ZYDIS_REGISTER_BP:
    case ZYDIS_REGISTER_EBP:
    case ZYDIS_REGISTER_RBP:
        return kGprRbp;
    case ZYDIS_REGISTER_SIL:
    case ZYDIS_REGISTER_SI:
    case ZYDIS_REGISTER_ESI:
    case ZYDIS_REGISTER_RSI:
        return kGprRsi;
    case ZYDIS_REGISTER_DIL:
    case ZYDIS_REGISTER_DI:
    case ZYDIS_REGISTER_EDI:
    case ZYDIS_REGISTER_RDI:
        return kGprRdi;
    default:
        return 0;
    }
}

bool is_terminator(const ZydisDecodedInstruction& ins) {
    switch (ins.meta.category) {
    case ZYDIS_CATEGORY_RET:
    case ZYDIS_CATEGORY_UNCOND_BR:
    case ZYDIS_CATEGORY_INTERRUPT:
        return true;
    default:
        break;
    }
    // ud2 / hlt style
    if (ins.mnemonic == ZYDIS_MNEMONIC_UD2 || ins.mnemonic == ZYDIS_MNEMONIC_HLT ||
        ins.mnemonic == ZYDIS_MNEMONIC_INT3)
        return true;
    return false;
}

} // namespace

bool has_real_disasm() {
    return true;
}

RealFunc disasm_real(const uint8_t* code, size_t len, uint64_t base_address, size_t max_insns) {
    RealFunc f;
    f.base_address = base_address;
    if (!code || !len)
        return f;

    ZydisDecoder decoder;
    if (!ZYAN_SUCCESS(ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64)))
        return f;

    ZydisFormatter formatter;
    ZydisFormatterInit(&formatter, ZYDIS_FORMATTER_STYLE_INTEL);

    size_t offset = 0;
    while (offset < len && f.insns.size() < max_insns) {
        ZydisDecodedInstruction instruction;
        ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];
        ZyanStatus st =
            ZydisDecoderDecodeFull(&decoder, code + offset, len - offset, &instruction, operands);
        if (!ZYAN_SUCCESS(st)) {
            // Invalid byte: record as 1-byte unknown and continue (robust lift).
            RealInsn bad;
            bad.address = base_address + offset;
            bad.offset = offset;
            bad.length = 1;
            bad.bytes.push_back(code[offset]);
            bad.text = "db 0x" + [&]() {
                char b[8];
                std::snprintf(b, sizeof(b), "%02x", code[offset]);
                return std::string(b);
            }();
            bad.stops_fallthrough = false;
            f.insns.push_back(std::move(bad));
            offset += 1;
            continue;
        }

        RealInsn in;
        in.address = base_address + offset;
        in.offset = offset;
        in.length = instruction.length;
        in.bytes.assign(code + offset, code + offset + instruction.length);

        char buffer[256];
        ZydisFormatterFormatInstruction(&formatter,
                                        &instruction,
                                        operands,
                                        instruction.operand_count_visible,
                                        buffer,
                                        sizeof(buffer),
                                        in.address,
                                        ZYAN_NULL);
        in.text = buffer;

        in.is_ret = (instruction.meta.category == ZYDIS_CATEGORY_RET);
        in.is_call = (instruction.meta.category == ZYDIS_CATEGORY_CALL);
        in.is_uncond_jump = (instruction.meta.category == ZYDIS_CATEGORY_UNCOND_BR);
        in.is_cond_jump = (instruction.meta.category == ZYDIS_CATEGORY_COND_BR);
        in.is_branch = in.is_ret || in.is_call || in.is_uncond_jump || in.is_cond_jump;
        in.stops_fallthrough = is_terminator(instruction) && !in.is_call;

        // Effect model (industry Phase I–II)
        if (instruction.cpu_flags) {
            in.reads_flags = (instruction.cpu_flags->tested != 0);
            in.writes_flags =
                (instruction.cpu_flags->modified != 0) || (instruction.cpu_flags->set_0 != 0) ||
                (instruction.cpu_flags->set_1 != 0) || (instruction.cpu_flags->undefined != 0);
        }
        for (ZyanU8 i = 0; i < instruction.operand_count; i++) {
            const auto& op = operands[i];
            if (op.type == ZYDIS_OPERAND_TYPE_REGISTER) {
                uint32_t m = zydis_reg_mask(op.reg.value);
                if (op.actions & ZYDIS_OPERAND_ACTION_MASK_READ)
                    in.gpr_read |= m;
                if (op.actions & ZYDIS_OPERAND_ACTION_MASK_WRITE)
                    in.gpr_write |= m;
            } else if (op.type == ZYDIS_OPERAND_TYPE_MEMORY) {
                in.has_memory = true;
                if (op.mem.base == ZYDIS_REGISTER_RIP || op.mem.index == ZYDIS_REGISTER_RIP)
                    in.has_rip_rel = true;
                uint32_t bm = zydis_reg_mask(op.mem.base);
                uint32_t im = zydis_reg_mask(op.mem.index);
                in.gpr_read |= bm | im;
            } else if (op.type == ZYDIS_OPERAND_TYPE_IMMEDIATE && op.imm.is_relative) {
                uint64_t target = 0;
                if (ZYAN_SUCCESS(
                        ZydisCalcAbsoluteAddress(&instruction, &op, in.address, &target))) {
                    in.has_imm_target = true;
                    in.branch_target = target;
                }
            } else if (op.type == ZYDIS_OPERAND_TYPE_IMMEDIATE && op.imm.is_signed == ZYAN_FALSE &&
                       (in.is_uncond_jump || in.is_cond_jump || in.is_call)) {
                in.has_imm_target = true;
                in.branch_target = (uint64_t)op.imm.value.u;
            }
        }

        offset += instruction.length;
        f.insns.push_back(std::move(in));

        // Optional early stop: if we hit a ret and user only wanted one function —
        // for Step 1 we keep decoding the whole buffer (caller slices .text).
    }

    build_real_cfg(f);
    return f;
}

void build_real_cfg(RealFunc& f) {
    f.blocks.clear();
    if (f.insns.empty())
        return;

    const size_t n = f.insns.size();
    std::vector<char> leader(n, 0);
    leader[0] = 1;

    std::map<uint64_t, size_t> addr_to_idx;
    for (size_t i = 0; i < n; i++)
        addr_to_idx[f.insns[i].address] = i;

    for (size_t i = 0; i < n; i++) {
        const auto& in = f.insns[i];
        if (in.is_branch && i + 1 < n)
            leader[i + 1] = 1;
        if ((in.is_uncond_jump || in.is_cond_jump || in.is_call) && in.has_imm_target) {
            auto it = addr_to_idx.find(in.branch_target);
            if (it != addr_to_idx.end())
                leader[it->second] = 1;
        }
        if (in.stops_fallthrough && i + 1 < n)
            leader[i + 1] = 1;
    }

    // Build blocks as contiguous leader..next_leader
    std::vector<int> insn_block(n, -1);
    int bid = 0;
    for (size_t i = 0; i < n;) {
        if (!leader[i] && i != 0) {
            i++;
            continue;
        }
        size_t start = i;
        i++;
        while (i < n && !leader[i])
            i++;
        RealBlock b;
        b.id = bid;
        b.start_idx = start;
        b.end_idx = i;
        for (size_t j = start; j < i; j++)
            insn_block[j] = bid;
        f.blocks.push_back(b);
        bid++;
    }

    // Edges
    for (auto& b : f.blocks) {
        if (b.start_idx >= b.end_idx)
            continue;
        const RealInsn& last = f.insns[b.end_idx - 1];
        if (last.is_ret) {
            b.fallthrough = -1;
            b.branch = -1;
        } else if (last.is_uncond_jump && last.has_imm_target) {
            auto it = addr_to_idx.find(last.branch_target);
            b.branch = (it != addr_to_idx.end()) ? insn_block[it->second] : -1;
            b.fallthrough = -1;
        } else if (last.is_cond_jump && last.has_imm_target) {
            auto it = addr_to_idx.find(last.branch_target);
            b.branch = (it != addr_to_idx.end()) ? insn_block[it->second] : -1;
            // fallthrough = next sequential block if any
            if (b.end_idx < n)
                b.fallthrough = insn_block[b.end_idx];
            else
                b.fallthrough = -1;
        } else if (last.stops_fallthrough) {
            b.fallthrough = -1;
            b.branch = -1;
        } else {
            if (b.end_idx < n)
                b.fallthrough = insn_block[b.end_idx];
            else
                b.fallthrough = -1;
            b.branch = -1;
        }
    }

    f.entry = 0;
}

std::string format_real_func(const RealFunc& f, size_t max_insns_print) {
    std::ostringstream os;
    os << "RealFunc base=0x" << std::hex << f.base_address << std::dec
       << " insns=" << f.insns.size() << " blocks=" << f.blocks.size()
       << " bytes=" << f.size_bytes() << "\n";
    size_t shown = 0;
    for (const auto& b : f.blocks) {
        os << "  block " << b.id << "  [" << b.start_idx << "," << b.end_idx << ")"
           << "  fall=" << b.fallthrough << "  br=" << b.branch << "\n";
        for (size_t i = b.start_idx; i < b.end_idx && shown < max_insns_print; i++, shown++) {
            const auto& in = f.insns[i];
            os << "    0x" << std::hex << in.address << std::dec << "  +" << in.offset << "  "
               << in.text;
            if (in.has_imm_target)
                os << "  -> 0x" << std::hex << in.branch_target << std::dec;
            os << "\n";
        }
        if (shown >= max_insns_print && b.end_idx > b.start_idx) {
            os << "    ... truncated ...\n";
            break;
        }
    }
    return os.str();
}

} // namespace aether
