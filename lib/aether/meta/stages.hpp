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
 * @file stages.hpp
 * @brief Multi-stage + nested metamorphic generations.
 *
 * Design (research / educational):
 *  - Each stage: disasm → random transform schedule → assemble (fresh encodings).
 *  - Feed-forward: stage N output becomes stage N+1 input (re-morph).
 *  - Nested: outer generation embeds an independently morph'd child blob
 *    (dynamic code carrying code that was itself generated dynamically).
 */

#include "aether/meta/transforms.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace aether {

/** One stage's outcome for demos / logging. */
struct StageReport {
    int index = 0;           ///< 0-based stage number in the full run
    size_t bytes_in = 0;     ///< input size to this stage
    size_t bytes_out = 0;    ///< assembled size after this stage
    size_t blocks = 0;       ///< IR blocks before assemble
    size_t insns = 0;        ///< IR insns before assemble
    bool flattened = false;  ///< flatten used in this stage
    int transform_steps = 0; ///< steps requested in the random schedule
    std::string kind;        ///< "morph" | "nested-child" | "nested-wrap"
};

/**
 * Runs a single metamorphic stage on a code buffer.
 * disasm → random transform schedule → assemble → random trailing nops.
 * @param src input bytes
 * @param len input length
 * @param report filled with stage metrics if non-null
 * @return newly assembled morph of @p src
 */
std::vector<uint8_t> morph_stage(const uint8_t* src, size_t len, StageReport* report = nullptr);

/**
 * Like morph_stage but forces a named structural LayerMode (e.g. permute-heavy).
 * Sets report->kind to the mode name when report is non-null.
 */
std::vector<uint8_t>
morph_stage_mode(const uint8_t* src, size_t len, LayerMode mode, StageReport* report = nullptr);

/**
 * Multi-stage feed-forward morph: each stage re-decodes previous output.
 * @param src seed machine code
 * @param len seed length
 * @param reports optional; new reports are appended (not cleared)
 * @param min_stages minimum generations (default 2)
 * @param max_stages maximum generations (default 4)
 * @param kind label stored in each StageReport::kind (default "morph")
 * @return final generation after the last stage
 */
std::vector<uint8_t> multi_stage_morph(const uint8_t* src,
                                       size_t len,
                                       std::vector<StageReport>* reports = nullptr,
                                       int min_stages = 2,
                                       int max_stages = 4,
                                       const char* kind = "morph");

/**
 * Nested generations: multi-stage child XOR-wrapped inside multi-stage outer.
 * Child is never re-disassembled after packaging.
 */
std::vector<uint8_t>
nested_generation(const uint8_t* src, size_t len, std::vector<StageReport>* reports = nullptr);

/**
 * Top research pipeline: usually nested multi-stage; else pure multi-stage.
 */
std::vector<uint8_t>
engine_pipeline(const uint8_t* src, size_t len, std::vector<StageReport>* reports = nullptr);

} // namespace aether
