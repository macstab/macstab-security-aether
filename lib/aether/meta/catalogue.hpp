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
 * @file catalogue.hpp
 * @brief Weighted semantic encoding catalogues and small x86 emit helpers.
 */

#include <cstdint>
#include <vector>

namespace aether {

/** One concrete machine encoding plus a selection weight for pick(). */
struct Form {
    std::vector<uint8_t> bytes; ///< raw x86-64 opcode bytes
    int weight;                 ///< relative probability weight (> 0)
};

/**
 * Chooses one Form from @p catalogue with probability proportional to weight.
 * Used to re-encode the same IR op as different equivalent machine code.
 * @param catalogue non-empty list of weighted encodings
 * @return const reference into @p catalogue
 */
const Form& pick(const std::vector<Form>& catalogue);

/**
 * Appends form.bytes to the end of @p out.
 * Used by the assembler when emitting catalogue-backed ops (NOP, CLEAR, JUNK).
 */
void emit_form(std::vector<uint8_t>& out, const Form& form);

/**
 * Appends a 32-bit little-endian integer to @p out (4 bytes).
 * Used for immediates and relative displacement placeholders.
 */
void emit_u32(std::vector<uint8_t>& out, uint32_t v);

/**
 * Appends the encoding of `mov ecx, imm32` (B9 + imm32 LE).
 * Zero-extends into rcx; used for SET_STATE / MOV_RCX_IMM.
 */
void emit_mov_rcx_imm(std::vector<uint8_t>& out, uint32_t imm);

/**
 * Appends the encoding of `mov eax, imm32` (B8 + imm32 LE).
 * Zero-extends into rax; used for MOV_RAX_IMM.
 */
void emit_mov_rax_imm(std::vector<uint8_t>& out, uint32_t imm);

/**
 * Returns the catalogue of encodings that clear RAX (xor/sub/mov/push-pop).
 * All forms are semantic equivalents of rax := 0.
 */
const std::vector<Form>& cat_clear_rax();

/**
 * Returns the catalogue of encodings that clear RCX (xor/sub/mov).
 * All forms are semantic equivalents of rcx := 0.
 */
const std::vector<Form>& cat_clear_rcx();

/**
 * Returns the catalogue of multi-byte NOP / identity encodings.
 * Used for Op::NOP expansion and trailing padding.
 */
const std::vector<Form>& cat_nop();

/**
 * Returns the catalogue of side-effect-free junk sequences
 * (nops, xchg rax,rax, push/pop pairs, lea rax,[rax]).
 * Used for Op::JUNK and Op::XCHG_RAX_RAX emission.
 */
const std::vector<Form>& cat_junk();

} // namespace aether
