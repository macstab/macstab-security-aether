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
 * @file equiv.hpp
 * @brief Equivalence oracle — educational IR + real (1B) restricted morph.
 *
 * Domains (all in one hard gate):
 *  - EduPureRax: IR morphs + morph_stage + multi_stage + multi_pass_ir
 *  - RealRestricted: Zydis RealFunc 1B morph (+ re-lift)
 *  - CascadeLeaf: onion build/peel → leaf RAX invariant
 *  - RealText: morph real ELF .text windows (structural + pure gadgets)
 *
 * CI: 0 semantic breaks. Native EAX when host is x86-64.
 */

#include "aether/meta/ir.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace aether {

enum class EquivDomain : int {
    EduPureRax = 0,
    RealRestricted = 1,
};

struct EquivBreak {
    int seed_id = -1;
    int trial = -1;
    std::string path;
    std::string domain;
    uint32_t expected = 0;
    uint32_t got = 0;
    bool interpret_failed = false;
};

struct EquivReport {
    int seeds = 0;
    int trials = 0;
    int breaks = 0;
    int unique_hashes = 0;
    int native_checked = 0;
    int native_breaks = 0;
    int paths_exercised = 0;
    int real_trials = 0;
    int real_breaks = 0;
    int cascade_trials = 0;
    int cascade_breaks = 0;
    int real_text_windows = 0; ///< real ELF .text morph windows exercised
    bool real_text_ok = false; ///< victim_clean (or alt) loaded + morph non-empty
    std::string domain_summary;
    std::vector<EquivBreak> sample_breaks;

    bool pass() const {
        return trials > 0 && breaks == 0 && native_breaks == 0 && real_breaks == 0 &&
               cascade_breaks == 0 && real_text_ok;
    }
};

std::optional<uint32_t> interpret_rax(const IRFunc& f);

std::optional<uint32_t> try_exec_x64_eax(const uint8_t* code, size_t len);
inline std::optional<uint32_t> try_exec_x64_eax(const std::vector<uint8_t>& code) {
    return try_exec_x64_eax(code.data(), code.size());
}

/** Native exec with SysV args in RDI/RSI (x86-64 only). */
std::optional<uint32_t> try_exec_x64_eax_args(const uint8_t* code,
                                              size_t len,
                                              uint64_t rdi,
                                              uint64_t rsi);
inline std::optional<uint32_t> try_exec_x64_eax_args(const std::vector<uint8_t>& code,
                                                     uint64_t rdi,
                                                     uint64_t rsi) {
    return try_exec_x64_eax_args(code.data(), code.size(), rdi, rsi);
}

/**
 * Full campaign: educational corpus + real restricted morph on same pure seeds.
 */
EquivReport run_equivalence_campaign(uint64_t campaign_seed = 0xAE7EC0DEu,
                                     int rounds_per_seed = 32);

std::string format_equiv_report(const EquivReport& r);
/** Machine-readable report for artifacts / stranger reproduce. */
std::string format_equiv_report_json(const EquivReport& r);

} // namespace aether
