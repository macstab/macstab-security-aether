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
 * @file bench.hpp
 * @brief Morph benchmark: ≥1k functions, break-rate, regenerable JSON report.
 */

#include <cstddef>
#include <cstdint>
#include <string>

namespace aether {

struct BenchReport {
    std::string scope_line;
    int version_major = 1;
    int version_minor = 1;
    int version_patch = 0;

    size_t corpus_total = 0;
    size_t corpus_synthetic = 0;
    size_t corpus_elf = 0;
    size_t pure_checked = 0;
    size_t structural_checked = 0;

    size_t morph_ok = 0;
    size_t pure_breaks = 0;       ///< RAX mismatch or interpret fail after morph
    size_t structural_breaks = 0; ///< empty morph / re-lift fail
    size_t unique_hashes = 0;
    size_t native_checked = 0;
    size_t native_breaks = 0;

    double break_rate = 1.0; ///< (pure_breaks + structural_breaks) / max(1, morph attempts)
    double avg_size_ratio = 1.0; ///< mean(out_len / in_len) over successful morphs
    uint64_t elapsed_ms = 0;
    bool pass = false; ///< break_rate == 0 && corpus_total >= min_corpus && elf>=200

    std::string first_failure;
};

/**
 * Run morph bench:
 *  - generate @p pure_count synthetic pure functions (default 1000)
 *  - extract real functions from default corpus ELFs (+ optional extra path)
 *  - morph each with analysis-gated real morph
 *  - pure: interpret RAX match; structural: re-lift non-empty
 *  - optional native on no-JE pure (x86-64 only)
 *
 * Pass requires: break_rate==0, corpus_total>=1000, corpus_elf>=200.
 */
BenchReport run_morph_bench(size_t pure_count = 1000,
                            const char* elf_path = nullptr,
                            uint64_t seed = 0xBEACu,
                            int morph_rounds = 2);

std::string format_bench_report(const BenchReport& r);
std::string format_bench_report_json(const BenchReport& r);

/** Write JSON to path; returns false on IO error. */
bool write_bench_report_json(const BenchReport& r, const std::string& path);

} // namespace aether
