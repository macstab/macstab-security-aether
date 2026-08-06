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
 * @file infect.hpp
 * @brief Controlled single-file ELF append infection (research only).
 *
 * Only the path explicitly named by the user is modified. No scanning,
 * no spreading, no network. Append-only; does not rewrite e_entry/PHDRs.
 */

#include <cstdint>
#include <vector>

namespace aether {

/**
 * Appends @p payload to the end of a Linux ELF file named by @p path.
 * Steps:
 *  1) open path O_RDWR
 *  2) grow file by payload.size()+48 with ftruncate
 *  3) mmap shared, verify ELF magic
 *  4) bump e_ident[EI_PAD] as a changing generation mark
 *  5) copy payload after old EOF
 *  6) continuous_mutate the first min(64, payload) bytes in place
 *  7) msync, unmap, close; print size before/after
 * Does not change e_entry or program headers (payload is not auto-executed).
 * @param path    absolute/relative path to one ELF file
 * @param payload bytes to append
 * @return 0 on success, non-zero on open/map/magic failure
 */
int infect(const char* path, const std::vector<uint8_t>& payload);

} // namespace aether
