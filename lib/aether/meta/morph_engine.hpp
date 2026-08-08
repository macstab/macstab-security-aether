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
 * @file morph_engine.hpp
 * @brief Industry morph framework: staged pipeline, dual product, verify, batch.
 *
 * Pipeline: Lift → Analyze → [policy stages] → Assemble → Verify
 */

#include "aether/meta/morph_real.hpp"
#include "aether/meta/real_analysis.hpp"
#include "aether/meta/real_ir.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace aether {

enum class MorphStage : int {
    Lift = 0,
    Analyze,
    Diversify,
    Expand,
    Shuffle,
    Split,
    Permute,
    Assemble,
    Verify,
    Count
};

const char* morph_stage_name(MorphStage s);

/**
 * Product modes (different guarantees):
 *  - Lab: break-rate 0 required on pure; SAFE/LAB; hard-fail pure
 *  - IndustryExperimental: best-effort; more passes
 *  - Industry: hard-fail pure + multi-input verify; structural re-lift required
 */
enum class ProductMode : int {
    Lab = 0,
    IndustryExperimental = 1,
    Industry = 2,
};

struct MorphEngineConfig {
    ProductMode product = ProductMode::Lab;
    MorphPolicy policy = MorphPolicy::Safe;
    MorphDomain domain = MorphDomain::PureRegs;
    bool verify_pure = true;
    bool multi_input_verify = false; ///< industry: several (rdi,rsi) pairs
    bool verify_native = false;      ///< industry: try_exec multi-input when x86-64
    bool size_fit = false;           ///< no expand/split/permute; prefer fit in original size
    bool allow_identity_fallback = true;
    bool require_pure = false;
    bool require_structural = true; ///< output must re-lift non-empty
    int extra_lab_passes = 1;
    double max_size_ratio = 0.0; ///< fail if blowup exceeds (0 = ignore)
    uint64_t base_address = 0x1000;
    uint64_t arg_rdi = 0;
    uint64_t arg_rsi = 0;
    bool has_expected_rax = false;
    uint32_t expected_rax = 0;
};

struct MorphEngineResult {
    bool ok = false;
    std::vector<uint8_t> bytes;
    size_t bytes_in = 0;
    size_t bytes_out = 0;
    size_t blocks_in = 0;
    size_t blocks_out = 0;
    size_t insns_in = 0;
    size_t insns_out = 0;
    bool pure_verified = false;
    uint32_t pure_rax = 0;
    size_t multi_input_checked = 0;
    size_t native_checked = 0;
    size_t native_breaks = 0;
    bool structural_ok = false;
    bool cfg_resolved = false;
    bool may_permute = false;
    bool has_memory = false;
    bool has_calls = false;
    bool regs_only = false;
    std::string error;
    std::vector<std::string> stages_run;
};

class MorphEngine {
  public:
    explicit MorphEngine(MorphEngineConfig cfg = {});

    void set_config(const MorphEngineConfig& cfg);
    const MorphEngineConfig& config() const;

    MorphEngineResult morph(const uint8_t* code, size_t len) const;
    MorphEngineResult morph(const std::vector<uint8_t>& code) const;

    std::vector<MorphEngineResult> morph_batch(const std::vector<std::vector<uint8_t>>& inputs,
                                               bool stop_on_fail = false) const;

  private:
    MorphEngineConfig cfg_;
};

struct MorphBatchReport {
    size_t jobs = 0;
    size_t ok = 0;
    size_t fail = 0;
    size_t pure_verified = 0;
    size_t pure_breaks = 0;
    double avg_size_ratio = 1.0;
    bool pass = false;
};

MorphBatchReport summarize_batch(const std::vector<MorphEngineResult>& results);
std::string format_batch_report(const MorphBatchReport& r);
std::string format_batch_report_json(const MorphBatchReport& r);

bool industry_framework_selftest(size_t pure_samples = 64);

/** Industry product multi-input + IndustryExperimental smoke. */
bool industry_product_selftest(size_t pure_samples = 48);

/** Full industry finish gate: product + rewrite + optional native + corpus scale. */
bool industry_finish_selftest();

} // namespace aether
