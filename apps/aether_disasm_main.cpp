/**
 * SPDX-License-Identifier: LicenseRef-Aether-Research
 * Copyright (c) 2024–2026 Aether Research Project contributors.
 *
 * Licensed under the Aether Research License in the repository root file
 * LICENSE. Authorized lab / lawful private use only as defined there.
 * By using this file you accept LICENSE. See docs/LICENSE_ACCEPTANCE.md.
 */

/**
 * @file aether_disasm_main.cpp
 * @brief Step 1 CLI: real Zydis disassembly of ELF .text or raw blob.
 *
 * Usage:
 *   aether_disasm <elf-or-raw> [--raw] [--max N] [--base 0xADDR]
 */

#include "aether/meta/decode_real.hpp"
#include "aether/meta/elf_view.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace {

bool load_raw(const std::string& path, std::vector<uint8_t>& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return false;
    in.seekg(0, std::ios::end);
    auto n = in.tellg();
    if (n <= 0)
        return false;
    in.seekg(0, std::ios::beg);
    out.resize((size_t)n);
    in.read(reinterpret_cast<char*>(out.data()), n);
    return true;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: %s <elf-path|raw> [--raw] [--max N] [--base 0xADDR]\n", argv[0]);
        printf("  Real Zydis x86-64 disasm + CFG (Step 1 — kill toy-only IR).\n");
        printf("  Example: %s victim_clean\n", argv[0]);
        return 1;
    }

    bool force_raw = false;
    size_t max_print = 48;
    uint64_t base_override = 0;
    bool have_base = false;
    const char* path = argv[1];

    for (int i = 2; i < argc; i++) {
        if (std::strcmp(argv[i], "--raw") == 0)
            force_raw = true;
        else if (std::strcmp(argv[i], "--max") == 0 && i + 1 < argc)
            max_print = (size_t)std::strtoul(argv[++i], nullptr, 0);
        else if (std::strcmp(argv[i], "--base") == 0 && i + 1 < argc) {
            base_override = std::strtoull(argv[++i], nullptr, 0);
            have_base = true;
        }
    }

    if (!aether::has_real_disasm()) {
        printf("ERROR: real disasm backend not linked\n");
        return 2;
    }

    const uint8_t* code = nullptr;
    size_t len = 0;
    uint64_t base = 0;
    std::string label;
    aether::ElfTextRegion region;
    std::vector<uint8_t> raw;

    if (!force_raw) {
        region = aether::load_elf64_text(path);
        if (region.ok) {
            code = aether::elf_text_data(region);
            len = region.size;
            base = have_base ? base_override : region.vaddr;
            label = region.name;
            printf("[elf] %s  region=%s  file_off=0x%zx  vaddr=0x%llx  size=%zu\n",
                   path,
                   label.c_str(),
                   region.file_offset,
                   (unsigned long long)base,
                   len);
        } else {
            printf("[elf] %s (%s) — trying raw\n", path, region.error.c_str());
            force_raw = true;
        }
    }

    if (force_raw || !code) {
        if (!load_raw(path, raw)) {
            printf("ERROR: cannot read %s\n", path);
            return 1;
        }
        code = raw.data();
        len = raw.size();
        base = have_base ? base_override : 0;
        label = "raw";
        printf("[raw] %s  size=%zu  base=0x%llx\n", path, len, (unsigned long long)base);
    }

    // Cap decode size for huge .text in demos (full region still loadable).
    size_t decode_len = len;
    if (decode_len > 65536)
        decode_len = 65536;

    aether::RealFunc f = aether::disasm_real(code, decode_len, base);
    printf("[disasm] backend=Zydis  insns=%zu  blocks=%zu  decoded_bytes=%zu/%zu\n\n",
           f.insns.size(),
           f.blocks.size(),
           f.size_bytes(),
           len);

    if (f.insns.empty()) {
        printf("ERROR: no instructions decoded\n");
        return 3;
    }

    std::string dump = aether::format_real_func(f, max_print);
    fputs(dump.c_str(), stdout);

    // Quick CFG stats
    size_t branches = 0, rets = 0, calls = 0;
    for (const auto& in : f.insns) {
        if (in.is_ret)
            rets++;
        if (in.is_call)
            calls++;
        if (in.is_branch)
            branches++;
    }
    printf("\n[stats] branch_insns=%zu  calls=%zu  rets=%zu\n", branches, calls, rets);
    printf("[ok] Step 1 real binary IR path is live.\n");
    return 0;
}
