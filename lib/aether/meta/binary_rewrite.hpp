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
 * @file binary_rewrite.hpp
 * @brief Closed-industry binary rewrite: function morph, size-fit, trampoline grow.
 *
 * - Prefer in-place size_fit morph (no layout break)
 * - If morph grows: trampoline (E9) at original site → body in appended RX region
 * - ELF64: extend file with page-aligned RX payload; PE32+: append RX blob + fix section
 * - RIP-relative within function bodies re-fixed by assemble_real
 */

#include "aether/meta/morph_engine.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace aether {

enum class BinaryFormat : int {
    Unknown = 0,
    Elf64 = 1,
    Pe32Plus = 2,
    Raw = 3,
};

struct BinaryRewriteResult {
    bool ok = false;
    BinaryFormat format = BinaryFormat::Unknown;
    std::string error;
    std::vector<uint8_t> file_bytes;
    size_t region_offset = 0;
    size_t region_size = 0;
    size_t morphed_size = 0;
    size_t padded_to = 0;
    size_t funcs_seen = 0;
    size_t funcs_morphed = 0;
    size_t funcs_identity = 0;
    size_t funcs_skipped = 0;
    size_t funcs_trampolined = 0; ///< grew via trampoline + section expand
    size_t grow_bytes = 0;
    bool pure_verified = false;
    bool used_function_level = false;
    bool section_grew = false;
    std::string region_name;
};

BinaryFormat detect_binary_format(const uint8_t* data, size_t len);

BinaryRewriteResult
rewrite_binary_buffer(const uint8_t* data, size_t len, const MorphEngineConfig& cfg);

BinaryRewriteResult rewrite_binary_file(const std::string& in_path,
                                        const std::string& out_path,
                                        const MorphEngineConfig& cfg);

BinaryRewriteResult rewrite_functions_in_region(std::vector<uint8_t> file,
                                                size_t region_off,
                                                size_t region_size,
                                                uint64_t region_vaddr,
                                                BinaryFormat fmt,
                                                const std::string& name,
                                                const MorphEngineConfig& cfg);

bool industry_rewrite_selftest();

} // namespace aether
