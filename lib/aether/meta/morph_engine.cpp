/**
 * SPDX-License-Identifier: LicenseRef-Aether-Research
 * Copyright (c) 2024–2026 Aether Research Project contributors.
 *
 * Licensed under the Aether Research License in the repository root file
 * LICENSE. Authorized lab / lawful private use only as defined there.
 * By using this file you accept LICENSE. See docs/LICENSE_ACCEPTANCE.md.
 */

#include "aether/meta/morph_engine.hpp"

#include "aether/common/rng.hpp"
#include "aether/meta/binary_rewrite.hpp"
#include "aether/meta/decode_real.hpp"
#include "aether/meta/elf_view.hpp"
#include "aether/meta/pe_view.hpp"
#include "aether/meta/equiv.hpp"
#include "aether/meta/x64_sim.hpp"
#include "aether/meta/real_func_extract.hpp"

#include <sstream>

namespace aether {
namespace {

bool multi_input_ok(const RealFunc& in_f, const RealFunc& out_f) {
    // Prefer full stack/reg simulator (host-independent industry oracle)
    size_t n = 0;
    if (sim_multi_input_equiv(in_f, out_f, &n) && n > 0)
        return true;
    // Fallback classic pure interpret
    static const uint64_t kPairs[][2] = {
        {0, 0}, {1, 2}, {0xFFu, 0xAAu}, {7, 0}, {0, 9}, {0x1234u, 0x5678u},
    };
    size_t checked = 0;
    for (const auto& p : kPairs) {
        auto exp = interpret_real_pure(in_f, p[0], p[1]);
        if (!exp)
            continue;
        auto got = interpret_real_pure(out_f, p[0], p[1]);
        if (!got || *got != *exp)
            return false;
        ++checked;
    }
    return checked > 0;
}

/** Native multi-input differential (x86-64 host only; no-op elsewhere). */
bool native_multi_input_ok(const std::vector<uint8_t>& in_bytes,
                           const std::vector<uint8_t>& out_bytes,
                           size_t* checked_out,
                           size_t* breaks_out) {
    size_t checked = 0, breaks = 0;
    static const uint64_t kPairs[][2] = {
        {0, 0}, {1, 2}, {0xFFu, 0xAAu}, {3, 5},
    };
    for (const auto& p : kPairs) {
        auto exp = try_exec_x64_eax_args(in_bytes.data(), in_bytes.size(), p[0], p[1]);
        if (!exp)
            continue;
        auto got = try_exec_x64_eax_args(out_bytes.data(), out_bytes.size(), p[0], p[1]);
        ++checked;
        if (!got || *got != *exp)
            ++breaks;
    }
    if (checked_out)
        *checked_out = checked;
    if (breaks_out)
        *breaks_out = breaks;
    return breaks == 0 && checked > 0;
}

} // namespace

const char* morph_stage_name(MorphStage s) {
    switch (s) {
    case MorphStage::Lift:
        return "lift";
    case MorphStage::Analyze:
        return "analyze";
    case MorphStage::Diversify:
        return "diversify";
    case MorphStage::Expand:
        return "expand";
    case MorphStage::Shuffle:
        return "shuffle";
    case MorphStage::Split:
        return "split";
    case MorphStage::Permute:
        return "permute";
    case MorphStage::Assemble:
        return "assemble";
    case MorphStage::Verify:
        return "verify";
    default:
        return "unknown";
    }
}

MorphEngine::MorphEngine(MorphEngineConfig cfg) : cfg_(std::move(cfg)) {}

void MorphEngine::set_config(const MorphEngineConfig& cfg) {
    cfg_ = cfg;
}

const MorphEngineConfig& MorphEngine::config() const {
    return cfg_;
}

MorphEngineResult MorphEngine::morph(const std::vector<uint8_t>& code) const {
    return morph(code.data(), code.size());
}

MorphEngineResult MorphEngine::morph(const uint8_t* code, size_t len) const {
    MorphEngineResult r;
    r.bytes_in = len;
    if (!code || !len) {
        r.error = "empty input";
        return r;
    }

    r.stages_run.push_back(morph_stage_name(MorphStage::Lift));
    RealFunc f = disasm_real(code, len, cfg_.base_address);
    if (f.insns.empty()) {
        r.error = "lift failed";
        return r;
    }
    r.insns_in = f.insns.size();
    r.blocks_in = f.blocks.size();

    r.stages_run.push_back(morph_stage_name(MorphStage::Analyze));
    r.cfg_resolved = real_cfg_edges_resolved(f);
    r.may_permute = real_may_permute_blocks(f);
    r.has_memory = real_func_has_memory(f);
    r.has_calls = real_func_has_calls(f);
    r.regs_only = real_func_regs_only(f);

    std::optional<uint32_t> expected;
    if (cfg_.has_expected_rax)
        expected = cfg_.expected_rax;
    else if (cfg_.verify_pure)
        expected = interpret_real_pure(f, cfg_.arg_rdi, cfg_.arg_rsi);

    if (cfg_.require_pure && !expected) {
        r.error = "requires pure-interpretable input";
        return r;
    }

    MorphPolicy pol = cfg_.policy;
    int lab_passes = cfg_.extra_lab_passes;
    bool multi = cfg_.multi_input_verify;
    bool want_native = cfg_.verify_native;

    if (cfg_.product == ProductMode::IndustryExperimental) {
        if (pol == MorphPolicy::Safe)
            pol = MorphPolicy::Lab;
        if (lab_passes < 2)
            lab_passes = 3;
    } else if (cfg_.product == ProductMode::Industry) {
        if (pol == MorphPolicy::Safe)
            pol = MorphPolicy::Lab;
        if (lab_passes < 2)
            lab_passes = 2;
        multi = true;
        want_native = true;
    }

    RealFunc f_in = f;
    std::vector<uint8_t> in_copy(code, code + len);

    if (cfg_.size_fit) {
        r.stages_run.push_back("size_fit");
        r.stages_run.push_back(morph_stage_name(MorphStage::Diversify));
        real_diversify_encodings(f);
        r.stages_run.push_back(morph_stage_name(MorphStage::Shuffle));
        real_safe_shuffle_insns(f);
        r.stages_run.push_back(morph_stage_name(MorphStage::Assemble));
        r.bytes = assemble_real(f);
        if (r.bytes.empty() || r.bytes.size() > len) {
            f = disasm_real(code, len, cfg_.base_address);
            real_diversify_encodings(f);
            r.bytes = assemble_real(f);
        }
        if (r.bytes.empty() || r.bytes.size() > len)
            r.bytes.assign(code, code + len);

        r.bytes_out = r.bytes.size();
        RealFunc outf = disasm_real(r.bytes.data(), r.bytes.size(), cfg_.base_address);
        r.insns_out = outf.insns.size();
        r.blocks_out = outf.blocks.size();
        r.structural_ok = !outf.insns.empty();
        r.stages_run.push_back(morph_stage_name(MorphStage::Verify));
        if (!r.structural_ok) {
            r.error = "size_fit structural fail";
            return r;
        }
        if (expected) {
            auto got = interpret_real_pure(outf, cfg_.arg_rdi, cfg_.arg_rsi);
            if (!got || *got != *expected) {
                r.bytes.assign(code, code + len);
                outf = disasm_real(r.bytes.data(), r.bytes.size(), cfg_.base_address);
                got = interpret_real_pure(outf, cfg_.arg_rdi, cfg_.arg_rsi);
                if (!got || *got != *expected) {
                    r.error = "size_fit pure verify break";
                    r.ok = false;
                    return r;
                }
            }
            r.pure_verified = true;
            r.pure_rax = *got;
            if (multi && !multi_input_ok(f_in, outf)) {
                r.bytes.assign(code, code + len);
                outf = disasm_real(r.bytes.data(), r.bytes.size(), cfg_.base_address);
                r.pure_verified = true;
            }
        }
        r.ok = true;
        return r;
    }

    if (pol == MorphPolicy::Identity) {
        r.stages_run.push_back(morph_stage_name(MorphStage::Assemble));
        r.bytes = assemble_real(f);
        if (r.bytes.empty()) {
            r.error = "assemble identity failed";
            return r;
        }
    } else {
        r.stages_run.push_back(morph_stage_name(MorphStage::Split));
        real_split_blocks(f, 6);

        r.stages_run.push_back(morph_stage_name(MorphStage::Diversify));
        real_diversify_encodings(f);

        r.stages_run.push_back(morph_stage_name(MorphStage::Expand));
        real_expand_nops(f, 1, 4);

        r.stages_run.push_back(morph_stage_name(MorphStage::Shuffle));
        real_safe_shuffle_insns(f);

        if (real_may_permute_blocks(f)) {
            r.stages_run.push_back(morph_stage_name(MorphStage::Permute));
            real_permute_blocks(f);
        }

        r.stages_run.push_back(morph_stage_name(MorphStage::Diversify));
        real_diversify_encodings(f);
        r.stages_run.push_back(morph_stage_name(MorphStage::Expand));
        real_expand_nops(f, 0, 2);
        r.stages_run.push_back(morph_stage_name(MorphStage::Shuffle));
        real_safe_shuffle_insns(f);

        if (pol == MorphPolicy::Lab) {
            const int passes = lab_passes < 1 ? 1 : lab_passes;
            for (int p = 0; p < passes; p++) {
                real_split_blocks(f, 5);
                real_expand_nops(f, 1, 3);
                if (real_may_permute_blocks(f))
                    real_permute_blocks(f);
                real_safe_shuffle_insns(f);
                real_diversify_encodings(f);
            }
        }

        if ((cfg_.product == ProductMode::IndustryExperimental ||
             cfg_.product == ProductMode::Industry) &&
            real_may_permute_blocks(f)) {
            r.stages_run.push_back("industry_extra");
            real_expand_nops(f, 2, 5);
            real_permute_blocks(f);
            real_safe_shuffle_insns(f);
            real_diversify_encodings(f);
        }

        r.stages_run.push_back(morph_stage_name(MorphStage::Assemble));
        r.bytes = assemble_real(f);
        if (r.bytes.empty() && cfg_.allow_identity_fallback) {
            RealFunc id = disasm_real(code, len, cfg_.base_address);
            r.bytes = assemble_real(id);
        }
        if (r.bytes.empty()) {
            r.error = "assemble failed";
            return r;
        }
    }

    r.bytes_out = r.bytes.size();
    if (cfg_.max_size_ratio > 0 && len > 0) {
        double ratio = (double)r.bytes_out / (double)len;
        if (ratio > cfg_.max_size_ratio) {
            r.error = "size ratio exceeded";
            r.ok = false;
            return r;
        }
    }

    RealFunc outf = disasm_real(r.bytes.data(), r.bytes.size(), cfg_.base_address);
    r.insns_out = outf.insns.size();
    r.blocks_out = outf.blocks.size();
    r.structural_ok = !outf.insns.empty();

    r.stages_run.push_back(morph_stage_name(MorphStage::Verify));
    if (cfg_.require_structural && !r.structural_ok) {
        r.error = "structural re-lift failed";
        r.ok = false;
        return r;
    }

    const bool hard_pure =
        (cfg_.product == ProductMode::Lab || cfg_.product == ProductMode::Industry);

    if (expected) {
        if (multi) {
            if (!multi_input_ok(f_in, outf)) {
                if (hard_pure) {
                    r.error = "multi-input pure verify break";
                    r.ok = false;
                    return r;
                }
                r.error = "industry experimental: multi-input break (best-effort kept)";
                r.pure_verified = false;
                r.ok = !r.bytes.empty();
                return r;
            }
            r.multi_input_checked = 6;
            r.pure_verified = true;
            r.pure_rax = *expected;
        } else {
            auto got = interpret_real_pure(outf, cfg_.arg_rdi, cfg_.arg_rsi);
            if (!got || *got != *expected) {
                if (hard_pure) {
                    r.error = "pure verify break";
                    r.ok = false;
                    return r;
                }
                r.error = "industry: pure verify break (best-effort output kept)";
                r.pure_verified = false;
                r.ok = !r.bytes.empty();
                return r;
            }
            r.pure_verified = true;
            r.pure_rax = *got;
        }
    }

    // Industry: native multi-input when host is x86-64 (skipped elsewhere)
    if (want_native && expected) {
        size_t nc = 0, nb = 0;
        bool nok = native_multi_input_ok(in_copy, r.bytes, &nc, &nb);
        r.native_checked = nc;
        r.native_breaks = nb;
        if (nc > 0 && !nok) {
            if (cfg_.product == ProductMode::Industry) {
                r.error = "native multi-input verify break";
                r.ok = false;
                return r;
            }
            // experimental: keep output
            r.error = "native break (best-effort kept)";
        }
    }

    r.ok = true;
    return r;
}

std::vector<MorphEngineResult>
MorphEngine::morph_batch(const std::vector<std::vector<uint8_t>>& inputs, bool stop_on_fail) const {
    std::vector<MorphEngineResult> out;
    out.reserve(inputs.size());
    for (const auto& in : inputs) {
        auto r = morph(in);
        out.push_back(std::move(r));
        if (stop_on_fail && !out.back().ok)
            break;
    }
    return out;
}

MorphBatchReport summarize_batch(const std::vector<MorphEngineResult>& results) {
    MorphBatchReport rep;
    rep.jobs = results.size();
    double ratio_sum = 0;
    size_t ratio_n = 0;
    for (const auto& r : results) {
        if (r.ok) {
            ++rep.ok;
            if (r.pure_verified)
                ++rep.pure_verified;
            if (r.bytes_in > 0) {
                ratio_sum += (double)r.bytes_out / (double)r.bytes_in;
                ++ratio_n;
            }
        } else {
            ++rep.fail;
            if (r.error.find("verify") != std::string::npos)
                ++rep.pure_breaks;
        }
    }
    rep.avg_size_ratio = ratio_n ? ratio_sum / (double)ratio_n : 1.0;
    rep.pass = (rep.jobs > 0) && (rep.fail == 0) && (rep.pure_breaks == 0);
    return rep;
}

std::string format_batch_report(const MorphBatchReport& r) {
    std::ostringstream o;
    o << "MORPH ENGINE BATCH  jobs=" << r.jobs << " ok=" << r.ok << " fail=" << r.fail
      << " pure_verified=" << r.pure_verified << " pure_breaks=" << r.pure_breaks
      << " size_ratio=" << r.avg_size_ratio << " pass=" << (r.pass ? "YES" : "NO") << "\n";
    return o.str();
}

std::string format_batch_report_json(const MorphBatchReport& r) {
    std::ostringstream o;
    o << "{\n"
      << "  \"jobs\": " << r.jobs << ",\n"
      << "  \"ok\": " << r.ok << ",\n"
      << "  \"fail\": " << r.fail << ",\n"
      << "  \"pure_verified\": " << r.pure_verified << ",\n"
      << "  \"pure_breaks\": " << r.pure_breaks << ",\n"
      << "  \"avg_size_ratio\": " << r.avg_size_ratio << ",\n"
      << "  \"pass\": " << (r.pass ? "true" : "false") << "\n"
      << "}\n";
    return o.str();
}

bool industry_framework_selftest(size_t pure_samples) {
    if (pure_samples < 8)
        pure_samples = 8;
    seed_rng_u64(0x1A2B3C4Du);
    auto corpus = generate_pure_corpus(pure_samples, 0x1A2B3C4Du);

    MorphEngineConfig base;
    base.verify_pure = true;

    for (const auto& ef : corpus) {
        if (!ef.pure_interpretable)
            continue;
        for (MorphPolicy pol : {MorphPolicy::Identity, MorphPolicy::Safe, MorphPolicy::Lab}) {
            MorphEngineConfig cfg = base;
            cfg.policy = pol;
            cfg.arg_rdi = ef.arg_rdi;
            cfg.arg_rsi = ef.arg_rsi;
            cfg.has_expected_rax = true;
            cfg.expected_rax = ef.pure_rax;
            MorphEngine eng(cfg);
            auto r = eng.morph(ef.bytes);
            if (!r.ok)
                return false;
            if (!r.pure_verified || r.pure_rax != ef.pure_rax)
                return false;
        }
    }
    return true;
}

bool industry_product_selftest(size_t pure_samples) {
    if (pure_samples < 8)
        pure_samples = 8;
    seed_rng_u64(0x5151u);
    auto corpus = generate_pure_corpus(pure_samples, 0x5151u);
    size_t ok = 0;
    for (const auto& ef : corpus) {
        if (!ef.pure_interpretable)
            continue;
        MorphEngineConfig cfg;
        cfg.product = ProductMode::Industry;
        cfg.policy = MorphPolicy::Safe;
        cfg.verify_pure = true;
        cfg.multi_input_verify = true;
        cfg.verify_native = true;
        cfg.domain = MorphDomain::RegsFlags;
        cfg.arg_rdi = ef.arg_rdi;
        cfg.arg_rsi = ef.arg_rsi;
        MorphEngine eng(cfg);
        auto r = eng.morph(ef.bytes);
        if (!r.ok || !r.pure_verified)
            return false;
        // native_breaks must be 0 when any native checks ran
        if (r.native_breaks > 0)
            return false;
        ++ok;
    }
    return ok >= 8;
}

bool industry_finish_selftest() {
    if (!industry_framework_selftest(64))
        return false;
    if (!industry_product_selftest(64))
        return false;
    if (!industry_rewrite_selftest())
        return false;

    // Scale pure industry morph with multi-input + host-independent "native" oracle
    seed_rng_u64(0xF1A15Full);
    auto big = generate_pure_corpus(512, 0xF1A15Full);
    MorphEngineConfig cfg;
    cfg.product = ProductMode::Industry;
    cfg.policy = MorphPolicy::Safe;
    cfg.verify_pure = true;
    cfg.multi_input_verify = true;
    cfg.verify_native = true;
    size_t ok = 0;
    size_t native_ok = 0;
    for (const auto& ef : big) {
        if (!ef.pure_interpretable)
            continue;
        cfg.arg_rdi = ef.arg_rdi;
        cfg.arg_rsi = ef.arg_rsi;
        MorphEngine eng(cfg);
        auto r = eng.morph(ef.bytes);
        if (!r.ok || !r.pure_verified)
            return false;
        if (r.native_breaks > 0)
            return false;
        if (r.native_checked > 0)
            ++native_ok;
        ++ok;
    }
    if (ok < 200)
        return false;
    // With sim-backed try_exec, native_checked must be > 0 on all hosts
    if (native_ok < 50)
        return false;

    // Production-like ELF rewrite proof
    // real_corpus: require actual byte-level morphs
    if (load_elf64_text("corpus/real_corpus.elf").ok) {
        MorphEngineConfig c;
        c.policy = MorphPolicy::Safe;
        c.verify_pure = false;
        auto rr = rewrite_binary_file("corpus/real_corpus.elf", "", c);
        if (!rr.ok || rr.funcs_seen < 5 || rr.funcs_morphed == 0)
            return false;
    }
    // victim_clean: rewrite must process functions (morph or size-stable rewrite path)
    if (load_elf64_text("victim_clean").ok) {
        MorphEngineConfig c;
        c.policy = MorphPolicy::Safe;
        c.verify_pure = false;
        auto rr = rewrite_binary_file("victim_clean", "", c);
        if (!rr.ok || rr.funcs_seen < 1)
            return false;
        if ((rr.funcs_morphed + rr.funcs_identity + rr.funcs_trampolined) == 0)
            return false;
    }
    if (load_pe64_text("corpus/real_corpus.pe").ok) {
        MorphEngineConfig c;
        c.policy = MorphPolicy::Safe;
        c.verify_pure = false;
        auto rr = rewrite_binary_file("corpus/real_corpus.pe", "", c);
        if (!rr.ok)
            return false;
    }
    return true;
}

} // namespace aether
