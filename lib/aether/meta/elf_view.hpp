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
 * @file elf_view.hpp
 * @brief Minimal ELF64 reader: load file, find executable segment / .text.
 *
 * Host-endian independent enough for little-endian ELF64 (Linux x86-64).
 * Used by Step 1 to feed Zydis real .text bytes.
 */

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace aether {

struct ElfTextRegion {
    bool ok = false;
    std::string error;
    std::vector<uint8_t> file_bytes; ///< full file (owned)
    size_t file_offset = 0;          ///< offset of region in file
    size_t size = 0;                 ///< region size
    uint64_t vaddr = 0;              ///< virtual address of region start
    std::string name;                ///< ".text" or "PT_LOAD+X"
};

/**
 * Load an ELF64 path and extract the best executable code region:
 * prefer section named .text; else first PF_X PT_LOAD.
 */
ElfTextRegion load_elf64_text(const std::string& path);

/**
 * Pointer to region bytes inside file_bytes (valid while region lives).
 */
inline const uint8_t* elf_text_data(const ElfTextRegion& r) {
    if (!r.ok || r.file_offset + r.size > r.file_bytes.size())
        return nullptr;
    return r.file_bytes.data() + r.file_offset;
}

} // namespace aether
