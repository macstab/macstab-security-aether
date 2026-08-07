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
 * @file x64_sim.hpp
 * @brief Host-independent x86-64 pure/stack simulator for industry multi-input oracle.
 *
 * Runs on any host (including Apple Silicon). Covers reg + stack-frame domain used
 * by Industry morph. Hardware native (try_exec) is additional when __x86_64__.
 */

#include "aether/meta/real_ir.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace aether {

struct X64SimState {
    uint64_t rax = 0, rcx = 0, rdx = 0, rbx = 0;
    uint64_t rsp = 0, rbp = 0, rsi = 0, rdi = 0;
    bool zf = false, sf = false, cf = false, of = false;
    bool ok = true;
};

/**
 * Simulate RealFunc pure/stack domain. Stack is a local 4KB scratch.
 * @return RAX low 32 on ret, nullopt if unsupported insn / fault.
 */
std::optional<uint32_t> sim_real_func(const RealFunc& f, uint64_t rdi = 0, uint64_t rsi = 0);

/** Multi-input differential: in vs out must match on all interpretable pairs. */
bool sim_multi_input_equiv(const RealFunc& a, const RealFunc& b, size_t* checked = nullptr);

/** Decode + simulate raw buffer. */
std::optional<uint32_t> sim_x64_buffer(const uint8_t* code, size_t len, uint64_t rdi, uint64_t rsi,
                                       uint64_t base = 0x1000);

} // namespace aether
