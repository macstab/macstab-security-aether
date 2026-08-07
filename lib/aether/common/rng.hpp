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
 * @file rng.hpp
 * @brief Shared pseudo-random utilities for the research engine.
 *
 * All metamorphic choices (catalogue pick, block shuffle, noise order)
 * go through this module so a single seed drives one demo run.
 */

#include <cstdint>
#include <random>
#include <vector>

namespace aether {

/**
 * Returns a reference to the process-wide Mersenne Twister generator.
 * Used by shuffle() and other callers that need the raw engine object.
 */
std::mt19937_64& rng();

/**
 * Initializes the global generator once at process start.
 * Mixes OS entropy, current time, and process id into the seed so each
 * run produces a different mutation sequence.
 * Call before any rnd() or shuffle that uses rng().
 */
void seed_rng();

/**
 * Reseed from an explicit 64-bit value (tests / multi-pass permute streams).
 * Still mixes a small counter so back-to-back calls differ if desired.
 */
void seed_rng_u64(uint64_t seed);

/**
 * Draws one random integer uniformly from the closed range [lo, hi].
 * Used everywhere the engine needs a discrete random choice (catalogue
 * weights, pad counts, noise jitter, etc.).
 * @param lo inclusive lower bound
 * @param hi inclusive upper bound
 * @pre lo <= hi
 * @return integer in [lo, hi]
 */
int rnd(int lo, int hi);

/** FNV-1a 64-bit hash of a byte buffer (uniqueness harness). */
uint64_t hash64(const uint8_t* data, size_t len);

inline uint64_t hash64(const std::vector<uint8_t>& v) {
    return hash64(v.data(), v.size());
}

} // namespace aether
