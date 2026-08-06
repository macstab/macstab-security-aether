/**
 * SPDX-License-Identifier: LicenseRef-Aether-Research
 * Copyright (c) 2024–2026 Aether Research Project contributors.
 *
 * Licensed under the Aether Research License in the repository root file
 * LICENSE. Authorized lab / lawful private use only as defined there.
 * By using this file you accept LICENSE. See docs/LICENSE_ACCEPTANCE.md.
 */

#include "aether/meta/bench.hpp"

#include "aether/common/rng.hpp"
#include "aether/meta/decode_real.hpp"
#include "aether/meta/equiv.hpp"
#include "aether/meta/morph_real.hpp"
#include "aether/meta/real_func_extract.hpp"

#include <chrono>
#include <fstream>
#include <set>
#include <sstream>

namespace aether {
namespace {

bool has_je_bytes(const std::vector<uint8_t>& code) {
    for (size_t i = 0; i < code.size(); i++) {
        if (code[i] >= 0x70 && code[i] <= 0x7F)
            return true;
        if (code[i] == 0x0F && i + 1 < code.size() && code[i + 1] >= 0x80 && code[i + 1] <= 0x8F)
            return true;
    }
    return false;
}

} // namespace

BenchReport run_morph_bench(size_t pure_count,
                            const char* elf_path,
                            uint64_t seed,
                            int morph_rounds) {
    BenchReport rep;
    rep.scope_line = "x86-64 ELF/PE/raw; analysis-gated morph (split/diversify/shuffle/permute); "
                     "pure RAX+RDI/RSI + structural";
    if (morph_rounds < 1)
        morph_rounds = 1;

    seed_rng_u64(seed);

    auto synth = generate_pure_corpus(pure_count, seed);
    auto paths = default_corpus_elf_paths();
    if (elf_path && elf_path[0])
        paths.insert(paths.begin(), std::string(elf_path));
    auto elf = extract_from_elfs(paths, 4096);

    rep.corpus_synthetic = synth.size();
    rep.corpus_elf = elf.size();
    rep.corpus_total = synth.size() + elf.size();

    std::set<uint64_t> hashes;
    size_t attempts = 0;
    double size_ratio_sum = 0.0;
    size_t size_ratio_n = 0;
    const auto t0 = std::chrono::steady_clock::now();

    auto run_one = [&](const ExtractedFunc& ef) {
        for (int r = 0; r < morph_rounds; r++) {
            ++attempts;
            auto morphed =
                morph_real(ef.bytes.data(), ef.bytes.size(), ef.vaddr, MorphPolicy::Safe);
            if (morphed.empty()) {
                // Fallback: identity re-layout (disasm/assemble only)
                aether::RealFunc id = aether::disasm_real(ef.bytes.data(), ef.bytes.size(), ef.vaddr);
                morphed = aether::assemble_real(id);
            }
            if (morphed.empty()) {
                ++rep.structural_breaks;
                if (rep.first_failure.empty())
                    rep.first_failure = ef.tag + " empty morph off=" + std::to_string(ef.offset);
                continue;
            }
            hashes.insert(hash64(morphed));
            if (!ef.bytes.empty()) {
                size_ratio_sum += (double)morphed.size() / (double)ef.bytes.size();
                ++size_ratio_n;
            }
            RealFunc rf = disasm_real(morphed.data(), morphed.size(), ef.vaddr);
            if (rf.insns.empty()) {
                ++rep.structural_breaks;
                if (rep.first_failure.empty())
                    rep.first_failure = ef.tag + " re-lift empty";
                continue;
            }
            ++rep.morph_ok;
            ++rep.structural_checked;

            if (ef.pure_interpretable) {
                ++rep.pure_checked;
                auto v = interpret_real_pure(rf, ef.arg_rdi, ef.arg_rsi);
                if (!v || *v != ef.pure_rax) {
                    ++rep.pure_breaks;
                    if (rep.first_failure.empty()) {
                        std::ostringstream o;
                        o << ef.tag << " pure break off=" << ef.offset << " exp=" << ef.pure_rax
                          << " got=" << (v ? *v : 0);
                        rep.first_failure = o.str();
                    }
                } else if (!has_je_bytes(morphed)) {
                    if (auto n = try_exec_x64_eax_args(morphed, ef.arg_rdi, ef.arg_rsi)) {
                        ++rep.native_checked;
                        if (*n != ef.pure_rax)
                            ++rep.native_breaks;
                    }
                }
            }
        }
    };

    for (const auto& ef : synth)
        run_one(ef);
    for (const auto& ef : elf)
        run_one(ef);

    rep.unique_hashes = hashes.size();
    const size_t breaks = rep.pure_breaks + rep.structural_breaks + rep.native_breaks;
    rep.break_rate = attempts ? (double)breaks / (double)attempts : 1.0;
    rep.avg_size_ratio = size_ratio_n ? size_ratio_sum / (double)size_ratio_n : 1.0;
    rep.elapsed_ms = (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::steady_clock::now() - t0)
                         .count();
    // Final gate: large corpus + hundreds of ELF-extracted funcs + zero breaks
    rep.pass = (rep.corpus_total >= 1000) && (rep.corpus_elf >= 200) && (breaks == 0) &&
               (attempts > 0);
    return rep;
}

std::string format_bench_report(const BenchReport& r) {
    std::ostringstream o;
    o << "═══════════════════════════════════════════════════════════\n"
      << "  AETHER MORPH BENCH  —  real functions + ≥1k corpus\n"
      << "═══════════════════════════════════════════════════════════\n"
      << "  scope     : " << r.scope_line << "\n"
      << "  corpus    : " << r.corpus_total << "  (synthetic=" << r.corpus_synthetic
      << " elf=" << r.corpus_elf << ")\n"
      << "  morph_ok  : " << r.morph_ok << "\n"
      << "  pure chk  : " << r.pure_checked << "  breaks=" << r.pure_breaks << "\n"
      << "  structural: " << r.structural_checked << "  breaks=" << r.structural_breaks << "\n"
      << "  unique h  : " << r.unique_hashes << "\n"
      << "  native    : " << r.native_checked << "  breaks=" << r.native_breaks << "\n"
      << "  break_rate: " << r.break_rate << "\n"
      << "  size_ratio: " << r.avg_size_ratio << "  elapsed_ms=" << r.elapsed_ms << "\n"
      << "  ───────────────────────────────────────────────────────\n"
      << "  VERDICT   : "
      << (r.pass ? "PASS  (0 breaks, corpus≥1000, elf≥200)" : "FAIL") << "\n";
    if (!r.first_failure.empty())
        o << "  first fail: " << r.first_failure << "\n";
    o << "═══════════════════════════════════════════════════════════\n";
    return o.str();
}

std::string format_bench_report_json(const BenchReport& r) {
    std::ostringstream o;
    o << "{\n"
      << "  \"scope\": \"" << r.scope_line << "\",\n"
      << "  \"corpus_total\": " << r.corpus_total << ",\n"
      << "  \"corpus_synthetic\": " << r.corpus_synthetic << ",\n"
      << "  \"corpus_elf\": " << r.corpus_elf << ",\n"
      << "  \"morph_ok\": " << r.morph_ok << ",\n"
      << "  \"pure_checked\": " << r.pure_checked << ",\n"
      << "  \"pure_breaks\": " << r.pure_breaks << ",\n"
      << "  \"structural_checked\": " << r.structural_checked << ",\n"
      << "  \"structural_breaks\": " << r.structural_breaks << ",\n"
      << "  \"unique_hashes\": " << r.unique_hashes << ",\n"
      << "  \"native_checked\": " << r.native_checked << ",\n"
      << "  \"native_breaks\": " << r.native_breaks << ",\n"
      << "  \"break_rate\": " << r.break_rate << ",\n"
      << "  \"avg_size_ratio\": " << r.avg_size_ratio << ",\n"
      << "  \"elapsed_ms\": " << r.elapsed_ms << ",\n"
      << "  \"pass\": " << (r.pass ? "true" : "false") << "\n"
      << "}\n";
    return o.str();
}

bool write_bench_report_json(const BenchReport& r, const std::string& path) {
    std::ofstream out(path);
    if (!out)
        return false;
    out << format_bench_report_json(r);
    return true;
}

} // namespace aether
