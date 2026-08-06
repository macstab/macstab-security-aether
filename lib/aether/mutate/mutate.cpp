/**
 * SPDX-License-Identifier: LicenseRef-Aether-Research
 * Copyright (c) 2024–2026 Aether Research Project contributors.
 *
 * Licensed under the Aether Research License in the repository root file
 * LICENSE. Authorized lab / lawful private use only as defined there.
 * By using this file you accept LICENSE. See docs/LICENSE_ACCEPTANCE.md.
 */

#include "aether/mutate/mutate.hpp"

#include <ctime>
#include <random>
#include <unistd.h>

namespace aether {

/**
 * Overwrites every byte of [page, page+len) with page[i] XOR keystream[i].
 * Builds a keystream from random_device, pid, time (and RDTSC on x86_64),
 * then advances it with a simple LCG per byte.
 * Null page or zero len is a no-op. Result is not valid executable code.
 */
void continuous_mutate(uint8_t* page, size_t len) {
    if (!page || !len)
        return;

    uint64_t ent = (uint64_t)std::random_device{}();
    ent ^= (uint64_t)getpid() << 13;
    ent ^= (uint64_t)time(nullptr) << 7;
#if defined(__x86_64__) || defined(_M_X64)
    uint32_t lo, hi;
    asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
    ent ^= ((uint64_t)hi << 32) | lo;
#endif
    for (size_t i = 0; i < len; i++) {
        page[i] ^= (uint8_t)(ent + i * 17);
        ent = ent * 6364136223846793005ULL + 1;
    }
}

} // namespace aether
