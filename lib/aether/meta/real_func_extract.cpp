/**
 * SPDX-License-Identifier: LicenseRef-Aether-Research
 * Copyright (c) 2024–2026 Aether Research Project contributors.
 *
 * Licensed under the Aether Research License in the repository root file
 * LICENSE. Authorized lab / lawful private use only as defined there.
 * By using this file you accept LICENSE. See docs/LICENSE_ACCEPTANCE.md.
 */

#include "aether/meta/real_func_extract.hpp"

#include "aether/meta/decode_real.hpp"
#include "aether/meta/elf_view.hpp"
#include "aether/meta/morph_real.hpp"
#include "aether/meta/pe_view.hpp"

#include <cstring>

namespace aether {
namespace {

void classify_pure(ExtractedFunc& ef) {
    RealFunc f = disasm_real(ef.bytes.data(), ef.bytes.size(), ef.vaddr);
    auto v = interpret_real_pure(f, ef.arg_rdi, ef.arg_rsi);
    if (v) {
        ef.pure_interpretable = true;
        ef.pure_rax = *v;
    }
}

void emit_u32(std::vector<uint8_t>& o, uint32_t v) {
    o.push_back((uint8_t)v);
    o.push_back((uint8_t)(v >> 8));
    o.push_back((uint8_t)(v >> 16));
    o.push_back((uint8_t)(v >> 24));
}

/** Build one pure function from a deterministic template index. */
std::vector<uint8_t> synth_one(uint32_t i) {
    std::vector<uint8_t> c;
    const uint32_t kind = i % 9;
    const uint32_t imm = (i * 0x9E3779B9u) & 0xFFu; // small imm 0..255
    switch (kind) {
    case 0: // mov eax, imm; ret
        c.push_back(0xB8);
        emit_u32(c, imm);
        c.push_back(0xC3);
        break;
    case 1: // xor rax; mov eax, k; ret  (k small)
        c.insert(c.end(), {0x48, 0x31, 0xC0});
        c.push_back(0xB8);
        emit_u32(c, imm & 7u);
        c.push_back(0xC3);
        break;
    case 2: { // xor; inc × n; ret
        c.insert(c.end(), {0x48, 0x31, 0xC0});
        const int n = (int)(imm % 6);
        for (int k = 0; k < n; k++)
            c.insert(c.end(), {0x48, 0xFF, 0xC0});
        c.push_back(0xC3);
        break;
    }
    case 3: // mov; push; pop; ret
        c.push_back(0xB8);
        emit_u32(c, imm ? imm : 1u);
        c.insert(c.end(), {0x50, 0x58, 0xC3});
        break;
    case 4: // xor; jmp +3; nops; ret → 0
        c.insert(c.end(), {0x48, 0x31, 0xC0, 0xEB, 0x03, 0x90, 0x90, 0x90, 0xC3});
        break;
    case 5: // xor; mov 1; nops; ret
        c.insert(c.end(), {0x48, 0x31, 0xC0, 0xB8, 0x01, 0x00, 0x00, 0x00, 0x90, 0x90, 0xC3});
        break;
    case 6: // xor; mov imm; inc; ret
        c.insert(c.end(), {0x48, 0x31, 0xC0});
        c.push_back(0xB8);
        emit_u32(c, imm & 15u);
        c.insert(c.end(), {0x48, 0xFF, 0xC0, 0xC3});
        break;
    case 7: // mov eax, edi; ret  (SysV arg0)
        c.insert(c.end(), {0x89, 0xF8, 0xC3});
        break;
    default: // mov eax, edi; add eax, esi; ret
        c.insert(c.end(), {0x89, 0xF8, 0x01, 0xF0, 0xC3});
        break;
    }
    return c;
}

} // namespace

std::vector<ExtractedFunc> extract_real_functions(const uint8_t* code,
                                                  size_t len,
                                                  uint64_t base_vaddr,
                                                  size_t min_len,
                                                  size_t max_len,
                                                  size_t max_funcs) {
    std::vector<ExtractedFunc> out;
    if (!code || len < min_len)
        return out;

    // Prefer starts at 0 and after each C3 (simple function boundary heuristic).
    std::vector<size_t> starts;
    starts.push_back(0);
    for (size_t i = 0; i + 1 < len; i++) {
        if (code[i] == 0xC3)
            starts.push_back(i + 1);
    }

    for (size_t s : starts) {
        if (out.size() >= max_funcs)
            break;
        if (s >= len)
            continue;
        // Find first ret within max_len
        for (size_t e = s + min_len - 1; e < len && e < s + max_len; e++) {
            if (code[e] != 0xC3)
                continue;
            size_t n = e - s + 1;
            if (n < min_len)
                continue;
            ExtractedFunc ef;
            ef.offset = s;
            ef.vaddr = base_vaddr + s;
            ef.bytes.assign(code + s, code + s + n);
            ef.tag = "elf";
            // Skip trivial single-ret
            if (n == 1)
                break;
            // Must decode as at least 1 insn with Zydis
            RealFunc rf = disasm_real(ef.bytes.data(), ef.bytes.size(), ef.vaddr);
            if (rf.insns.size() < 2)
                break;
            classify_pure(ef);
            out.push_back(std::move(ef));
            break; // one function per start
        }
    }
    return out;
}

std::vector<ExtractedFunc> generate_pure_corpus(size_t count, uint64_t seed) {
    std::vector<ExtractedFunc> out;
    out.reserve(count);
    for (size_t i = 0; i < count; i++) {
        uint32_t idx = (uint32_t)(i + (seed & 0xFFFFu));
        ExtractedFunc ef;
        ef.offset = i;
        ef.vaddr = 0x400000 + i * 0x40;
        ef.bytes = synth_one(idx);
        ef.tag = "synthetic";
        const uint32_t kind = idx % 9;
        if (kind == 7) {
            ef.arg_rdi = 3 + (i % 50);
            ef.arg_rsi = 0;
        } else if (kind == 8) {
            ef.arg_rdi = 2 + (i % 20);
            ef.arg_rsi = 5 + (i % 11);
        }
        classify_pure(ef);
        out.push_back(std::move(ef));
    }
    return out;
}

std::vector<ExtractedFunc> extract_from_elf(const std::string& path, size_t max_funcs) {
    // Try ELF then PE (industry multi-format extract)
    ElfTextRegion reg = load_elf64_text(path);
    if (!reg.ok)
        reg = load_elf64_text("../" + path);
    if (!reg.ok)
        reg = load_elf64_text("corpus/" + path);
    if (reg.ok) {
        const uint8_t* p = elf_text_data(reg);
        if (p && reg.size >= 8) {
            auto v = extract_real_functions(p, reg.size, reg.vaddr, 3, 96, max_funcs);
            for (auto& ef : v)
                ef.tag = "elf:" + path;
            return v;
        }
    }
    PeTextRegion pe = load_pe64_text(path);
    if (!pe.ok)
        pe = load_pe64_text("../" + path);
    if (!pe.ok)
        pe = load_pe64_text("corpus/" + path);
    const uint8_t* pp = pe_text_data(pe);
    if (!pe.ok || !pp || pe.size < 8)
        return {};
    auto v = extract_real_functions(pp, pe.size, pe.vaddr, 3, 96, max_funcs);
    for (auto& ef : v)
        ef.tag = "pe:" + path;
    return v;
}

std::vector<ExtractedFunc> extract_from_elfs(const std::vector<std::string>& paths,
                                             size_t max_funcs) {
    std::vector<ExtractedFunc> out;
    for (const auto& p : paths) {
        if (out.size() >= max_funcs)
            break;
        auto part = extract_from_elf(p, max_funcs - out.size());
        out.insert(out.end(), part.begin(), part.end());
    }
    return out;
}

std::vector<std::string> default_corpus_elf_paths() {
    return {
        "victim_clean",
        "corpus/real_corpus.elf",
        "corpus/real_corpus.pe",
        "corpus/third_party/busybox",
        "../victim_clean",
        "../corpus/real_corpus.elf",
        "../corpus/real_corpus.pe",
        "../corpus/third_party/busybox",
    };
}

} // namespace aether
