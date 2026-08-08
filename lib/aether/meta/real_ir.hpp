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
 * @file real_ir.hpp
 * @brief Real binary IR (Step 1): decoded x86-64 instructions + CFG blocks.
 *
 * Unlike educational `IRFunc`/`Op`, this preserves full instruction bytes and
 * metadata from a real disassembler (Zydis). Morph transforms on RealFunc
 * come later (Step 2–3); Step 1 kills the "toy seed only" limitation.
 */

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace aether {

/** GPR bitmask: bit0=RAX … bit7=RDI (SysV low 8). */
constexpr uint32_t kGprRax = 1u << 0;
constexpr uint32_t kGprRcx = 1u << 1;
constexpr uint32_t kGprRdx = 1u << 2;
constexpr uint32_t kGprRbx = 1u << 3;
constexpr uint32_t kGprRsp = 1u << 4;
constexpr uint32_t kGprRbp = 1u << 5;
constexpr uint32_t kGprRsi = 1u << 6;
constexpr uint32_t kGprRdi = 1u << 7;

/** One decoded machine instruction (ground truth from Zydis). */
struct RealInsn {
    uint64_t address = 0;       ///< runtime/VA or file-relative base + offset
    size_t offset = 0;          ///< offset into the owning buffer
    size_t length = 0;          ///< instruction size in bytes
    std::vector<uint8_t> bytes; ///< raw opcode bytes
    std::string text;           ///< Intel-syntax disassembly text

    bool is_branch = false; ///< any control-flow transfer
    bool is_call = false;
    bool is_ret = false;
    bool is_uncond_jump = false;    ///< jmp (not call)
    bool is_cond_jump = false;      ///< jcc
    bool has_imm_target = false;    ///< relative/absolute target resolved
    uint64_t branch_target = 0;     ///< absolute address when has_imm_target
    bool stops_fallthrough = false; ///< ret, jmp, ud2, etc.

    /** Effect model (filled at lift; used by industry analysis). */
    uint32_t gpr_read = 0;
    uint32_t gpr_write = 0;
    bool has_memory = false;
    bool has_rip_rel = false;
    bool writes_flags = false;
    bool reads_flags = false;
};

/** Basic block over RealInsn indices. */
struct RealBlock {
    int id = 0;
    size_t start_idx = 0; ///< first instruction index in RealFunc::insns
    size_t end_idx = 0;   ///< one-past-last instruction index
    int fallthrough = -1; ///< next block id if execution falls through
    int branch = -1;      ///< taken target block id for jcc/jmp when known
};

/** A decoded function or code region. */
struct RealFunc {
    uint64_t base_address = 0; ///< address of insns[0]
    std::vector<RealInsn> insns;
    std::vector<RealBlock> blocks;
    int entry = 0;

    size_t size_bytes() const {
        size_t n = 0;
        for (const auto& in : insns)
            n += in.length;
        return n;
    }
};

} // namespace aether
