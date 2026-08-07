/**
 * SPDX-License-Identifier: LicenseRef-Aether-Research
 * Copyright (c) 2024–2026 Macstab GmbH.
 *
 * Licensed under the Aether Research License in the repository root file
 * LICENSE. Authorized lab / lawful private use only as defined there.
 * By using this file you accept LICENSE. See docs/LICENSE_ACCEPTANCE.md.
 */

#include "aether/meta/pipeline.hpp"

#include "aether/meta/decode.hpp"
#include "aether/noise/noise.hpp"

namespace aether {
namespace {

PipelineStats g_last_stats;
std::vector<StageReport> g_last_reports;

} // namespace

/**
 * Returns stats from the last full_pipeline() invocation.
 */
const PipelineStats& last_pipeline_stats() {
    return g_last_stats;
}

/**
 * Returns the detailed stage log from the last full_pipeline() call.
 */
const std::vector<StageReport>& last_stage_reports() {
    return g_last_reports;
}

/**
 * Orchestrates noise + multi-stage/nested engine and records demo stats.
 */
std::vector<uint8_t> full_pipeline(const uint8_t* src, size_t len) {
    do_random_noise(2);

    g_last_stats = {};
    g_last_reports.clear();

    // Baseline IR metrics from the seed (for [1]/[2] narrative).
    if (src && len) {
        IRFunc base = disasm(src, len);
        g_last_stats.blocks_in = base.blocks.size();
        for (const auto& b : base.blocks)
            g_last_stats.insns_in += b.insns.size();
    }

    auto code = engine_pipeline(src, len, &g_last_reports);

    g_last_stats.bytes_out = code.size();
    g_last_stats.stages = static_cast<int>(g_last_reports.size());
    for (const auto& r : g_last_reports) {
        if (r.flattened)
            g_last_stats.flattened = true;
        if (r.kind == "nested-child")
            g_last_stats.nested_children++;
        if (r.kind == "nested-wrap")
            g_last_stats.nested = true;
        g_last_stats.blocks_out = r.blocks; // last stage that reports blocks
    }

    do_random_noise(1);
    return code;
}

} // namespace aether
