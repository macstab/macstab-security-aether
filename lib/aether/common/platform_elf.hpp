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
 * @file platform_elf.hpp
 * @brief Portable ELF64 types for host toolchains without <elf.h> (e.g. macOS).
 *
 * Infection still targets Linux ELF images only. This header only lets the
 * research tool build and manipulate ELF bytes on developer machines.
 */

#include <cstdint>

#if __has_include(<elf.h>)
#include <elf.h>
#else
#define ELFMAG "\177ELF"
#define EI_PAD 9
typedef struct {
    unsigned char e_ident[16];
    uint16_t e_type, e_machine;
    uint32_t e_version;
    uint64_t e_entry, e_phoff, e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize, e_phentsize, e_phnum, e_shentsize, e_shnum, e_shstrndx;
} Elf64_Ehdr;
#endif
