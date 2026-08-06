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
 * @file noise.hpp
 * @brief Benign syscall noise — order and timing change every run.
 *
 * Purpose: demonstrate behavioral surface jitter for research articles.
 * All calls are harmless (getpid, open /dev/null, short sleeps, …).
 */

namespace aether {

/**
 * Executes several rounds of harmless system calls in a random order.
 * Each round walks a shuffled table of getpid/gettimeofday/getrandom/
 * open(/dev/null)/nanosleep/getppid/getuid, and may insert extra short
 * sleeps between calls so the behavioral timeline differs every run.
 * @param rounds how many full passes over the noise table (default 3)
 */
void do_random_noise(int rounds = 3);

} // namespace aether
