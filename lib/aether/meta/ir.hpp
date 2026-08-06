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
 * @file ir.hpp
 * @brief Intermediate representation for the educational metamorphic engine.
 *
 * CFG model: function → blocks → instructions. Jumps use block ids (not
 * raw offsets) after build_cfg / disasm resolve targets.
 */

#include <cstdint>
#include <vector>

namespace aether {

/**
 * Semantic opcode for one IR instruction.
 * The catalogue/assembler map each Op to one or more concrete x86 encodings.
 */
enum class Op : uint8_t {
    NOP,          ///< multi-form nop (no architectural effect)
    CLEAR_RAX,    ///< set rax to 0
    CLEAR_RCX,    ///< set rcx to 0
    MOV_RAX_IMM,  ///< set rax to imm32 (zero-extended)
    MOV_RCX_IMM,  ///< set rcx to imm32 (zero-extended)
    PUSH_RAX,     ///< push rax onto the stack
    POP_RAX,      ///< pop into rax from the stack
    XCHG_RAX_RAX, ///< identity xchg; treated as nop-class filler
    INC_RAX,      ///< rax = rax + 1
    DEC_RAX,      ///< rax = rax - 1
    JMP,          ///< unconditional jump to target block
    JE,           ///< jump if ZF=1 to target block (educational)
    RET,          ///< return from function
    JUNK,         ///< side-effect-free filler from the junk catalogue
    RAW,          ///< opaque original bytes kept verbatim
    SET_STATE,    ///< rcx = imm (flattener state write)
    CMP_STATE,    ///< cmp rcx, imm (dispatcher state test)
};

/**
 * One intermediate-representation instruction.
 * For JMP/JE after CFG build, @c target holds the destination block id.
 * During linear decode only, JMP/JE may store an absolute byte offset in @c imm.
 */
struct IRInsn {
    Op op = Op::NOP;          ///< what this instruction means
    int32_t imm = 0;          ///< immediate, state id, or (pre-CFG) abs offset
    int target = -1;          ///< destination block id for JMP / JE (-1 if none)
    std::vector<uint8_t> raw; ///< original bytes when op == RAW
};

/**
 * One basic block: a straight-line sequence of IR instructions.
 * @c fallthrough is the next block if execution falls off the end without
 * taking a JMP/RET (and is the not-taken path for a terminal JE).
 */
struct IRBlock {
    int id = 0;                ///< dense block id (usually vector index)
    std::vector<IRInsn> insns; ///< instructions in program order
    int fallthrough = -1;      ///< successor block id, or -1 if none
};

/**
 * One function as a control-flow graph of basic blocks.
 * @c entry is the block id where execution starts.
 */
struct IRFunc {
    std::vector<IRBlock> blocks; ///< all blocks (prefer dense ids)
    int entry = 0;               ///< starting block id
};

} // namespace aether
