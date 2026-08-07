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
 * @file real_func_extract.hpp
 * @brief Extract function-like regions from real x86-64 code for morph corpus.
 */

#include "aether/meta/real_ir.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// ExtractedFunc, extract_*, generate_pure_corpus

namespace aether {

struct ExtractedFunc {
    size_t offset = 0;     ///< offset into source buffer
    uint64_t vaddr = 0;    ///< virtual address of first byte
    std::vector<uint8_t> bytes;
    bool pure_interpretable = false; ///< interpret_real_pure succeeds on entry
    uint32_t pure_rax = 0;           ///< if pure_interpretable
    uint64_t arg_rdi = 0;            ///< SysV arg0 for multi-arg pure
    uint64_t arg_rsi = 0;            ///< SysV arg1
    std::string tag;                 ///< "elf" | "synthetic"
};

/**
 * Heuristic extract: linear scan for ret-terminated spans of [min_len, max_len].
 * Prefer starts after previous ret / buffer start. Caps at @p max_funcs.
 */
std::vector<ExtractedFunc> extract_real_functions(const uint8_t* code,
                                                  size_t len,
                                                  uint64_t base_vaddr,
                                                  size_t min_len = 3,
                                                  size_t max_len = 96,
                                                  size_t max_funcs = 512);

/**
 * Deterministic synthetic pure-function corpus (mov/xor/inc/jmp/ret family).
 * Generates @p count distinct pure functions for statistical break-rate gates.
 */
std::vector<ExtractedFunc> generate_pure_corpus(size_t count, uint64_t seed = 0xC0A90005ull);

/**
 * Load ELF .text and extract real functions; empty if path missing.
 */
std::vector<ExtractedFunc> extract_from_elf(const std::string& path, size_t max_funcs = 512);

/**
 * Extract from multiple ELF paths (skips missing). Caps total at max_funcs.
 */
std::vector<ExtractedFunc> extract_from_elfs(const std::vector<std::string>& paths,
                                             size_t max_funcs = 2048);

/** Default shipped corpus paths (repo-relative). */
std::vector<std::string> default_corpus_elf_paths();

} // namespace aether
