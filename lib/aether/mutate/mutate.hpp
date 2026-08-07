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
 * @file mutate.hpp
 * @brief In-memory XOR entropy mutation for payload buffers.
 *
 * Intentionally destructive to machine-code validity. Use only on demo
 * copies / infection payload heads — never on live assembled IR output
 * that must remain structurally coherent for the article demo.
 */

#include <cstddef>
#include <cstdint>

namespace aether {

/**
 * Overwrites every byte of [page, page+len) with page[i] XOR keystream[i].
 * Builds a keystream from random_device, pid, time (and RDTSC on x86_64),
 * then advances it with a simple LCG per byte.
 * Null page or zero len is a no-op. Result is not valid executable code.
 * @param page buffer start (may be null)
 * @param len  number of bytes to scramble
 */
void continuous_mutate(uint8_t* page, size_t len);

} // namespace aether
