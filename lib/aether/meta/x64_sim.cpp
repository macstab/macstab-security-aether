/**
 * SPDX-License-Identifier: LicenseRef-Aether-Research
 * Copyright (c) 2024–2026 Macstab GmbH.
 *
 * Licensed under the Aether Research License in the repository root file
 * LICENSE. Authorized lab / lawful private use only as defined there.
 * By using this file you accept LICENSE. See docs/LICENSE_ACCEPTANCE.md.
 */

#include "aether/meta/x64_sim.hpp"

#include "aether/meta/decode_real.hpp"

#include <cstring>

namespace aether {
namespace {

constexpr size_t kStackBytes = 4096;

bool is_nop(const RealInsn& in) {
    const auto& b = in.bytes;
    if (b.empty())
        return false;
    if (b[0] == 0x90)
        return true;
    if (b.size() >= 2 && b[0] == 0x66 && b[1] == 0x90)
        return true;
    if (b.size() >= 3 && b[0] == 0x0F && b[1] == 0x1F)
        return true;
    if (b.size() >= 3 && b[0] == 0x48 && b[1] == 0x87 && b[2] == 0xC0)
        return true;
    if (b.size() >= 2 && b[0] == 0x87 && b[1] == 0xC0)
        return true;
    if (!in.text.empty() && in.text.find("nop") == 0)
        return true;
    return false;
}

uint64_t* gpr_ptr(X64SimState& s, int reg) {
    // 0=rax ... 7=rdi (low 3 bits of modrm)
    switch (reg & 7) {
    case 0:
        return &s.rax;
    case 1:
        return &s.rcx;
    case 2:
        return &s.rdx;
    case 3:
        return &s.rbx;
    case 4:
        return &s.rsp;
    case 5:
        return &s.rbp;
    case 6:
        return &s.rsi;
    case 7:
        return &s.rdi;
    default:
        return &s.rax;
    }
}

} // namespace

std::optional<uint32_t> sim_real_func(const RealFunc& f, uint64_t rdi, uint64_t rsi) {
    if (f.insns.empty() || f.blocks.empty())
        return std::nullopt;

    X64SimState st;
    st.rdi = rdi;
    st.rsi = rsi;
    st.rsp = 0x0800; // mid-stack
    st.rbp = st.rsp;
    uint8_t stack[kStackBytes];
    std::memset(stack, 0, sizeof(stack));

    int bi = f.entry;
    if (bi < 0 || bi >= (int)f.blocks.size())
        return std::nullopt;

    for (int steps = 0; steps < 200000; steps++) {
        if (bi < 0 || bi >= (int)f.blocks.size())
            return std::nullopt;
        const RealBlock& b = f.blocks[(size_t)bi];
        for (size_t ii = b.start_idx; ii < b.end_idx && ii < f.insns.size(); ii++) {
            const RealInsn& in = f.insns[ii];
            const auto& by = in.bytes;
            if (by.empty())
                continue;
            if (is_nop(in))
                continue;

            // ret
            if (in.is_ret || by[0] == 0xC3)
                return (uint32_t)st.rax;

            // push r64: 50+r
            if (by[0] >= 0x50 && by[0] <= 0x57 && by.size() == 1) {
                if (st.rsp < 8)
                    return std::nullopt;
                st.rsp -= 8;
                uint64_t v = *gpr_ptr(st, by[0] - 0x50);
                std::memcpy(stack + st.rsp, &v, 8);
                continue;
            }
            // pop r64: 58+r
            if (by[0] >= 0x58 && by[0] <= 0x5F && by.size() == 1) {
                if (st.rsp + 8 > kStackBytes)
                    return std::nullopt;
                uint64_t v = 0;
                std::memcpy(&v, stack + st.rsp, 8);
                *gpr_ptr(st, by[0] - 0x58) = v;
                st.rsp += 8;
                continue;
            }
            // push imm8: 6A ib
            if (by[0] == 0x6A && by.size() >= 2) {
                if (st.rsp < 8)
                    return std::nullopt;
                st.rsp -= 8;
                int64_t v = (int8_t)by[1];
                uint64_t u = (uint64_t)v;
                std::memcpy(stack + st.rsp, &u, 8);
                continue;
            }

            // mov r32, imm32: B8+r
            if (by[0] >= 0xB8 && by[0] <= 0xBF && by.size() >= 5) {
                uint32_t imm;
                std::memcpy(&imm, by.data() + 1, 4);
                *gpr_ptr(st, by[0] - 0xB8) = imm;
                continue;
            }
            // mov rax, imm32: 48 C7 C0 imm32
            if (by.size() >= 7 && by[0] == 0x48 && by[1] == 0xC7 && by[2] == 0xC0) {
                uint32_t imm;
                std::memcpy(&imm, by.data() + 3, 4);
                st.rax = imm;
                continue;
            }
            // xor r64,r64 / sub r64,r64 clear forms
            if (by.size() >= 3 && by[0] == 0x48 && by[1] == 0x31 && (by[2] & 0xC0) == 0xC0) {
                int r1 = by[2] & 7, r2 = (by[2] >> 3) & 7;
                if (r1 == r2) {
                    *gpr_ptr(st, r1) = 0;
                    st.zf = true;
                    continue;
                }
            }
            if (by.size() >= 3 && by[0] == 0x48 && by[1] == 0x29 && (by[2] & 0xC0) == 0xC0) {
                int r1 = by[2] & 7, r2 = (by[2] >> 3) & 7;
                if (r1 == r2) {
                    *gpr_ptr(st, r1) = 0;
                    st.zf = true;
                    continue;
                }
            }
            if (by.size() >= 2 && by[0] == 0x31 && (by[1] & 0xC0) == 0xC0) {
                int r1 = by[1] & 7, r2 = (by[1] >> 3) & 7;
                if (r1 == r2) {
                    *gpr_ptr(st, r1) = 0;
                    st.zf = true;
                    continue;
                }
            }
            // xor eax,eax
            if (by.size() >= 2 && by[0] == 0x31 && by[1] == 0xC0) {
                st.rax = 0;
                st.zf = true;
                continue;
            }
            // mov eax, edi/esi
            if (by.size() >= 2 && by[0] == 0x89 && by[1] == 0xF8) {
                st.rax = (uint32_t)st.rdi;
                continue;
            }
            if (by.size() >= 2 && by[0] == 0x89 && by[1] == 0xF0) {
                st.rax = (uint32_t)st.rsi;
                continue;
            }
            // add eax, edi/esi
            if (by.size() >= 2 && by[0] == 0x01 && by[1] == 0xF8) {
                st.rax = (uint32_t)(st.rax + st.rdi);
                continue;
            }
            if (by.size() >= 2 && by[0] == 0x01 && by[1] == 0xF0) {
                st.rax = (uint32_t)(st.rax + st.rsi);
                continue;
            }
            // inc/dec rax
            if (by.size() >= 3 && by[0] == 0x48 && by[1] == 0xFF && by[2] == 0xC0) {
                st.rax = (uint32_t)(st.rax + 1);
                continue;
            }
            if (by.size() >= 3 && by[0] == 0x48 && by[1] == 0xFF && by[2] == 0xC8) {
                st.rax = (uint32_t)(st.rax - 1);
                continue;
            }
            // mov ecx,imm / xor ecx
            if (by[0] == 0xB9 && by.size() >= 5) {
                uint32_t imm;
                std::memcpy(&imm, by.data() + 1, 4);
                st.rcx = imm;
                continue;
            }
            if ((by.size() >= 3 && by[0] == 0x48 && by[1] == 0x31 && by[2] == 0xC9) ||
                (by.size() >= 2 && by[0] == 0x31 && by[1] == 0xC9)) {
                st.rcx = 0;
                continue;
            }
            // cmp rcx, imm8/imm32
            if (by.size() >= 3 && by[0] == 0x83 && by[1] == 0xF9) {
                st.zf = ((uint32_t)st.rcx == (uint32_t)(int8_t)by[2]);
                continue;
            }
            if (by.size() >= 6 && by[0] == 0x81 && by[1] == 0xF9) {
                uint32_t imm;
                std::memcpy(&imm, by.data() + 2, 4);
                st.zf = ((uint32_t)st.rcx == imm);
                continue;
            }

            // sub rsp, imm8: 48 83 EC ib
            if (by.size() >= 4 && by[0] == 0x48 && by[1] == 0x83 && by[2] == 0xEC) {
                st.rsp -= (uint8_t)by[3];
                continue;
            }
            // add rsp, imm8: 48 83 C4 ib
            if (by.size() >= 4 && by[0] == 0x48 && by[1] == 0x83 && by[2] == 0xC4) {
                st.rsp += (uint8_t)by[3];
                continue;
            }
            // mov rbp, rsp: 48 89 E5
            if (by.size() >= 3 && by[0] == 0x48 && by[1] == 0x89 && by[2] == 0xE5) {
                st.rbp = st.rsp;
                continue;
            }
            // mov rsp, rbp: 48 89 EC
            if (by.size() >= 3 && by[0] == 0x48 && by[1] == 0x89 && by[2] == 0xEC) {
                st.rsp = st.rbp;
                continue;
            }
            // leave: C9
            if (by[0] == 0xC9) {
                st.rsp = st.rbp;
                if (st.rsp + 8 > kStackBytes)
                    return std::nullopt;
                std::memcpy(&st.rbp, stack + st.rsp, 8);
                st.rsp += 8;
                continue;
            }

            // mov r/m64, r64 with [rsp+disp8]: 48 89 44 24 d8  etc — simplified:
            // 48 89 04 24 = mov [rsp], rax
            if (by.size() >= 4 && by[0] == 0x48 && by[1] == 0x89 && by[2] == 0x04 &&
                by[3] == 0x24) {
                if (st.rsp + 8 > kStackBytes)
                    return std::nullopt;
                std::memcpy(stack + st.rsp, &st.rax, 8);
                continue;
            }
            // 48 89 44 24 disp8 = mov [rsp+d], rax
            if (by.size() >= 5 && by[0] == 0x48 && by[1] == 0x89 && by[2] == 0x44 &&
                by[3] == 0x24) {
                int8_t d = (int8_t)by[4];
                uint64_t idx = st.rsp + (uint64_t)(int64_t)d;
                if (idx + 8 > kStackBytes)
                    return std::nullopt;
                std::memcpy(stack + idx, &st.rax, 8);
                continue;
            }
            // 48 8B 04 24 = mov rax, [rsp]
            if (by.size() >= 4 && by[0] == 0x48 && by[1] == 0x8B && by[2] == 0x04 &&
                by[3] == 0x24) {
                if (st.rsp + 8 > kStackBytes)
                    return std::nullopt;
                std::memcpy(&st.rax, stack + st.rsp, 8);
                continue;
            }
            // 48 8B 44 24 disp8 = mov rax, [rsp+d]
            if (by.size() >= 5 && by[0] == 0x48 && by[1] == 0x8B && by[2] == 0x44 &&
                by[3] == 0x24) {
                int8_t d = (int8_t)by[4];
                uint64_t idx = st.rsp + (uint64_t)(int64_t)d;
                if (idx + 8 > kStackBytes)
                    return std::nullopt;
                std::memcpy(&st.rax, stack + idx, 8);
                continue;
            }

            // lea rax,[rax] identity
            if (by.size() >= 3 && by[0] == 0x48 && by[1] == 0x8D && by[2] == 0x00)
                continue;

            // Control transfer
            if (in.is_uncond_jump) {
                if (b.branch < 0)
                    return std::nullopt;
                bi = b.branch;
                goto next_block;
            }
            if (in.is_cond_jump) {
                if (st.zf) {
                    if (b.branch < 0)
                        return std::nullopt;
                    bi = b.branch;
                    goto next_block;
                }
                continue;
            }

            // Unsupported
            return std::nullopt;
        }
        // fallthrough
        if (b.fallthrough >= 0)
            bi = b.fallthrough;
        else
            return std::nullopt;
    next_block:;
    }
    return std::nullopt;
}

bool sim_multi_input_equiv(const RealFunc& a, const RealFunc& b, size_t* checked) {
    static const uint64_t kPairs[][2] = {
        {0, 0},
        {1, 2},
        {0xFFu, 0xAAu},
        {7, 0},
        {0, 9},
        {0x1234u, 0x5678u},
        {3, 5},
        {100, 200},
    };
    size_t n = 0;
    for (const auto& p : kPairs) {
        auto ea = sim_real_func(a, p[0], p[1]);
        if (!ea)
            continue;
        auto eb = sim_real_func(b, p[0], p[1]);
        if (!eb || *ea != *eb)
            return false;
        ++n;
    }
    if (checked)
        *checked = n;
    return n > 0;
}

std::optional<uint32_t>
sim_x64_buffer(const uint8_t* code, size_t len, uint64_t rdi, uint64_t rsi, uint64_t base) {
    if (!code || !len)
        return std::nullopt;
    RealFunc f = disasm_real(code, len, base);
    return sim_real_func(f, rdi, rsi);
}

} // namespace aether
