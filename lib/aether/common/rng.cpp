/**
 * SPDX-License-Identifier: LicenseRef-Aether-Research
 * Copyright (c) 2024–2026 Macstab GmbH.
 *
 * Licensed under the Aether Research License in the repository root file
 * LICENSE. Authorized lab / lawful private use only as defined there.
 * By using this file you accept LICENSE. See docs/LICENSE_ACCEPTANCE.md.
 */

#include "aether/common/rng.hpp"

#include <ctime>
#include <unistd.h>

namespace aether {
namespace {

std::mt19937_64 g_rng;
uint64_t g_stream_counter = 0;

} // namespace

std::mt19937_64& rng() {
    return g_rng;
}

void seed_rng() {
    const uint64_t a = (uint64_t)std::random_device{}();
    const uint64_t b = (uint64_t)std::random_device{}();
    const uint64_t t = (uint64_t)time(nullptr);
    const uint64_t p = (uint64_t)getpid();
    g_rng.seed(a ^ (b << 1) ^ (t << 17) ^ (p << 32) ^ (++g_stream_counter * 0x9E3779B97F4A7C15ULL));
}

void seed_rng_u64(uint64_t seed) {
    // Distinct streams even if callers pass the same base seed twice.
    g_rng.seed(seed ^ (++g_stream_counter * 0x9E3779B97F4A7C15ULL));
}

int rnd(int lo, int hi) {
    std::uniform_int_distribution<int> d(lo, hi);
    return d(g_rng);
}

uint64_t hash64(const uint8_t* data, size_t len) {
    uint64_t h = 14695981039346656037ULL; // FNV-1a offset basis
    if (!data && len)
        return h;
    for (size_t i = 0; i < len; i++) {
        h ^= data[i];
        h *= 1099511628211ULL;
    }
    return h;
}

} // namespace aether
