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
 * @file pipeline.hpp
 * @brief Public entry to the multi-stage metamorphic engine + demo stats.
 */

#include "aether/meta/stages.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace aether {

/** Aggregate metrics for the last full_pipeline() call (demo printer). */
struct PipelineStats {
    size_t blocks_in = 0;    ///< IR blocks after first disasm of seed
    size_t blocks_out = 0;   ///< blocks on last morph stage (if any)
    size_t insns_in = 0;     ///< IR insns after first disasm of seed
    size_t bytes_out = 0;    ///< final output size
    bool flattened = false;  ///< any stage used flatten
    int stages = 0;          ///< number of stage reports recorded
    int nested_children = 0; ///< stages tagged nested-child
    bool nested = false;     ///< true if a nest wrap stage ran
};

/**
 * Returns stats from the last full_pipeline() invocation.
 */
const PipelineStats& last_pipeline_stats();

/**
 * Returns the detailed stage log from the last full_pipeline() call.
 */
const std::vector<StageReport>& last_stage_reports();

/**
 * Runs the multi-stage (+ nested) educational metamorphic engine on @p src.
 * Random: stage count, transform order, encodings, nesting, pad size.
 * @return unique machine-code buffer for this run
 */
std::vector<uint8_t> full_pipeline(const uint8_t* src, size_t len);

} // namespace aether
