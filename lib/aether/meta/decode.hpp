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
 * @file decode.hpp
 * @brief Educational x86-64 decode → IR + CFG construction.
 *
 * Unrecognized bytes become Op::RAW (preserved). Not a full disassembler.
 */

#include "aether/meta/ir.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace aether {

/**
 * Turns a raw machine-code buffer into an IR function with basic blocks.
 * Steps:
 *  1) Linearly decode bytes into IRInsn (known ops + RAW for unknown).
 *  2) Coalesce adjacent RAW bytes into single instructions.
 *  3) Split into basic blocks at leaders (entry, branch targets, after ret/jmp).
 *  4) Resolve JMP/JE absolute offsets to block ids; set fallthrough edges.
 * @param code pointer to x86-64 bytes
 * @param len  byte length
 * @return IRFunc with entry = 0 and dense block ids
 */
IRFunc disasm(const uint8_t* code, size_t len);

/**
 * Keep only blocks reachable from entry; renumber ids; re-assemble.
 * Strips trailing NOP sleds / dead code so multi-stage re-lift stays pure.
 * @return empty if disasm/assemble fails or no reachable RET path
 */
std::vector<uint8_t> normalize_reachable(const uint8_t* code, size_t len);
inline std::vector<uint8_t> normalize_reachable(const std::vector<uint8_t>& code) {
    return normalize_reachable(code.data(), code.size());
}

} // namespace aether
