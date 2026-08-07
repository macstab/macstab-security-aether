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
 * @file lazy_jsr.hpp
 * @brief Probabilistic poly-layer init: each layer either chains next or emits impl.
 *
 * After external trigger + JSR clear, bootstrap runs layers:
 *   layer L (mode e.g. permute-heavy) is GENERATED
 *   with probability p  → function-ref path: handoff + generate NEXT layer
 *   with probability 1-p → EMIT real research implementation, stop
 *   either way: wipe full layer body when that layer is done
 *
 * Fixed min depth before emit is allowed; max depth forces emit.
 * Educational prototype only. Not undetectable. No spreading.
 */

#include "aether/meta/stages.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace aether {

/** Decision taken at the end of a layer. */
enum class LayerDecision : int {
    NextLayer = 0, ///< create handoff / function-ref path to generate another layer
    EmitImpl = 1,  ///< stop chaining; produce research "virus implementation" body
};

struct LayerStep {
    int index = 0;
    std::string mode; ///< e.g. "permute-heavy"
    LayerDecision decision = LayerDecision::NextLayer;
    size_t bytes = 0;
    size_t blocks = 0;
    bool flattened = false;
};

struct LazyConfig {
    int idle_ms = 150;
    int max_idle_rounds = 64;
    bool force_fire = false;
    bool idle_only = false;
    int post_clear_rounds = 4;
    int min_layers = 3;    ///< force at least this many poly layers before emit allowed
    int max_layers = 20;   ///< force emit by this depth
    int continue_pct = 70; ///< probability (0-100) to chain another layer when allowed
};

struct LazyResult {
    size_t idle_rounds = 0;
    long idle_ms_elapsed = 0;
    bool trigger_armed = false;
    bool payload_entered = false;
    bool jsr_cleared = false;
    bool generated = false;
    bool impl_emitted = false; ///< true if real-impl branch taken
    size_t generated_bytes = 0;
    size_t peak_layer_bytes = 0;
    int layers = 0;
    int layers_wiped = 0;
    bool wiped = false;
    size_t post_clear_idle = 0;
    std::vector<LayerStep> layer_steps;
    std::vector<StageReport> layer_reports;
};

LazyResult run_lazy_jsr(const uint8_t* seed,
                        size_t len,
                        const LazyConfig& cfg,
                        std::vector<uint8_t>* out_body = nullptr);

} // namespace aether
