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
 * @file decode_real.hpp
 * @brief Step 1: Zydis-backed x86-64 disassembly + CFG construction.
 */

#include "aether/meta/real_ir.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace aether {

/**
 * Disassemble a contiguous x86-64 code buffer with Zydis.
 * @param code          bytes to decode
 * @param len           length
 * @param base_address  virtual address of code[0] (for branch target resolution)
 * @param max_insns     safety cap (default 100000)
 * @return RealFunc with linear insns + basic blocks; empty on hard failure
 */
RealFunc disasm_real(const uint8_t* code,
                     size_t len,
                     uint64_t base_address = 0,
                     size_t max_insns = 100000);

/**
 * Build CFG blocks on an already-filled RealFunc::insns (recomputes blocks).
 */
void build_real_cfg(RealFunc& f);

/**
 * Pretty-print RealFunc to a string (for demos / tests).
 */
std::string format_real_func(const RealFunc& f, size_t max_insns_print = 64);

/** True if Zydis backend was compiled in (always true when this TU links). */
bool has_real_disasm();

} // namespace aether
