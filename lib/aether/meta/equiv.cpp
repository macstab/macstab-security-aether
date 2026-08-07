/**
 * SPDX-License-Identifier: LicenseRef-Aether-Research
 * Copyright (c) 2024–2026 Macstab GmbH.
 *
 * Licensed under the Aether Research License in the repository root file
 * LICENSE. Authorized lab / lawful private use only as defined there.
 * By using this file you accept LICENSE. See docs/LICENSE_ACCEPTANCE.md.
 */

#include "aether/meta/equiv.hpp"

#include "aether/common/rng.hpp"
#include "aether/meta/assemble.hpp"
#include "aether/meta/crypto_cascade.hpp"
#include "aether/meta/decode.hpp"
#include "aether/meta/decode_real.hpp"
#include "aether/meta/elf_view.hpp"
#include "aether/meta/morph_real.hpp"
#include "aether/meta/stages.hpp"
#include "aether/meta/transforms.hpp"
#include "aether/meta/x64_sim.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <set>
#include <sstream>

#if defined(__x86_64__) || defined(_M_X64)
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace aether {
namespace {

constexpr int kMaxStack = 64;
constexpr int kMaxSteps = 100000;

struct SeedCase {
    const char* name;
    std::vector<uint8_t> bytes;
    uint32_t expected;
};

std::vector<SeedCase> corpus() {
    return {
        {"imm7", {0xB8, 0x07, 0x00, 0x00, 0x00, 0xC3}, 7},
        {"xor_mov2_inc",
         {0x48, 0x31, 0xC0, 0xB8, 0x02, 0x00, 0x00, 0x00, 0x48, 0xFF, 0xC0, 0xC3},
         3},
        {"inc5",
         {0x48, 0x31, 0xC0, 0x48, 0xFF, 0xC0, 0x48, 0xFF, 0xC0, 0x48, 0xFF, 0xC0, 0x48, 0xFF, 0xC0,
          0x48, 0xFF, 0xC0, 0xC3},
         5},
        {"push_pop_42", {0xB8, 0x2A, 0x00, 0x00, 0x00, 0x50, 0x58, 0xC3}, 42},
        {"jmp_skip", {0x48, 0x31, 0xC0, 0xEB, 0x03, 0x90, 0x90, 0x90, 0xC3}, 0},
        {"mov1_nops",
         {0x48, 0x31, 0xC0, 0xB8, 0x01, 0x00, 0x00, 0x00, 0x90, 0x90, 0x90, 0x90, 0xC3},
         1},
        {"zero", {0x48, 0x31, 0xC0, 0xB8, 0x00, 0x00, 0x00, 0x00, 0xC3}, 0},
        {"imm17", {0xB8, 0x11, 0x00, 0x00, 0x00, 0xC3}, 17},
    };
}

void record_break(EquivReport& rep,
                  int seed_id,
                  int trial,
                  const char* path,
                  const char* domain,
                  uint32_t expected,
                  uint32_t got,
                  bool failed) {
    ++rep.breaks;
    if (std::string(domain) == "RealRestricted")
        ++rep.real_breaks;
    if (std::string(domain) == "CascadeLeaf")
        ++rep.cascade_breaks;
    if (rep.sample_breaks.size() >= 8)
        return;
    EquivBreak b;
    b.seed_id = seed_id;
    b.trial = trial;
    b.path = path;
    b.domain = domain;
    b.expected = expected;
    b.got = got;
    b.interpret_failed = failed;
    rep.sample_breaks.push_back(std::move(b));
}

/**
 * Native EAX cross-check (x86-64 only).
 * Strict domain: no RAW, no JE (ambient host ZF ≠ interpreter zf=false),
 * interpret agrees, then mmap+CALL.
 */
void maybe_native(EquivReport& rep, const std::vector<uint8_t>& code, uint32_t expected) {
#if defined(__x86_64__) || defined(_M_X64)
    if (code.empty())
        return;
    auto pure = normalize_reachable(code);
    const std::vector<uint8_t>& buf = pure.empty() ? code : pure;
    IRFunc f = disasm(buf.data(), buf.size());
    for (const auto& b : f.blocks) {
        for (const auto& in : b.insns) {
            if (in.op == Op::RAW || in.op == Op::JE)
                return;
        }
    }
    auto g = interpret_rax(f);
    if (!g || *g != expected)
        return;
    if (auto n = try_exec_x64_eax(buf)) {
        ++rep.native_checked;
        if (*n != expected)
            ++rep.native_breaks;
    }
#else
    (void)rep;
    (void)code;
    (void)expected;
#endif
}

void check_ir(EquivReport& rep,
              std::set<uint64_t>& hashes,
              std::set<std::string>& paths,
              int seed_id,
              int trial,
              const char* path,
              IRFunc ir,
              uint32_t expected) {
    paths.insert(path);
    ++rep.trials;
    auto ir_got = interpret_rax(ir);
    if (!ir_got) {
        record_break(rep, seed_id, trial, path, "EduPureRax", expected, 0, true);
        return;
    }
    if (*ir_got != expected) {
        record_break(rep, seed_id, trial, path, "EduPureRax", expected, *ir_got, false);
        return;
    }

    auto code = assemble(ir);
    if (code.empty()) {
        record_break(rep, seed_id, trial, path, "EduPureRax", expected, 0, true);
        return;
    }
    hashes.insert(hash64(code));
    IRFunc again = disasm(code.data(), code.size());
    ++rep.trials;
    auto got = interpret_rax(again);
    if (!got) {
        record_break(rep, seed_id, trial, (std::string(path) + "+codec").c_str(), "EduPureRax",
                     expected, 0, true);
        return;
    }
    if (*got != expected)
        record_break(rep, seed_id, trial, (std::string(path) + "+codec").c_str(), "EduPureRax",
                     expected, *got, false);
    // Dual oracle on IR→assemble path (no trailing junk sled)
    maybe_native(rep, code, expected);
}

void check_bytes(EquivReport& rep,
                 std::set<uint64_t>& hashes,
                 std::set<std::string>& paths,
                 int seed_id,
                 int trial,
                 const char* path,
                 const std::vector<uint8_t>& code,
                 uint32_t expected) {
    paths.insert(path);
    if (code.empty()) {
        record_break(rep, seed_id, trial, path, "EduPureRax", expected, 0, true);
        return;
    }
    hashes.insert(hash64(code));
    // Prefer pure reachable body for interpret (strip trailing NOP sled)
    auto pure = normalize_reachable(code);
    const auto& buf = pure.empty() ? code : pure;
    IRFunc f = disasm(buf.data(), buf.size());
    auto got = interpret_rax(f);
    ++rep.trials;
    if (!got) {
        record_break(rep, seed_id, trial, path, "EduPureRax", expected, 0, true);
        return;
    }
    if (*got != expected)
        record_break(rep, seed_id, trial, path, "EduPureRax", expected, *got, false);
    maybe_native(rep, buf, expected);
}

void check_real(EquivReport& rep,
                std::set<uint64_t>& hashes,
                std::set<std::string>& paths,
                int seed_id,
                int trial,
                const char* path,
                const std::vector<uint8_t>& code,
                uint32_t expected) {
    paths.insert(path);
    ++rep.trials;
    ++rep.real_trials;
    if (code.empty()) {
        record_break(rep, seed_id, trial, path, "RealRestricted", expected, 0, true);
        return;
    }
    hashes.insert(hash64(code));
    RealFunc rf = disasm_real(code.data(), code.size(), 0x1000);
    auto got = interpret_real_pure(rf);
    if (!got) {
        record_break(rep, seed_id, trial, path, "RealRestricted", expected, 0, true);
        return;
    }
    if (*got != expected)
        record_break(rep, seed_id, trial, path, "RealRestricted", expected, *got, false);
    // Native only when educational codec can also model the buffer
    maybe_native(rep, code, expected);
}

} // namespace

std::optional<uint32_t> interpret_rax(const IRFunc& f) {
    if (f.blocks.empty() || f.entry < 0 || f.entry >= (int)f.blocks.size())
        return std::nullopt;

    uint64_t rax = 0;
    uint64_t rcx = 0;
    bool zf = false;
    uint64_t stack[kMaxStack];
    int sp = 0;
    int bi = f.entry;

    for (int steps = 0; steps < kMaxSteps && bi >= 0 && bi < (int)f.blocks.size(); steps++) {
        const IRBlock& b = f.blocks[(size_t)bi];
        for (const auto& in : b.insns) {
            switch (in.op) {
            case Op::NOP:
            case Op::JUNK:
            case Op::XCHG_RAX_RAX:
            case Op::RAW:
                break;
            case Op::CLEAR_RAX:
                rax = 0;
                break;
            case Op::CLEAR_RCX:
                rcx = 0;
                break;
            case Op::MOV_RAX_IMM:
                rax = (uint32_t)in.imm;
                break;
            case Op::MOV_RCX_IMM:
            case Op::SET_STATE:
                rcx = (uint32_t)in.imm;
                break;
            case Op::CMP_STATE:
                zf = ((uint32_t)rcx == (uint32_t)in.imm);
                break;
            case Op::INC_RAX:
                rax = (uint32_t)(rax + 1);
                break;
            case Op::DEC_RAX:
                rax = (uint32_t)(rax - 1);
                break;
            case Op::PUSH_RAX:
                if (sp >= kMaxStack)
                    return std::nullopt;
                stack[sp++] = rax;
                break;
            case Op::POP_RAX:
                if (sp <= 0)
                    return std::nullopt;
                rax = stack[--sp];
                break;
            case Op::RET:
                return (uint32_t)rax;
            case Op::JMP:
                if (in.target < 0 || in.target >= (int)f.blocks.size())
                    return std::nullopt;
                bi = in.target;
                goto next_block;
            case Op::JE:
                if (zf) {
                    if (in.target < 0 || in.target >= (int)f.blocks.size())
                        return std::nullopt;
                    bi = in.target;
                    goto next_block;
                }
                break;
            }
        }
        if (b.fallthrough >= 0 && b.fallthrough < (int)f.blocks.size()) {
            bi = b.fallthrough;
            continue;
        }
        return std::nullopt;
    next_block:;
    }
    return std::nullopt;
}

std::optional<uint32_t> try_exec_x64_eax(const uint8_t* code, size_t len) {
    return try_exec_x64_eax_args(code, len, 0, 0);
}

std::optional<uint32_t> try_exec_x64_eax_args(const uint8_t* code,
                                              size_t len,
                                              uint64_t rdi,
                                              uint64_t rsi) {
    if (!code || !len)
        return std::nullopt;
#if defined(__x86_64__) || defined(_M_X64)
    // Hardware native path
    uint8_t prefix[14];
    prefix[0] = 0x48;
    prefix[1] = 0xC7;
    prefix[2] = 0xC7;
    std::memcpy(prefix + 3, &rdi, 4);
    prefix[7] = 0x48;
    prefix[8] = 0xC7;
    prefix[9] = 0xC6;
    std::memcpy(prefix + 10, &rsi, 4);
    const size_t total = sizeof(prefix) + len;
    size_t page = (size_t)sysconf(_SC_PAGESIZE);
    size_t need = (total + page - 1) & ~(page - 1);
    void* m = mmap(nullptr, need, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (m != MAP_FAILED) {
        std::memcpy(m, prefix, sizeof(prefix));
        std::memcpy(static_cast<uint8_t*>(m) + sizeof(prefix), code, len);
        if (mprotect(m, need, PROT_READ | PROT_EXEC) == 0) {
            using Fn = uint32_t (*)();
            uint32_t eax = reinterpret_cast<Fn>(m)();
            munmap(m, need);
            return eax;
        }
        munmap(m, need);
    }
#endif
    // Industry always-on path: software x86-64 simulator (any host, incl. Apple Silicon)
    return sim_x64_buffer(code, len, rdi, rsi, 0x1000);
}

EquivReport run_equivalence_campaign(uint64_t campaign_seed, int rounds_per_seed) {
    EquivReport rep;
    if (rounds_per_seed < 1)
        rounds_per_seed = 1;

    seed_rng_u64(campaign_seed);
    auto cases = corpus();
    rep.seeds = (int)cases.size();
    rep.domain_summary = "EduPureRax+RealRestricted+CascadeLeaf+RealText";

    std::set<uint64_t> hashes;
    std::set<std::string> paths;

    for (int sid = 0; sid < (int)cases.size(); sid++) {
        const auto& sc = cases[(size_t)sid];

        {
            IRFunc base = disasm(sc.bytes.data(), sc.bytes.size());
            auto g = interpret_rax(base);
            if (!g || *g != sc.expected) {
                record_break(rep, sid, -1, "seed_golden", "EduPureRax", sc.expected, g.value_or(0),
                             !g);
                continue;
            }
            RealFunc rb = disasm_real(sc.bytes.data(), sc.bytes.size(), 0x1000);
            auto rg = interpret_real_pure(rb);
            if (!rg || *rg != sc.expected) {
                record_break(rep, sid, -1, "seed_golden_real", "RealRestricted", sc.expected,
                             rg.value_or(0), !rg);
            }
            maybe_native(rep, sc.bytes, sc.expected);
        }

        for (int t = 0; t < rounds_per_seed; t++) {
            {
                IRFunc ir = disasm(sc.bytes.data(), sc.bytes.size());
                world_class_permute(ir); // includes def-use safe_permute_insns
                diversify_implementation(ir);
                expand(ir);
                safe_permute_insns(ir);
                check_ir(rep, hashes, paths, sid, t, "wc_permute+div+exp", std::move(ir),
                         sc.expected);
            }
            {
                // Def-use gate smoke: independent swaps must preserve RAX
                IRFunc ir = disasm(sc.bytes.data(), sc.bytes.size());
                expand(ir);
                expand(ir);
                safe_permute_insns(ir);
                safe_permute_insns(ir);
                check_ir(rep, hashes, paths, sid, t, "safe_permute_insns", std::move(ir),
                         sc.expected);
            }
            {
                IRFunc ir = disasm(sc.bytes.data(), sc.bytes.size());
                expand(ir);
                expand(ir);
                flatten(ir);
                check_ir(rep, hashes, paths, sid, t, "flatten", std::move(ir), sc.expected);
            }
            for (int m = 0; m < (int)LayerMode::Count; m++) {
                IRFunc ir = disasm(sc.bytes.data(), sc.bytes.size());
                expand(ir);
                apply_layer_mode(ir, static_cast<LayerMode>(m));
                char label[48];
                std::snprintf(label, sizeof(label), "layer:%s",
                              layer_mode_name(static_cast<LayerMode>(m)));
                check_ir(rep, hashes, paths, sid, t, label, std::move(ir), sc.expected);
            }
            {
                auto code = morph_stage(sc.bytes.data(), sc.bytes.size(), nullptr);
                check_bytes(rep, hashes, paths, sid, t, "morph_stage", code, sc.expected);
            }
            {
                auto code = morph_stage_mode(sc.bytes.data(), sc.bytes.size(),
                                             LayerMode::PermuteHeavy, nullptr);
                check_bytes(rep, hashes, paths, sid, t, "morph:PermuteHeavy", code, sc.expected);
            }
            // Byte multi-stage (normalize_reachable after each gen)
            if (t % 2 == 0) {
                auto code =
                    multi_stage_morph(sc.bytes.data(), sc.bytes.size(), nullptr, 2, 3, "equiv");
                check_bytes(rep, hashes, paths, sid, t, "multi_stage", code, sc.expected);
            }
            {
                IRFunc ir = disasm(sc.bytes.data(), sc.bytes.size());
                const int passes = 2 + (t & 1);
                for (int p = 0; p < passes; p++) {
                    (void)apply_random_transform_schedule(ir, 3, 5);
                    diversify_implementation(ir);
                }
                check_ir(rep, hashes, paths, sid, t, "multi_pass_ir", std::move(ir), sc.expected);
            }
            {
                auto code = morph_real_restricted(sc.bytes.data(), sc.bytes.size(), 0x1000);
                check_real(rep, hashes, paths, sid, t, "morph_real_restricted", code, sc.expected);
            }
            if (t % 2 == 0) {
                auto c1 = morph_real_restricted(sc.bytes.data(), sc.bytes.size(), 0x1000);
                auto c2 = morph_real_restricted(c1.data(), c1.size(), 0x1000);
                check_real(rep, hashes, paths, sid, t, "morph_real_x2", c2, sc.expected);
            }
            // Cascade: build onion on pure leaf → peel → same RAX
            {
                paths.insert("cascade_peel");
                ++rep.trials;
                ++rep.cascade_trials;
                std::vector<uint8_t> leaf = sc.bytes;
                auto onion = cascade_build(leaf, 2 + (t & 1), nullptr);
                if (onion.empty() || !cascade_peel(onion, nullptr)) {
                    record_break(rep, sid, t, "cascade_peel", "CascadeLeaf", sc.expected, 0, true);
                } else {
                    hashes.insert(hash64(onion));
                    auto got = interpret_rax(disasm(onion.data(), onion.size()));
                    if (!got)
                        record_break(rep, sid, t, "cascade_peel", "CascadeLeaf", sc.expected, 0,
                                     true);
                    else if (*got != sc.expected)
                        record_break(rep, sid, t, "cascade_peel", "CascadeLeaf", sc.expected, *got,
                                     false);
                }
            }
        }
    }

    // Real ELF .text: morph windows + pure-gadget scan
    {
        paths.insert("real_text");
        ElfTextRegion reg = load_elf64_text("victim_clean");
        if (!reg.ok)
            reg = load_elf64_text("../victim_clean");
        const uint8_t* td = elf_text_data(reg);
        if (reg.ok && td && reg.size > 32) {
            rep.real_text_ok = true;
            const size_t win = 64;
            const size_t step = 32;
            size_t windows = 0;
            for (size_t off = 0; off + 16 < reg.size && windows < 24; off += step) {
                size_t n = std::min(win, reg.size - off);
                auto morphed = morph_real_restricted(td + off, n, reg.vaddr + off);
                ++rep.real_text_windows;
                ++windows;
                if (morphed.empty()) {
                    // empty only if CFG broken on undecodable trash — soft skip
                    continue;
                }
                hashes.insert(hash64(morphed));
                RealFunc rf = disasm_real(morphed.data(), morphed.size(), reg.vaddr + off);
                if (rf.insns.empty()) {
                    record_break(rep, -1, (int)off, "real_text_morph", "RealText", 0, 0, true);
                    rep.real_text_ok = false;
                }
            }
            // Scan .text for pure B8 imm / C3 gadgets and prove morph RAX
            for (size_t i = 0; i + 6 <= reg.size && rep.real_text_windows < 40; i++) {
                if (td[i] == 0xB8 && i + 5 < reg.size && td[i + 5] == 0xC3) {
                    uint32_t imm;
                    std::memcpy(&imm, td + i + 1, 4);
                    std::vector<uint8_t> g(td + i, td + i + 6);
                    auto m = morph_real_restricted(g.data(), g.size(), reg.vaddr + i);
                    ++rep.trials;
                    ++rep.real_trials;
                    paths.insert("real_text_gadget");
                    if (m.empty()) {
                        record_break(rep, -2, (int)i, "real_text_gadget", "RealRestricted", imm, 0,
                                     true);
                        continue;
                    }
                    hashes.insert(hash64(m));
                    auto v = interpret_real_pure(disasm_real(m.data(), m.size(), 0x1000));
                    if (!v || *v != imm)
                        record_break(rep, -2, (int)i, "real_text_gadget", "RealRestricted", imm,
                                     v.value_or(0), !v);
                }
            }
        } else {
            rep.real_text_ok = false;
            record_break(rep, -1, -1, "real_text_load", "RealText", 0, 0, true);
        }
    }

    rep.unique_hashes = (int)hashes.size();
    rep.paths_exercised = (int)paths.size();
    return rep;
}

std::string format_equiv_report(const EquivReport& r) {
    std::ostringstream o;
    o << "═══════════════════════════════════════════════════════════\n"
      << "  AETHER EQUIVALENCE ORACLE  —  COMPLETE GATE\n"
      << "═══════════════════════════════════════════════════════════\n"
      << "  domains           : " << r.domain_summary << "\n"
      << "  corpus seeds      : " << r.seeds << "\n"
      << "  morph checks      : " << r.trials << "\n"
      << "  semantic breaks   : " << r.breaks << "\n"
      << "  real 1B checks    : " << r.real_trials << "  breaks=" << r.real_breaks << "\n"
      << "  cascade peel      : " << r.cascade_trials << "  breaks=" << r.cascade_breaks << "\n"
      << "  real .text wins   : " << r.real_text_windows
      << "  ok=" << (r.real_text_ok ? "yes" : "NO") << "\n"
      << "  unique byte hashes: " << r.unique_hashes << "  (diversity under invariant)\n"
      << "  transform paths   : " << r.paths_exercised << "\n"
      << "  native x64 checks : " << r.native_checked << "  breaks=" << r.native_breaks << "\n"
      << "  ───────────────────────────────────────────────────────\n"
      << "  VERDICT           : " << (r.pass() ? "PASS  (0 breaks)" : "FAIL") << "\n";
    if (!r.sample_breaks.empty()) {
        o << "  sample failures:\n";
        for (const auto& b : r.sample_breaks) {
            o << "    [" << b.domain << "] seed=" << b.seed_id << " trial=" << b.trial
              << " path=" << b.path << " expected=" << b.expected;
            if (b.interpret_failed)
                o << " INTERPRET_FAIL\n";
            else
                o << " got=" << b.got << "\n";
        }
    }
    o << "═══════════════════════════════════════════════════════════\n";
    return o.str();
}

std::string format_equiv_report_json(const EquivReport& r) {
    std::ostringstream o;
    o << "{\n"
      << "  \"domain_summary\": \"" << r.domain_summary << "\",\n"
      << "  \"seeds\": " << r.seeds << ",\n"
      << "  \"trials\": " << r.trials << ",\n"
      << "  \"breaks\": " << r.breaks << ",\n"
      << "  \"real_trials\": " << r.real_trials << ",\n"
      << "  \"real_breaks\": " << r.real_breaks << ",\n"
      << "  \"cascade_trials\": " << r.cascade_trials << ",\n"
      << "  \"cascade_breaks\": " << r.cascade_breaks << ",\n"
      << "  \"real_text_windows\": " << r.real_text_windows << ",\n"
      << "  \"real_text_ok\": " << (r.real_text_ok ? "true" : "false") << ",\n"
      << "  \"unique_hashes\": " << r.unique_hashes << ",\n"
      << "  \"paths_exercised\": " << r.paths_exercised << ",\n"
      << "  \"native_checked\": " << r.native_checked << ",\n"
      << "  \"native_breaks\": " << r.native_breaks << ",\n"
      << "  \"pass\": " << (r.pass() ? "true" : "false") << "\n"
      << "}\n";
    return o.str();
}

} // namespace aether
