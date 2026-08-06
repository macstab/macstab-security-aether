/**
 * SPDX-License-Identifier: LicenseRef-Aether-Research
 * Copyright (c) 2024–2026 Aether Research Project contributors.
 *
 * Licensed under the Aether Research License in the repository root file
 * LICENSE. Authorized lab / lawful private use only as defined there.
 * By using this file you accept LICENSE. See docs/LICENSE_ACCEPTANCE.md.
 */

#include "aether/noise/noise.hpp"

#include "aether/common/rng.hpp"

#include <algorithm>
#include <fcntl.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <vector>

namespace aether {
namespace {

/**
 * Calls getpid() and discards the result.
 * Adds a common, harmless process-id query to the noise stream.
 */
void noise_getpid() {
    (void)getpid();
}

/**
 * Calls gettimeofday() and discards the result.
 * Adds a common wall-clock query to the noise stream.
 */
void noise_time() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
}

/**
 * Requests a few random bytes (getrandom when available) or fills a local
 * buffer with rnd() on platforms without getrandom.
 * Bytes are discarded; purpose is only to exercise the entropy syscall path.
 */
void noise_getrandom() {
    unsigned char b[8];
#if defined(SYS_getrandom)
    (void)syscall(SYS_getrandom, b, 8, 0);
#else
    for (int i = 0; i < 8; i++)
        b[i] = (unsigned char)rnd(0, 255);
#endif
    (void)b;
}

/**
 * Opens /dev/null for read and closes the file descriptor if open succeeded.
 * Adds a trivial open/close pair to the noise stream.
 */
void noise_open_null() {
    int fd = open("/dev/null", O_RDONLY);
    if (fd >= 0)
        close(fd);
}

/**
 * Sleeps a random duration between ~50µs and ~600µs via nanosleep.
 * Used both as a table entry and as optional inter-call jitter.
 */
void noise_nanosleep() {
    struct timespec ts = {0, (long)rnd(50000, 600000)};
    nanosleep(&ts, nullptr);
}

/**
 * Calls getppid() and discards the result.
 * Adds a parent-pid query to the noise stream.
 */
void noise_getppid() {
    (void)getppid();
}

/**
 * Calls getuid() and discards the result.
 * Adds a user-id query to the noise stream.
 */
void noise_getuid() {
    (void)getuid();
}

using NoiseFn = void (*)();

const NoiseFn kNoiseTable[] = {
    noise_getpid,
    noise_time,
    noise_getrandom,
    noise_open_null,
    noise_nanosleep,
    noise_getppid,
    noise_getuid,
};

constexpr int kNoiseCount = 7;

} // namespace

/**
 * Executes several rounds of harmless system calls in a random order.
 * Each round walks a shuffled table of getpid/gettimeofday/getrandom/
 * open(/dev/null)/nanosleep/getppid/getuid, and may insert extra short
 * sleeps between calls so the behavioral timeline differs every run.
 * @param rounds how many full passes over the noise table
 */
void do_random_noise(int rounds) {
    std::vector<int> order(kNoiseCount);
    for (int i = 0; i < kNoiseCount; i++)
        order[i] = i;
    std::shuffle(order.begin(), order.end(), rng());
    for (int r = 0; r < rounds; r++) {
        for (int idx : order) {
            kNoiseTable[idx]();
            if (rnd(0, 2) == 0)
                noise_nanosleep();
        }
        std::shuffle(order.begin(), order.end(), rng());
    }
}

} // namespace aether
