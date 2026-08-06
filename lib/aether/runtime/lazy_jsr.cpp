/**
 * SPDX-License-Identifier: LicenseRef-Aether-Research
 * Copyright (c) 2024–2026 Aether Research Project contributors.
 *
 * Licensed under the Aether Research License in the repository root file
 * LICENSE. Authorized lab / lawful private use only as defined there.
 * By using this file you accept LICENSE. See docs/LICENSE_ACCEPTANCE.md.
 */

#include "aether/runtime/lazy_jsr.hpp"

#include "aether/common/rng.hpp"
#include "aether/meta/crypto_cascade.hpp"
#include "aether/meta/stages.hpp"
#include "aether/meta/transforms.hpp"
#include "aether/noise/noise.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <sys/time.h>
#include <unistd.h>
#include <vector>

namespace aether {
namespace {

using Clock = std::chrono::steady_clock;
using SlotFn = void (*)();

std::atomic<SlotFn> g_slot{nullptr};

const uint8_t* g_seed = nullptr;
size_t g_seed_len = 0;
LazyConfig g_cfg{};
LazyResult* g_result = nullptr;
std::vector<uint8_t>* g_out_body = nullptr;

long sample_wall_time_ms() {
    struct timeval tv{};
    gettimeofday(&tv, nullptr);
    return static_cast<long>(tv.tv_sec) * 1000L + static_cast<long>(tv.tv_usec) / 1000L;
}

void idle_fn() {
    (void)sample_wall_time_ms();
    do_random_noise(1);
    struct timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    (void)ts;
}

void clear_jsr_to_idle() {
    g_slot.store(&idle_fn, std::memory_order_release);
    if (g_result)
        g_result->jsr_cleared = true;
}

void external_trigger_set_jsr(SlotFn bootstrap) {
    g_slot.store(bootstrap, std::memory_order_release);
    if (g_result)
        g_result->trigger_armed = true;
    printf("[trigger] arm JSR → bootstrap  (no cascade material yet)\n");
}

void wipe_bytes(std::vector<uint8_t>& body) {
    if (!body.empty()) {
        volatile uint8_t* p = body.data();
        for (size_t i = 0; i < body.size(); i++)
            p[i] = 0;
    }
    body.clear();
    body.shrink_to_fit();
}

/**
 * Research impl leaf (still educational marker + morph of seed/handoff).
 */
std::vector<uint8_t> emit_research_implementation(const uint8_t* seed, size_t seed_len) {
    StageReport rep;
    // Prefer permute-heavy structure for the leaf scaffold.
    auto body = morph_stage_mode(seed, seed_len, LayerMode::PermuteHeavy, &rep);
    static const char kMark[] = "AETHER-IMPL";
    body.insert(body.end(),
                reinterpret_cast<const uint8_t*>(kMark),
                reinterpret_cast<const uint8_t*>(kMark) + sizeof(kMark) - 1);
    if (body.empty() || body.back() != 0xC3)
        body.push_back(0xC3);
    if (g_result) {
        rep.kind = "impl/permute-heavy";
        g_result->layer_reports.push_back(rep);
    }
    return body;
}

/**
 * Choose cascade depth with the same probabilistic spirit as layer chaining:
 * grow until random stop (after min), force stop at max.
 */
int choose_cascade_depth(int min_L, int max_L, int cont_pct) {
    int d = min_L;
    while (d < max_L) {
        if (rnd(1, 100) > cont_pct)
            break;
        d++;
    }
    return d;
}

/**
 * Real crypto cascade init:
 *  1) Build impl leaf
 *  2) Onion-encrypt L times (keystream cascade) — each wrap is a crypto layer
 *  3) Peel outside-in: decrypt one layer → wipe that package → next
 *  4) Structural morph modes tag each wrap; peel logs cascade steps
 *
 * Poly morph of shells is applied to leaf; wraps are true encrypt/decrypt.
 */
void run_crypto_cascade_init(const uint8_t* seed, size_t seed_len) {
    if (!seed || !seed_len)
        return;

    int min_L = g_cfg.min_layers < 1 ? 1 : g_cfg.min_layers;
    int max_L = g_cfg.max_layers < min_L ? min_L : g_cfg.max_layers;
    int cont = g_cfg.continue_pct;
    if (cont < 0)
        cont = 0;
    if (cont > 100)
        cont = 100;

    const int depth = choose_cascade_depth(min_L, max_L, cont);

    printf("[cascade] REAL multi-layer decrypt cascade  depth=%d  (min=%d max=%d P(grow)=%d%%)\n",
           depth,
           min_L,
           max_L,
           cont);
    printf("[cascade] build inside-out → peel outside-in → wipe each layer when done\n");

    // --- Build leaf (only exists now) ---
    auto leaf = emit_research_implementation(seed, seed_len);
    printf("[cascade] leaf GENERATED  %zu B  (research impl)\n", leaf.size());

    std::vector<CascadeStep> build_steps;
    auto onion = cascade_build(leaf, depth, &build_steps);
    wipe_bytes(leaf); // leaf only lives encrypted inside onion now
    printf("[cascade] onion BUILT     outer=%zu B  wraps=%zu\n", onion.size(), build_steps.size());

    for (const auto& s : build_steps) {
        LayerStep ls;
        ls.index = s.index;
        ls.mode = s.mode;
        ls.bytes = s.cipher_bytes;
        ls.blocks = 0;
        ls.flattened = (s.mode == "flatten");
        ls.decision = LayerDecision::NextLayer;
        if (g_result)
            g_result->layer_steps.push_back(ls);
        printf("[cascade]   wrap %02d  mode=%-14s  plain=%zu → cipher=%zu\n",
               s.index + 1,
               s.mode.c_str(),
               s.plain_bytes,
               s.cipher_bytes);
    }

    // --- Peel: decrypt cascade; each package wiped when layer done ---
    std::vector<CascadeStep> peel_steps;
    std::vector<uint8_t> cur = std::move(onion);
    size_t peak = cur.size();
    int peeled = 0;

    printf("[cascade] PEEL start\n");
    if (!cascade_peel(cur, &peel_steps)) {
        printf("[cascade] PEEL failed (bad magic/structure)\n");
        wipe_bytes(cur);
        return;
    }

    for (const auto& s : peel_steps) {
        peeled++;
        if (s.cipher_bytes > peak)
            peak = s.cipher_bytes;
        printf("[cascade]   peel %02d  mode=%-14s  decrypted %zu B  leaf=%s  (prior pack wiped)\n",
               s.index + 1,
               s.mode.c_str(),
               s.plain_bytes,
               s.is_leaf ? "yes" : "no");

        LayerStep ls;
        ls.index = s.index;
        ls.mode = std::string("peel/") + s.mode;
        ls.bytes = s.plain_bytes;
        ls.decision = s.is_leaf ? LayerDecision::EmitImpl : LayerDecision::NextLayer;
        if (g_result)
            g_result->layer_steps.push_back(ls);
    }

    printf("[cascade] PEEL done  final_plain=%zu B  layers_peeled=%d\n", cur.size(), peeled);

    if (g_out_body)
        *g_out_body = cur;

    if (g_result) {
        g_result->generated = true;
        g_result->impl_emitted = true;
        g_result->generated_bytes = cur.size();
        g_result->layers = depth;
        g_result->layers_wiped = peeled + 1; // wraps + final wipe
        g_result->peak_layer_bytes = peak;
        g_result->wiped = true;
    }

    wipe_bytes(cur);
    printf("[cascade] final plaintext WIPED  (init complete)\n");
}

void bootstrap_fn() {
    clear_jsr_to_idle();
    printf("[bootstrap] JSR cleared first\n");

    if (g_result)
        g_result->payload_entered = true;

    if (!g_seed || !g_seed_len) {
        printf("[bootstrap] no seed\n");
        return;
    }

    // World-class permute is used inside morph modes / leaf scaffold.
    run_crypto_cascade_init(g_seed, g_seed_len);
    printf("[bootstrap] cascade init finished; only idle remains\n");
}

void dispatch_slot() {
    SlotFn fn = g_slot.load(std::memory_order_acquire);
    if (!fn)
        fn = &idle_fn;
    fn();
}

} // namespace

LazyResult run_lazy_jsr(const uint8_t* seed,
                        size_t len,
                        const LazyConfig& cfg,
                        std::vector<uint8_t>* out_body) {
    LazyResult r{};
    g_result = &r;
    g_seed = seed;
    g_seed_len = len;
    g_cfg = cfg;
    g_out_body = out_body;
    g_slot.store(&idle_fn, std::memory_order_release);

    printf("[main] idle | shit only — nothing to validate offline\n");

    const auto t0 = Clock::now();
    bool trigger_done = false;

    for (int i = 0; i < cfg.max_idle_rounds; i++) {
        dispatch_slot();
        r.idle_rounds++;
        const auto ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t0).count();
        r.idle_ms_elapsed = static_cast<long>(ms);

        if (cfg.idle_only)
            continue;

        if (!trigger_done && (cfg.force_fire || ms >= cfg.idle_ms)) {
            external_trigger_set_jsr(&bootstrap_fn);
            trigger_done = true;
        }

        if (r.payload_entered && r.jsr_cleared) {
            r.post_clear_idle++;
            if (static_cast<int>(r.post_clear_idle) >= cfg.post_clear_rounds)
                break;
        }
    }

    if (cfg.idle_only) {
        printf("[main] idle-only\n");
    } else if (!r.trigger_armed) {
        printf("[main] never armed\n");
    } else if (!r.payload_entered) {
        dispatch_slot();
        for (int k = 0; k < cfg.post_clear_rounds; k++) {
            dispatch_slot();
            r.post_clear_idle++;
        }
    }

    g_slot.store(&idle_fn, std::memory_order_release);
    g_seed = nullptr;
    g_seed_len = 0;
    g_out_body = nullptr;
    g_result = nullptr;
    return r;
}

} // namespace aether
