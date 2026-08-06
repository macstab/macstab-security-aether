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
 * @file pe_view.hpp
 * @brief PE32+ AMD64: .text extract + validate + section helpers for rewrite quality.
 */

#include <cstdint>
#include <string>
#include <vector>

namespace aether {

struct PeTextRegion {
    bool ok = false;
    std::string error;
    std::vector<uint8_t> file_bytes;
    size_t file_offset = 0;
    size_t size = 0;
    uint64_t vaddr = 0;
    std::string name;
    // Extra for commercial-grade rewrite bookkeeping
    size_t e_lfanew = 0;
    size_t section_header_off = 0; ///< file offset of this section header
    uint32_t section_index = 0;
    uint32_t file_align = 0x200;
    uint32_t sect_align = 0x1000;
    uint64_t image_base = 0;
    uint32_t characteristics = 0;
};

/** Load PE32+ and extract .text (or first executable section). */
PeTextRegion load_pe64_text(const std::string& path);

/** Validate PE32+ AMD64 structure (headers, section bounds). */
bool pe64_validate(const uint8_t* data, size_t len, std::string* err = nullptr);

/**
 * Recompute optional PE checksum (IMAGE_OPTIONAL_HEADER.CheckSum).
 * Windows loader often ignores for EXEs; still good for rewrite hygiene.
 */
void pe64_fix_checksum(std::vector<uint8_t>& file);

/** Align n up to a (a power of two or any positive). */
inline size_t pe_align_up(size_t n, size_t a) {
    if (a == 0)
        return n;
    return (n + a - 1) / a * a;
}

inline const uint8_t* pe_text_data(const PeTextRegion& r) {
    if (!r.ok || r.file_offset + r.size > r.file_bytes.size())
        return nullptr;
    return r.file_bytes.data() + r.file_offset;
}

} // namespace aether
