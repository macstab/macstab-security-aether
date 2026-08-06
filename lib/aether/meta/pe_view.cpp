/**
 * SPDX-License-Identifier: LicenseRef-Aether-Research
 * Copyright (c) 2024–2026 Aether Research Project contributors.
 *
 * Licensed under the Aether Research License in the repository root file
 * LICENSE. Authorized lab / lawful private use only as defined there.
 * By using this file you accept LICENSE. See docs/LICENSE_ACCEPTANCE.md.
 */

#include "aether/meta/pe_view.hpp"

#include <cstring>
#include <fstream>

namespace aether {
namespace {

bool read_file(const std::string& path, std::vector<uint8_t>& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return false;
    in.seekg(0, std::ios::end);
    auto n = in.tellg();
    if (n <= 0)
        return false;
    in.seekg(0, std::ios::beg);
    out.resize((size_t)n);
    in.read(reinterpret_cast<char*>(out.data()), n);
    return (bool)in;
}

} // namespace

bool pe64_validate(const uint8_t* data, size_t len, std::string* err) {
    auto fail = [&](const char* m) {
        if (err)
            *err = m;
        return false;
    };
    if (!data || len < 0x40)
        return fail("too small");
    if (data[0] != 'M' || data[1] != 'Z')
        return fail("not MZ");
    uint32_t e_lfanew = 0;
    std::memcpy(&e_lfanew, data + 0x3C, 4);
    if ((size_t)e_lfanew + 24 + 112 > len)
        return fail("bad e_lfanew");
    const uint8_t* nt = data + e_lfanew;
    if (std::memcmp(nt, "PE\0\0", 4) != 0)
        return fail("not PE");
    uint16_t machine = 0, nsec = 0, opt_size = 0;
    std::memcpy(&machine, nt + 4, 2);
    std::memcpy(&nsec, nt + 6, 2);
    std::memcpy(&opt_size, nt + 20, 2);
    if (machine != 0x8664)
        return fail("not AMD64");
    const uint8_t* opt = nt + 24;
    uint16_t magic = 0;
    std::memcpy(&magic, opt, 2);
    if (magic != 0x20B)
        return fail("not PE32+");
    if ((size_t)(opt - data) + opt_size > len)
        return fail("opt header OOB");
    const uint8_t* sec = opt + opt_size;
    for (uint16_t i = 0; i < nsec; i++) {
        if ((size_t)(sec - data) + 40 > len)
            return fail("section header OOB");
        uint32_t rawsz = 0, rawptr = 0;
        std::memcpy(&rawsz, sec + 16, 4);
        std::memcpy(&rawptr, sec + 20, 4);
        if (rawsz > 0 && (size_t)rawptr + rawsz > len)
            return fail("section data OOB");
        sec += 40;
    }
    return true;
}

void pe64_fix_checksum(std::vector<uint8_t>& file) {
    if (file.size() < 0x40)
        return;
    uint32_t e_lfanew = 0;
    std::memcpy(&e_lfanew, file.data() + 0x3C, 4);
    if ((size_t)e_lfanew + 24 + 88 > file.size())
        return;
    // CheckSum is at optional header + 64 for PE32+
    size_t ck_off = (size_t)e_lfanew + 24 + 64;
    if (ck_off + 4 > file.size())
        return;
    // Zero checksum field, then compute
    std::memset(file.data() + ck_off, 0, 4);
    uint64_t sum = 0;
    const size_t n = file.size();
    for (size_t i = 0; i + 1 < n; i += 2) {
        uint16_t w = (uint16_t)file[i] | ((uint16_t)file[i + 1] << 8);
        sum += w;
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    if (n & 1)
        sum += file[n - 1];
    sum = (sum & 0xFFFF) + (sum >> 16);
    sum += (uint32_t)n;
    uint32_t ck = (uint32_t)sum;
    std::memcpy(file.data() + ck_off, &ck, 4);
}

PeTextRegion load_pe64_text(const std::string& path) {
    PeTextRegion r;
    if (!read_file(path, r.file_bytes)) {
        r.error = "cannot read";
        return r;
    }
    std::string verr;
    if (!pe64_validate(r.file_bytes.data(), r.file_bytes.size(), &verr)) {
        r.error = verr;
        return r;
    }
    uint32_t e_lfanew = 0;
    std::memcpy(&e_lfanew, r.file_bytes.data() + 0x3C, 4);
    r.e_lfanew = e_lfanew;
    const uint8_t* nt = r.file_bytes.data() + e_lfanew;
    uint16_t nsec = 0, opt_size = 0;
    std::memcpy(&nsec, nt + 6, 2);
    std::memcpy(&opt_size, nt + 20, 2);
    const uint8_t* opt = nt + 24;
    uint32_t file_align = 0x200, sect_align = 0x1000;
    std::memcpy(&sect_align, opt + 32, 4);
    std::memcpy(&file_align, opt + 36, 4);
    r.file_align = file_align ? file_align : 0x200;
    r.sect_align = sect_align ? sect_align : 0x1000;
    std::memcpy(&r.image_base, opt + 24, 8);
    const uint8_t* sec = opt + opt_size;
    for (uint16_t i = 0; i < nsec; i++) {
        char name[9] = {};
        std::memcpy(name, sec, 8);
        uint32_t vsize = 0, va = 0, rawsz = 0, rawptr = 0, chars = 0;
        std::memcpy(&vsize, sec + 8, 4);
        std::memcpy(&va, sec + 12, 4);
        std::memcpy(&rawsz, sec + 16, 4);
        std::memcpy(&rawptr, sec + 20, 4);
        std::memcpy(&chars, sec + 36, 4);
        const bool exec = (chars & 0x20000000) != 0;
        const bool is_text = std::strncmp(name, ".text", 5) == 0;
        if ((is_text || exec) && rawsz > 0 && (size_t)rawptr + rawsz <= r.file_bytes.size()) {
            r.ok = true;
            r.file_offset = rawptr;
            r.size = rawsz;
            r.vaddr = r.image_base + va;
            r.name = name;
            r.section_header_off = (size_t)(sec - r.file_bytes.data());
            r.section_index = i;
            r.characteristics = chars;
            if (is_text)
                return r;
        }
        sec += 40;
    }
    if (!r.ok)
        r.error = "no executable section";
    return r;
}

} // namespace aether
