/**
 * SPDX-License-Identifier: LicenseRef-Aether-Research
 * Copyright (c) 2024–2026 Aether Research Project contributors.
 *
 * Licensed under the Aether Research License in the repository root file
 * LICENSE. Authorized lab / lawful private use only as defined there.
 * By using this file you accept LICENSE. See docs/LICENSE_ACCEPTANCE.md.
 */

#include "aether/meta/binary_rewrite.hpp"

#include "aether/meta/decode_real.hpp"
#include "aether/meta/elf_view.hpp"
#include "aether/meta/pe_view.hpp"
#include "aether/meta/real_func_extract.hpp"

#include <climits>
#include <cstring>
#include <fstream>
#include <vector>

namespace aether {
namespace {

bool write_file(const std::string& path, const std::vector<uint8_t>& data) {
    std::ofstream o(path, std::ios::binary);
    if (!o)
        return false;
    o.write(reinterpret_cast<const char*>(data.data()), (std::streamsize)data.size());
    return (bool)o;
}

std::vector<uint8_t> encode_jmp_rel32(int32_t rel) {
    std::vector<uint8_t> j = {0xE9, 0, 0, 0, 0};
    std::memcpy(j.data() + 1, &rel, 4);
    return j;
}

/** Append RX blob to ELF64 file; returns file offset of blob and preferred VA. */
bool elf_append_rx(std::vector<uint8_t>& file, const std::vector<uint8_t>& blob,
                   size_t* out_off, uint64_t* out_va) {
    if (file.size() < 64 || file[4] != 2)
        return false;
    // Align to 0x1000
    size_t align = 0x1000;
    size_t pad = (align - (file.size() % align)) % align;
    file.insert(file.end(), pad, 0);
    size_t blob_off = file.size();
    file.insert(file.end(), blob.begin(), blob.end());
    // Pad end
    size_t end_pad = (align - (file.size() % align)) % align;
    file.insert(file.end(), end_pad, 0);

    // Choose VA: high enough (image base style). Use 0x400000 + blob_off for simplicity
    // Better: after last PT_LOAD vaddr+memsz
    uint64_t e_phoff = 0;
    uint16_t e_phentsize = 0, e_phnum = 0;
    std::memcpy(&e_phoff, file.data() + 32, 8);
    std::memcpy(&e_phentsize, file.data() + 54, 2);
    std::memcpy(&e_phnum, file.data() + 56, 2);
    uint64_t max_end = 0x400000;
    for (uint16_t i = 0; i < e_phnum; i++) {
        size_t pho = (size_t)e_phoff + (size_t)i * e_phentsize;
        if (pho + 56 > file.size())
            break;
        uint32_t p_type = 0;
        uint64_t p_vaddr = 0, p_memsz = 0;
        std::memcpy(&p_type, file.data() + pho, 4);
        std::memcpy(&p_vaddr, file.data() + pho + 16, 8);
        std::memcpy(&p_memsz, file.data() + pho + 40, 8);
        if (p_type == 1) {
            uint64_t end = p_vaddr + p_memsz;
            if (end > max_end)
                max_end = end;
        }
    }
    uint64_t blob_va = (max_end + align - 1) & ~(uint64_t)(align - 1);

    // Extend first free PT_LOAD or patch last PF_X LOAD if possible — inject new PHDR hard.
    // Practical approach: extend last PT_LOAD to cover new bytes when it is last in file.
    int last_load = -1;
    for (uint16_t i = 0; i < e_phnum; i++) {
        size_t pho = (size_t)e_phoff + (size_t)i * e_phentsize;
        uint32_t p_type = 0;
        std::memcpy(&p_type, file.data() + pho, 4);
        if (p_type == 1)
            last_load = (int)i;
    }
    if (last_load >= 0) {
        size_t pho = (size_t)e_phoff + (size_t)last_load * e_phentsize;
        uint64_t p_offset = 0, p_vaddr = 0, p_filesz = 0, p_memsz = 0;
        uint32_t p_flags = 0;
        std::memcpy(&p_flags, file.data() + pho + 4, 4);
        std::memcpy(&p_offset, file.data() + pho + 8, 8);
        std::memcpy(&p_vaddr, file.data() + pho + 16, 8);
        std::memcpy(&p_filesz, file.data() + pho + 32, 8);
        std::memcpy(&p_memsz, file.data() + pho + 40, 8);
        // Only extend if this LOAD reaches near previous end of file (before our append)
        // Simpler: set filesz/memsz so offset+filesz covers whole file end
        uint64_t new_filesz = file.size() - p_offset;
        uint64_t new_memsz = new_filesz;
        // Ensure executable
        p_flags |= 1; // PF_X
        std::memcpy(file.data() + pho + 4, &p_flags, 4);
        std::memcpy(file.data() + pho + 32, &new_filesz, 8);
        std::memcpy(file.data() + pho + 40, &new_memsz, 8);
        blob_va = p_vaddr + (blob_off - p_offset);
    }

    if (out_off)
        *out_off = blob_off;
    if (out_va)
        *out_va = blob_va;
    return true;
}

BinaryRewriteResult rewrite_region_whole(std::vector<uint8_t> file, size_t off, size_t size,
                                         uint64_t vaddr, const std::string& name, BinaryFormat fmt,
                                         const MorphEngineConfig& cfg) {
    BinaryRewriteResult r;
    r.format = fmt;
    r.region_offset = off;
    r.region_size = size;
    r.region_name = name;
    if (off + size > file.size() || size == 0) {
        r.error = "bad region";
        return r;
    }
    MorphEngineConfig c = cfg;
    c.base_address = vaddr;
    c.verify_pure = false;
    c.size_fit = true;
    MorphEngine eng(c);
    auto mr = eng.morph(file.data() + off, size);
    if (!mr.ok || mr.bytes.empty() || mr.bytes.size() > size) {
        r.ok = true;
        r.file_bytes = std::move(file);
        r.morphed_size = size;
        r.padded_to = size;
        r.funcs_identity = 1;
        return r;
    }
    std::memcpy(file.data() + off, mr.bytes.data(), mr.bytes.size());
    for (size_t i = mr.bytes.size(); i < size; i++)
        file[off + i] = 0x90;
    r.ok = true;
    r.file_bytes = std::move(file);
    r.morphed_size = mr.bytes.size();
    r.padded_to = size;
    r.funcs_morphed = 1;
    return r;
}

} // namespace

BinaryRewriteResult rewrite_functions_in_region(std::vector<uint8_t> file, size_t region_off,
                                                size_t region_size, uint64_t region_vaddr,
                                                BinaryFormat fmt, const std::string& name,
                                                const MorphEngineConfig& cfg) {
    BinaryRewriteResult r;
    r.format = fmt;
    r.region_offset = region_off;
    r.region_size = region_size;
    r.region_name = name;
    r.used_function_level = true;

    if (region_off + region_size > file.size() || region_size == 0) {
        r.error = "bad region";
        return r;
    }

    const uint8_t* text = file.data() + region_off;
    auto funcs = extract_real_functions(text, region_size, region_vaddr, 4, 192, 4096);
    r.funcs_seen = funcs.size();
    if (funcs.empty())
        return rewrite_region_whole(std::move(file), region_off, region_size, region_vaddr, name,
                                    fmt, cfg);

    // Collect overflow morph bodies for trampoline append
    struct Overflow {
        size_t func_file_off = 0; ///< absolute file offset of function start
        size_t orig_size = 0;
        std::vector<uint8_t> body;
        size_t blob_index = 0; ///< offset into grow blob
    };
    std::vector<Overflow> overflows;
    std::vector<uint8_t> grow_blob;
    size_t total_morphed_bytes = 0;

    for (const auto& ef : funcs) {
        if (ef.offset + ef.bytes.size() > region_size) {
            ++r.funcs_skipped;
            continue;
        }
        MorphEngineConfig c = cfg;
        c.base_address = ef.vaddr;
        c.verify_pure = false;
        c.require_structural = true;
        c.product = (cfg.product == ProductMode::Lab) ? ProductMode::IndustryExperimental
                                                      : cfg.product;

        // 1) size_fit attempt
        c.size_fit = true;
        c.policy = MorphPolicy::Safe;
        MorphEngine eng(c);
        auto mr = eng.morph(ef.bytes.data(), ef.bytes.size());
        if (mr.ok && !mr.bytes.empty() && mr.bytes.size() <= ef.bytes.size()) {
            const size_t abs = region_off + ef.offset;
            std::memcpy(file.data() + abs, mr.bytes.data(), mr.bytes.size());
            for (size_t i = mr.bytes.size(); i < ef.bytes.size(); i++)
                file[abs + i] = 0x90;
            total_morphed_bytes += mr.bytes.size();
            if (mr.bytes == ef.bytes)
                ++r.funcs_identity;
            else
                ++r.funcs_morphed;
            continue;
        }

        // 2) Full morph + trampoline grow (need ≥5 bytes at site)
        if (ef.bytes.size() < 5) {
            ++r.funcs_skipped;
            continue;
        }
        c.size_fit = false;
        c.policy = MorphPolicy::Safe;
        c.max_size_ratio = 16.0;
        MorphEngine eng2(c);
        mr = eng2.morph(ef.bytes.data(), ef.bytes.size());
        if (!mr.ok || mr.bytes.empty()) {
            ++r.funcs_skipped;
            continue;
        }
        // Align body in grow blob
        while (grow_blob.size() % 16)
            grow_blob.push_back(0x90);
        Overflow ov;
        ov.func_file_off = region_off + ef.offset;
        ov.orig_size = ef.bytes.size();
        ov.blob_index = grow_blob.size();
        ov.body = std::move(mr.bytes);
        grow_blob.insert(grow_blob.end(), ov.body.begin(), ov.body.end());
        // INT3 pad for safety
        grow_blob.push_back(0xCC);
        overflows.push_back(std::move(ov));
    }

    if (!overflows.empty() && (fmt == BinaryFormat::Elf64 || fmt == BinaryFormat::Pe32Plus)) {
        size_t blob_off = 0;
        uint64_t blob_va = 0;
        if (fmt == BinaryFormat::Elf64) {
            if (!elf_append_rx(file, grow_blob, &blob_off, &blob_va)) {
                r.error = "ELF grow failed";
                return r;
            }
            r.section_grew = true;
            r.grow_bytes = grow_blob.size();
            for (const auto& ov : overflows) {
                uint64_t target_va = blob_va + ov.blob_index;
                // Site VA: region_vaddr + (func_file_off - region_off)
                uint64_t site_va = region_vaddr + (ov.func_file_off - region_off);
                // jmp rel32: target - (site + 5)
                int64_t rel = (int64_t)target_va - (int64_t)(site_va + 5);
                if (rel < INT32_MIN || rel > INT32_MAX) {
                    ++r.funcs_skipped;
                    continue;
                }
                auto jmp = encode_jmp_rel32((int32_t)rel);
                std::memcpy(file.data() + ov.func_file_off, jmp.data(), 5);
                for (size_t i = 5; i < ov.orig_size; i++)
                    file[ov.func_file_off + i] = 0xCC; // trap rest of old body
                ++r.funcs_trampolined;
                ++r.funcs_morphed;
                total_morphed_bytes += ov.body.size();
            }
        } else {
            // PE: append raw at end; update last section raw size if possible
            size_t align = 0x200;
            size_t pad = (align - (file.size() % align)) % align;
            file.insert(file.end(), pad, 0);
            blob_off = file.size();
            file.insert(file.end(), grow_blob.begin(), grow_blob.end());
            pad = (align - (file.size() % align)) % align;
            file.insert(file.end(), pad, 0);
            r.section_grew = true;
            r.grow_bytes = grow_blob.size();
            // Prefer image_base + high VA for trampolines — use region_vaddr + large delta
            blob_va = region_vaddr + region_size + 0x10000;
            // Without full PE section header rewrite, trampoline VA may not load —
            // for PE fall back: only count size_fit morphs already done; skip trampoline apply
            // Still append bytes for research; write int3 at sites only if we can map.
            // Industry PE path: rewrite section SizeOfRawData for last section
            uint32_t e_lfanew = 0;
            std::memcpy(&e_lfanew, file.data() + 0x3C, 4);
            const uint8_t* nt = file.data() + e_lfanew;
            uint16_t nsec = 0, opt_size = 0;
            std::memcpy(&nsec, nt + 6, 2);
            std::memcpy(&opt_size, nt + 20, 2);
            uint8_t* sec = (uint8_t*)nt + 24 + opt_size;
            // last section
            if (nsec > 0) {
                uint8_t* last = sec + (size_t)(nsec - 1) * 40;
                uint32_t rawptr = 0, rawsz = 0, va = 0, vsz = 0, chars = 0;
                std::memcpy(&vsz, last + 8, 4);
                std::memcpy(&va, last + 12, 4);
                std::memcpy(&rawsz, last + 16, 4);
                std::memcpy(&rawptr, last + 20, 4);
                std::memcpy(&chars, last + 36, 4);
                uint64_t image_base = 0;
                std::memcpy(&image_base, nt + 24 + 24, 8);
                // Expand last section to cover append if rawptr was near end
                uint32_t new_rawsz = (uint32_t)(file.size() - rawptr);
                std::memcpy(last + 16, &new_rawsz, 4);
                uint32_t new_vsz = new_rawsz;
                std::memcpy(last + 8, &new_vsz, 4);
                chars |= 0x20000000 | 0x40000000 | 0x80000000; // exec|read|write
                std::memcpy(last + 36, &chars, 4);
                blob_va = image_base + va + (blob_off - rawptr);
                for (const auto& ov : overflows) {
                    uint64_t target_va = blob_va + ov.blob_index;
                    uint64_t site_va = region_vaddr + (ov.func_file_off - region_off);
                    int64_t rel = (int64_t)target_va - (int64_t)(site_va + 5);
                    if (rel < INT32_MIN || rel > INT32_MAX) {
                        ++r.funcs_skipped;
                        continue;
                    }
                    auto jmp = encode_jmp_rel32((int32_t)rel);
                    std::memcpy(file.data() + ov.func_file_off, jmp.data(), 5);
                    for (size_t i = 5; i < ov.orig_size; i++)
                        file[ov.func_file_off + i] = 0xCC;
                    ++r.funcs_trampolined;
                    ++r.funcs_morphed;
                    total_morphed_bytes += ov.body.size();
                }
            }
        }
    } else if (!overflows.empty()) {
        // raw format: just skip overflows
        r.funcs_skipped += overflows.size();
    }

    r.ok = true;
    r.file_bytes = std::move(file);
    r.morphed_size = total_morphed_bytes;
    r.padded_to = region_size;
    if (r.funcs_morphed == 0 && r.funcs_identity == 0 && r.funcs_seen > 0 &&
        r.funcs_skipped == r.funcs_seen) {
        r.ok = false;
        r.error = "no functions rewritten";
    }
    return r;
}

BinaryFormat detect_binary_format(const uint8_t* data, size_t len) {
    if (!data || len == 0)
        return BinaryFormat::Unknown;
    if (len >= 4 && data[0] == 0x7F && data[1] == 'E' && data[2] == 'L' && data[3] == 'F')
        return BinaryFormat::Elf64;
    if (len >= 2 && data[0] == 'M' && data[1] == 'Z')
        return BinaryFormat::Pe32Plus;
    return BinaryFormat::Raw;
}

BinaryRewriteResult rewrite_binary_buffer(const uint8_t* data, size_t len,
                                          const MorphEngineConfig& cfg) {
    BinaryRewriteResult r;
    if (!data || !len) {
        r.error = "empty";
        return r;
    }
    std::vector<uint8_t> file(data, data + len);
    BinaryFormat fmt = detect_binary_format(data, len);

    if (fmt == BinaryFormat::Elf64) {
        auto tmp = load_elf64_text; // link
        (void)tmp;
        // Use phdr scan
        if (file.size() < 64 || file[4] != 2) {
            r.error = "not ELF64";
            r.format = fmt;
            return r;
        }
        uint64_t e_phoff = 0;
        uint16_t e_phentsize = 0, e_phnum = 0;
        std::memcpy(&e_phoff, file.data() + 32, 8);
        std::memcpy(&e_phentsize, file.data() + 54, 2);
        std::memcpy(&e_phnum, file.data() + 56, 2);
        size_t best_off = 0, best_sz = 0;
        uint64_t best_va = 0;
        bool found = false;
        for (uint16_t i = 0; i < e_phnum; i++) {
            size_t pho = (size_t)e_phoff + (size_t)i * e_phentsize;
            if (pho + 56 > file.size())
                break;
            uint32_t p_type = 0, p_flags = 0;
            uint64_t p_offset = 0, p_vaddr = 0, p_filesz = 0;
            std::memcpy(&p_type, file.data() + pho, 4);
            std::memcpy(&p_flags, file.data() + pho + 4, 4);
            std::memcpy(&p_offset, file.data() + pho + 8, 8);
            std::memcpy(&p_vaddr, file.data() + pho + 16, 8);
            std::memcpy(&p_filesz, file.data() + pho + 32, 8);
            if (p_type == 1 && (p_flags & 1) && p_filesz > 0 &&
                p_offset + p_filesz <= file.size()) {
                if (!found || p_filesz > best_sz) {
                    best_off = (size_t)p_offset;
                    best_sz = (size_t)p_filesz;
                    best_va = p_vaddr;
                    found = true;
                }
            }
        }
        if (!found) {
            r.error = "no PF_X";
            r.format = fmt;
            return r;
        }
        return rewrite_functions_in_region(std::move(file), best_off, best_sz, best_va, fmt,
                                           "PT_LOAD+X", cfg);
    }

    if (fmt == BinaryFormat::Pe32Plus) {
        // Parse via pe_view style
        if (file.size() < 0x40) {
            r.error = "not PE";
            return r;
        }
        uint32_t e_lfanew = 0;
        std::memcpy(&e_lfanew, file.data() + 0x3C, 4);
        const uint8_t* nt = file.data() + e_lfanew;
        uint16_t nsec = 0, opt_size = 0;
        std::memcpy(&nsec, nt + 6, 2);
        std::memcpy(&opt_size, nt + 20, 2);
        uint64_t image_base = 0;
        std::memcpy(&image_base, nt + 24 + 24, 8);
        const uint8_t* sec = nt + 24 + opt_size;
        for (uint16_t i = 0; i < nsec; i++) {
            char name[9] = {};
            std::memcpy(name, sec, 8);
            uint32_t va = 0, rawsz = 0, rawptr = 0, chars = 0;
            std::memcpy(&va, sec + 12, 4);
            std::memcpy(&rawsz, sec + 16, 4);
            std::memcpy(&rawptr, sec + 20, 4);
            std::memcpy(&chars, sec + 36, 4);
            if (((chars & 0x20000000) || std::strncmp(name, ".text", 5) == 0) && rawsz > 0 &&
                (size_t)rawptr + rawsz <= file.size()) {
                return rewrite_functions_in_region(std::move(file), rawptr, rawsz, image_base + va,
                                                   fmt, name, cfg);
            }
            sec += 40;
        }
        r.error = "no PE text";
        r.format = fmt;
        return r;
    }

    MorphEngine eng(cfg);
    auto mr = eng.morph(data, len);
    if (!mr.ok || mr.bytes.empty()) {
        r.error = mr.error.empty() ? "morph failed" : mr.error;
        r.format = BinaryFormat::Raw;
        return r;
    }
    r.ok = true;
    r.format = BinaryFormat::Raw;
    r.file_bytes = std::move(mr.bytes);
    r.region_size = len;
    r.morphed_size = r.file_bytes.size();
    r.funcs_morphed = 1;
    r.funcs_seen = 1;
    r.region_name = "raw";
    return r;
}

BinaryRewriteResult rewrite_binary_file(const std::string& in_path, const std::string& out_path,
                                        const MorphEngineConfig& cfg) {
    BinaryRewriteResult r;
    auto elf = load_elf64_text(in_path);
    if (elf.ok) {
        MorphEngineConfig c2 = cfg;
        c2.base_address = elf.vaddr;
        c2.verify_pure = false;
        auto rr = rewrite_functions_in_region(elf.file_bytes, elf.file_offset, elf.size, elf.vaddr,
                                              BinaryFormat::Elf64, elf.name, c2);
        if (rr.ok && !out_path.empty() && !write_file(out_path, rr.file_bytes)) {
            rr.ok = false;
            rr.error = "write failed";
        }
        return rr;
    }
    auto pe = load_pe64_text(in_path);
    if (pe.ok) {
        MorphEngineConfig c2 = cfg;
        c2.base_address = pe.vaddr;
        c2.verify_pure = false;
        auto rr = rewrite_functions_in_region(pe.file_bytes, pe.file_offset, pe.size, pe.vaddr,
                                              BinaryFormat::Pe32Plus, pe.name, c2);
        if (rr.ok) {
            pe64_fix_checksum(rr.file_bytes);
            std::string verr;
            if (!pe64_validate(rr.file_bytes.data(), rr.file_bytes.size(), &verr)) {
                rr.ok = false;
                rr.error = "PE validate after rewrite: " + verr;
                return rr;
            }
        }
        if (rr.ok && !out_path.empty() && !write_file(out_path, rr.file_bytes)) {
            rr.ok = false;
            rr.error = "write failed";
        }
        return rr;
    }
    std::ifstream in(in_path, std::ios::binary);
    if (!in) {
        r.error = "cannot read";
        return r;
    }
    std::vector<uint8_t> buf((std::istreambuf_iterator<char>(in)),
                             std::istreambuf_iterator<char>());
    r = rewrite_binary_buffer(buf.data(), buf.size(), cfg);
    if (r.ok && !out_path.empty() && !write_file(out_path, r.file_bytes)) {
        r.ok = false;
        r.error = "write failed";
    }
    return r;
}

bool industry_rewrite_selftest() {
    auto corpus = generate_pure_corpus(16, 0xB1A2u);
    MorphEngineConfig cfg;
    cfg.policy = MorphPolicy::Safe;
    cfg.verify_pure = true;
    cfg.product = ProductMode::Industry;
    size_t ok = 0;
    for (const auto& ef : corpus) {
        if (!ef.pure_interpretable)
            continue;
        MorphEngineConfig c = cfg;
        c.arg_rdi = ef.arg_rdi;
        c.arg_rsi = ef.arg_rsi;
        auto rr = rewrite_binary_buffer(ef.bytes.data(), ef.bytes.size(), c);
        if (rr.ok && !rr.file_bytes.empty())
            ++ok;
    }
    if (ok < 8)
        return false;

    if (load_elf64_text("corpus/real_corpus.elf").ok) {
        MorphEngineConfig c;
        c.policy = MorphPolicy::Safe;
        c.verify_pure = false;
        auto rr = rewrite_binary_file("corpus/real_corpus.elf", "", c);
        if (!rr.ok)
            return false;
        if (rr.funcs_seen >= 5 && rr.funcs_morphed == 0)
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
    // Production victim_clean
    if (load_elf64_text("victim_clean").ok) {
        MorphEngineConfig c;
        c.policy = MorphPolicy::Safe;
        c.verify_pure = false;
        auto rr = rewrite_binary_file("victim_clean", "", c);
        if (!rr.ok)
            return false;
    }
    return true;
}

} // namespace aether
