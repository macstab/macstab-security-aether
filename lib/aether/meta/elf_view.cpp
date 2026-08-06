/**
 * SPDX-License-Identifier: LicenseRef-Aether-Research
 * Copyright (c) 2024–2026 Aether Research Project contributors.
 *
 * Licensed under the Aether Research License in the repository root file
 * LICENSE. Authorized lab / lawful private use only as defined there.
 * By using this file you accept LICENSE. See docs/LICENSE_ACCEPTANCE.md.
 */

#include "aether/meta/elf_view.hpp"

#include <cstdio>
#include <cstring>
#include <fstream>

namespace aether {
namespace {

#pragma pack(push, 1)
struct Ehdr64 {
    uint8_t e_ident[16];
    uint16_t e_type, e_machine;
    uint32_t e_version;
    uint64_t e_entry, e_phoff, e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize, e_phentsize, e_phnum, e_shentsize, e_shnum, e_shstrndx;
};
struct Phdr64 {
    uint32_t p_type, p_flags;
    uint64_t p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align;
};
struct Shdr64 {
    uint32_t sh_name, sh_type;
    uint64_t sh_flags, sh_addr, sh_offset, sh_size, sh_link, sh_info, sh_addralign, sh_entsize;
};
#pragma pack(pop)

constexpr uint32_t PT_LOAD = 1;
constexpr uint32_t PF_X = 1;
constexpr uint32_t SHT_PROGBITS = 1;
constexpr uint64_t SHF_EXECINSTR = 0x4;

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

ElfTextRegion load_elf64_text(const std::string& path) {
    ElfTextRegion r;
    if (!read_file(path, r.file_bytes)) {
        r.error = "cannot read file";
        return r;
    }
    if (r.file_bytes.size() < sizeof(Ehdr64)) {
        r.error = "file too small";
        return r;
    }
    Ehdr64 eh{};
    std::memcpy(&eh, r.file_bytes.data(), sizeof(eh));
    if (std::memcmp(eh.e_ident, "\177ELF", 4) != 0) {
        r.error = "not ELF";
        return r;
    }
    if (eh.e_ident[4] != 2) {
        r.error = "not ELF64";
        return r;
    }
    if (eh.e_machine != 0x3E) { // EM_X86_64
        r.error = "not x86-64 (e_machine != EM_X86_64)";
        // still allow lift for learning — comment if too strict
    }

    // Prefer .text section
    if (eh.e_shoff && eh.e_shnum && eh.e_shstrndx < eh.e_shnum) {
        size_t shoff = (size_t)eh.e_shoff;
        size_t shentsz = eh.e_shentsize;
        size_t shnum = eh.e_shnum;
        if (shoff + shnum * shentsz <= r.file_bytes.size() && shentsz >= sizeof(Shdr64)) {
            Shdr64 strsh{};
            std::memcpy(&strsh,
                        r.file_bytes.data() + shoff + (size_t)eh.e_shstrndx * shentsz,
                        sizeof(strsh));
            const char* strtab = nullptr;
            if (strsh.sh_offset + strsh.sh_size <= r.file_bytes.size())
                strtab = reinterpret_cast<const char*>(r.file_bytes.data() + (size_t)strsh.sh_offset);

            for (size_t i = 0; i < shnum; i++) {
                Shdr64 sh{};
                std::memcpy(&sh, r.file_bytes.data() + shoff + i * shentsz, sizeof(sh));
                if (!strtab || sh.sh_name >= strsh.sh_size)
                    continue;
                const char* name = strtab + sh.sh_name;
                if (std::strcmp(name, ".text") == 0 && sh.sh_size > 0 &&
                    sh.sh_offset + sh.sh_size <= r.file_bytes.size()) {
                    r.ok = true;
                    r.file_offset = (size_t)sh.sh_offset;
                    r.size = (size_t)sh.sh_size;
                    r.vaddr = sh.sh_addr;
                    r.name = ".text";
                    return r;
                }
            }

            // Any executable PROGBITS
            for (size_t i = 0; i < shnum; i++) {
                Shdr64 sh{};
                std::memcpy(&sh, r.file_bytes.data() + shoff + i * shentsz, sizeof(sh));
                if (sh.sh_type == SHT_PROGBITS && (sh.sh_flags & SHF_EXECINSTR) && sh.sh_size > 0 &&
                    sh.sh_offset + sh.sh_size <= r.file_bytes.size()) {
                    r.ok = true;
                    r.file_offset = (size_t)sh.sh_offset;
                    r.size = (size_t)sh.sh_size;
                    r.vaddr = sh.sh_addr;
                    r.name = "SHF_EXECINSTR";
                    return r;
                }
            }
        }
    }

    // Fallback: first executable PT_LOAD
    if (eh.e_phoff && eh.e_phnum) {
        size_t phoff = (size_t)eh.e_phoff;
        size_t phentsz = eh.e_phentsize;
        for (size_t i = 0; i < eh.e_phnum; i++) {
            if (phoff + (i + 1) * phentsz > r.file_bytes.size())
                break;
            Phdr64 ph{};
            std::memcpy(&ph, r.file_bytes.data() + phoff + i * phentsz, sizeof(ph));
            if (ph.p_type == PT_LOAD && (ph.p_flags & PF_X) && ph.p_filesz > 0 &&
                ph.p_offset + ph.p_filesz <= r.file_bytes.size()) {
                r.ok = true;
                r.file_offset = (size_t)ph.p_offset;
                r.size = (size_t)ph.p_filesz;
                r.vaddr = ph.p_vaddr;
                r.name = "PT_LOAD+X";
                return r;
            }
        }
    }

    r.error = "no executable region found";
    return r;
}

} // namespace aether
