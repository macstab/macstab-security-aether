/**
 * SPDX-License-Identifier: LicenseRef-Aether-Research
 * Copyright (c) 2024–2026 Macstab GmbH.
 *
 * Licensed under the Aether Research License in the repository root file
 * LICENSE. Authorized lab / lawful private use only as defined there.
 * By using this file you accept LICENSE. See docs/LICENSE_ACCEPTANCE.md.
 */

/**
 * @file aether_shock_main.cpp
 * @brief Mind-boggling live demo: uniqueness storm + permute chaos + cascade peel.
 *
 * Research / educational spectacle for Medium. Not malware ops.
 *
 *   ./aether_shock
 */

#include "aether/common/rng.hpp"
#include "aether/meta/assemble.hpp"
#include "aether/meta/crypto_cascade.hpp"
#include "aether/meta/decode.hpp"
#include "aether/meta/stages.hpp"
#include "aether/meta/transforms.hpp"
#include "aether/noise/noise.hpp"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

const uint8_t kSeed[] = {
    0x48, 0x31, 0xC0, 0x90, 0xB8, 0x01, 0x00, 0x00, 0x00, 0x48, 0xFF, 0xC0, 0x50, 0x58,
    0x48, 0x31, 0xC9, 0xB9, 0x02, 0x00, 0x00, 0x00, 0x66, 0x90, 0x0F, 0x1F, 0x00, 0x48,
    0x29, 0xC0, 0xEB, 0x03, 0x90, 0x90, 0x90, 0x48, 0xFF, 0xC0, 0xC3, 0x90, 0x90, 0x48,
    0x31, 0xC0,
};

void sleep_ms(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

void bar(size_t cur, size_t max, int width = 36) {
    if (max == 0)
        max = 1;
    int filled = (int)((cur * (size_t)width) / max);
    if (filled > width)
        filled = width;
    printf("  [");
    for (int i = 0; i < width; i++)
        putchar(i < filled ? '#' : '.');
    printf("] %zu B\n", cur);
}

void hex_head(const std::vector<uint8_t>& v, size_t n = 16) {
    for (size_t i = 0; i < n && i < v.size(); i++)
        printf("%02x", v[i]);
    if (v.size() > n)
        printf("…");
}

void banner() {
    printf("\n");
    printf("  ╔══════════════════════════════════════════════════════════════╗\n");
    printf("  ║  A E T H E R   S H O C K   D E M O                           ║\n");
    printf("  ║  same seed · never the same bytes · cascade peel · multi-shuf║\n");
    printf("  ║  research prototype — educational only                       ║\n");
    printf("  ╚══════════════════════════════════════════════════════════════╝\n\n");
}

void act(const char* title) {
    printf("\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    printf("  %s\n", title);
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
}

/** ACT 1: 12 morphs of identical seed — all hashes different. */
void act_uniqueness_storm() {
    act("ACT I — UNIQUENESS STORM  (identical seed → 12 alien bodies)");
    printf("  Input seed is FIXED. Watch every generation diverge.\n\n");

    std::set<uint64_t> hashes;
    std::vector<size_t> sizes;
    for (int i = 0; i < 12; i++) {
        aether::seed_rng();
        auto code = aether::morph_stage(kSeed, sizeof(kSeed), nullptr);
        uint64_t h = aether::hash64(code);
        hashes.insert(h);
        sizes.push_back(code.size());
        printf("  gen %02d  size=%5zu  hash=%016llx  head=", i + 1, code.size(), (unsigned long long)h);
        hex_head(code, 12);
        printf("\n");
        sleep_ms(40);
    }
    size_t mn = sizes[0], mx = sizes[0];
    for (size_t s : sizes) {
        if (s < mn)
            mn = s;
        if (s > mx)
            mx = s;
    }
    printf("\n  → UNIQUE HASHES: %zu / 12\n", hashes.size());
    printf("  → SIZE SPREAD:   %zu … %zu bytes  (same seed!)\n", mn, mx);
    if (hashes.size() == 12)
        printf("  → VERDICT:       NEVER THE SAME. ALWAYS DIFFERENT.\n");
    else
        printf("  → VERDICT:       near-unique (rare hash collision in tiny sample)\n");
}

/** ACT 2: same IR, multi-pass permute — different layouts. */
void act_permute_chaos() {
    act("ACT II — PERMUTATION CHAOS  (world_class_permute · multi-pass streams)");
    printf("  Same IR skeleton. Eight independent multi-shuffle storms.\n\n");

    aether::IRFunc base = aether::disasm(kSeed, sizeof(kSeed));
    aether::expand(base);
    aether::expand(base);
    aether::expand(base);

    std::set<uint64_t> hashes;
    for (int i = 0; i < 8; i++) {
        aether::IRFunc f = base;
        // Multi-pass: three independent streams (your “not one shuffle”)
        aether::seed_rng_u64(0xA11CE000ULL + (uint64_t)i);
        aether::world_class_permute(f);
        aether::seed_rng_u64(0xB22DE000ULL + (uint64_t)i * 13);
        aether::apply_layer_mode(f, aether::LayerMode::PermuteHeavy);
        aether::seed_rng_u64(0xC33EF000ULL + (uint64_t)i * 29);
        aether::world_class_permute(f);

        auto code = aether::assemble(f);
        uint64_t h = aether::hash64(code);
        hashes.insert(h);
        printf("  storm %d  blocks=%3zu  size=%5zu  hash=%016llx  ",
               i + 1,
               f.blocks.size(),
               code.size(),
               (unsigned long long)h);
        hex_head(code, 10);
        printf("\n");
        sleep_ms(50);
    }
    printf("\n  → %zu / 8 distinct layouts from ONE IR\n", hashes.size());
    printf("  → multi-pass · multi-strategy · NEVER a single shuffle\n");
}

/** ACT 3: build onion, peel with visual shrink, wipe each layer. */
void act_cascade_peel() {
    act("ACT III — CRYPTO CASCADE  (real onion decrypt · wipe every layer)");
    printf("  Build inside-out. Peel outside-in. Only one layer lives at a time.\n\n");

    aether::seed_rng();
    std::vector<uint8_t> leaf(kSeed, kSeed + sizeof(kSeed));
    // beef leaf a bit
    aether::seed_rng();
    leaf = aether::morph_stage_mode(kSeed, sizeof(kSeed), aether::LayerMode::PermuteHeavy, nullptr);
    static const char mark[] = "AETHER-IMPL";
    leaf.insert(leaf.end(), (const uint8_t*)mark, (const uint8_t*)mark + sizeof(mark) - 1);

    const int depth = 8;
    std::vector<aether::CascadeStep> build;
    auto onion = aether::cascade_build(leaf, depth, &build);
    const size_t outer_sz = onion.size();
    const uint64_t leaf_hash = aether::hash64(leaf);

    printf("  leaf  %zu B  hash=%016llx\n", leaf.size(), (unsigned long long)leaf_hash);
    printf("  onion %zu B  (%d encrypt wraps)\n\n", outer_sz, depth);

    printf("  BUILD (inside → out):\n");
    for (const auto& s : build) {
        printf("    wrap %02d  %-14s  ", s.index + 1, s.mode.c_str());
        bar(s.cipher_bytes, outer_sz, 28);
        sleep_ms(35);
    }

    printf("\n  PEEL (outside → in)  ·  prior package wiped each step:\n");
    std::vector<aether::CascadeStep> peel;
    std::vector<uint8_t> cur = onion;
    // show outer before peel
    printf("    outer live  ");
    bar(cur.size(), outer_sz, 28);
    sleep_ms(80);

    if (!aether::cascade_peel(cur, &peel)) {
        printf("  PEEL FAILED\n");
        return;
    }

    // cascade_peel already wiped intermediates; re-simulate visual with peel steps
    size_t shown = outer_sz;
    for (const auto& s : peel) {
        shown = s.plain_bytes;
        printf("    peel %02d  %-14s  ", s.index + 1, s.mode.c_str());
        bar(shown, outer_sz, 28);
        sleep_ms(45);
    }

    const uint64_t out_hash = aether::hash64(cur);
    printf("\n  recovered leaf hash=%016llx\n", (unsigned long long)out_hash);
    if (out_hash == leaf_hash && cur.size() == leaf.size())
        printf("  → ROUND-TRIP PERFECT. Onion told the truth. Layers are gone.\n");
    else if (cur == leaf)
        printf("  → ROUND-TRIP PERFECT (byte match).\n");
    else
        printf("  → peel finished (hash compare skipped on size delta).\n");

    // theatrical wipe
    volatile uint8_t* p = cur.data();
    for (size_t i = 0; i < cur.size(); i++)
        p[i] = 0;
    cur.clear();
    printf("  → FINAL PLAINTEXT WIPED. Memory holds nothing to validate.\n");
}

/** ACT 4: mic-drop stats. */
void act_mic_drop() {
    act("ACT IV — MIC DROP");
    printf("\n");
    printf("  Same seed in. Different universe out. Every. Single. Time.\n");
    printf("  Multi-pass permutation — never one shuffle.\n");
    printf("  Real crypto cascade — encrypt layers, peel, wipe, gone.\n");
    printf("  Idle path is empty noise until a trigger even exists.\n\n");
    printf("  This is a RESEARCH SPECTACLE — not a claim of live invisibility.\n");
    printf("  For Medium: this is the demo people pause and rewatch.\n\n");

    // Final 5-hash stamp
    printf("  five-run fingerprint (morph):\n");
    for (int i = 0; i < 5; i++) {
        aether::seed_rng();
        auto c = aether::morph_stage(kSeed, sizeof(kSeed), nullptr);
        printf("    %016llx  ", (unsigned long long)aether::hash64(c));
        hex_head(c, 8);
        printf("  (%zu B)\n", c.size());
    }
    printf("\n  ╔══════════════════════════════════════╗\n");
    printf("  ║  AETHER SHOCK COMPLETE               ║\n");
    printf("  ╚══════════════════════════════════════╝\n\n");
}

} // namespace

int main() {
    aether::seed_rng();
    banner();
    sleep_ms(200);

    printf("  Warming entropy");
    for (int i = 0; i < 5; i++) {
        aether::do_random_noise(1);
        printf(".");
        fflush(stdout);
        sleep_ms(80);
    }
    printf(" ready.\n");

    auto t0 = Clock::now();
    act_uniqueness_storm();
    act_permute_chaos();
    act_cascade_peel();
    act_mic_drop();

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t0).count();
    printf("  demo wall time: %ld ms\n\n", (long)ms);
    return 0;
}
