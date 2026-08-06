/**
 * SPDX-License-Identifier: LicenseRef-Aether-Research
 * Copyright (c) 2024–2026 Aether Research Project contributors.
 *
 * Licensed under the Aether Research License in the repository root file
 * LICENSE. Authorized lab / lawful private use only as defined there.
 * By using this file you accept LICENSE. See docs/LICENSE_ACCEPTANCE.md.
 */

#include "aether/meta/catalogue.hpp"

#include "aether/common/rng.hpp"

namespace aether {
namespace {

// rax := 0
const std::vector<Form> kClearRax = {
    {{0x48, 0x31, 0xC0}, 12},                        // xor rax, rax
    {{0x48, 0x29, 0xC0}, 10},                        // sub rax, rax
    {{0x48, 0xC7, 0xC0, 0x00, 0x00, 0x00, 0x00}, 8}, // mov rax, 0
    {{0x31, 0xC0}, 7},                               // xor eax, eax
    {{0x6A, 0x00, 0x58}, 5},                         // push 0; pop rax
};

const std::vector<Form> kClearRcx = {
    {{0x48, 0x31, 0xC9}, 12},
    {{0x48, 0x29, 0xC9}, 9},
    {{0x31, 0xC9}, 7},
    {{0x48, 0xC7, 0xC1, 0x00, 0x00, 0x00, 0x00}, 6},
};

const std::vector<Form> kNop = {
    {{0x90}, 14},
    {{0x66, 0x90}, 10},
    {{0x0F, 0x1F, 0x00}, 9},
    {{0x0F, 0x1F, 0x40, 0x00}, 7},
    {{0x0F, 0x1F, 0x44, 0x00, 0x00}, 5},
    {{0x48, 0x87, 0xC0}, 4}, // xchg rax, rax
};

const std::vector<Form> kJunk = {
    {{0x90}, 8},
    {{0x66, 0x90}, 6},
    {{0x48, 0x87, 0xC0}, 5},
    {{0x50, 0x58}, 4},                   // push rax; pop rax
    {{0x51, 0x59}, 4},                   // push rcx; pop rcx
    {{0x48, 0x8D, 0x00}, 3},             // lea rax, [rax]
    {{0x87, 0xC0}, 3},                   // xchg eax, eax
    {{0x0F, 0x1F, 0x00}, 3},             // nop dword [rax]
    {{0x48, 0x8D, 0x64, 0x24, 0x00}, 2}, // lea rsp, [rsp+0] (identity)
};

} // namespace

/**
 * Chooses one Form from @p catalogue with probability proportional to weight.
 * Sums weights, draws rnd(0, total-1), walks the list until the bucket hits.
 * @return const reference into @p catalogue
 */
const Form& pick(const std::vector<Form>& catalogue) {
    int total = 0;
    for (const auto& f : catalogue)
        total += f.weight;
    int r = rnd(0, total - 1);
    for (const auto& f : catalogue) {
        if (r < f.weight)
            return f;
        r -= f.weight;
    }
    return catalogue[0];
}

/**
 * Appends form.bytes to the end of @p out.
 */
void emit_form(std::vector<uint8_t>& out, const Form& form) {
    out.insert(out.end(), form.bytes.begin(), form.bytes.end());
}

/**
 * Appends a 32-bit little-endian integer to @p out (4 bytes).
 */
void emit_u32(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back((uint8_t)(v));
    out.push_back((uint8_t)(v >> 8));
    out.push_back((uint8_t)(v >> 16));
    out.push_back((uint8_t)(v >> 24));
}

/**
 * Appends `mov ecx, imm32` (opcode B9 + little-endian imm32).
 */
void emit_mov_rcx_imm(std::vector<uint8_t>& out, uint32_t imm) {
    out.push_back(0xB9);
    emit_u32(out, imm);
}

/**
 * Appends `mov eax, imm32` (opcode B8 + little-endian imm32).
 */
void emit_mov_rax_imm(std::vector<uint8_t>& out, uint32_t imm) {
    out.push_back(0xB8);
    emit_u32(out, imm);
}

/** Returns encodings that clear RAX (semantic rax := 0). */
const std::vector<Form>& cat_clear_rax() {
    return kClearRax;
}

/** Returns encodings that clear RCX (semantic rcx := 0). */
const std::vector<Form>& cat_clear_rcx() {
    return kClearRcx;
}

/** Returns multi-byte NOP / identity encodings. */
const std::vector<Form>& cat_nop() {
    return kNop;
}

/** Returns side-effect-free junk encodings. */
const std::vector<Form>& cat_junk() {
    return kJunk;
}

} // namespace aether
