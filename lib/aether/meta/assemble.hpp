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
 * @file assemble.hpp
 * @brief Two-pass IR → machine-code emit with relative jump fixups.
 */

#include "aether/meta/ir.hpp"

#include <cstdint>
#include <vector>

namespace aether {

/**
 * Lowers an entire IR function to an x86-64 byte buffer.
 * Pass 1: emit entry block first, then remaining blocks in id order; for each
 * instruction pick catalogue encodings or fixed opcodes; record fixups for
 * JMP (E9 rel32) and JE (0F 84 rel32).
 * Pass 2: patch every fixup so the relative displacement points at the
 * start of the target block.
 * @param f IR function (expects dense block ids matching vector indices)
 * @return assembled machine-code bytes (structurally valid for supported ops)
 */
std::vector<uint8_t> assemble(const IRFunc& f);

} // namespace aether
