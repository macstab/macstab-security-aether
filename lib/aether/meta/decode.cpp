/**
 * SPDX-License-Identifier: LicenseRef-Aether-Research
 * Copyright (c) 2024–2026 Macstab GmbH.
 *
 * Licensed under the Aether Research License in the repository root file
 * LICENSE. Authorized lab / lawful private use only as defined there.
 * By using this file you accept LICENSE. See docs/LICENSE_ACCEPTANCE.md.
 */

#include "aether/meta/decode.hpp"

#include "aether/common/rng.hpp"
#include "aether/meta/assemble.hpp"

#include <cstring>
#include <queue>
#include <vector>

namespace aether {
namespace {

/**
 * Decodes exactly one instruction starting at byte index @p i in buffer @p p.
 * Recognizes nops, ret, push/pop rax, clear/mov/inc/dec, jmp/je; anything else
 * becomes Op::RAW for a single byte. On success advances @p i past the insn
 * and fills @p out. For JMP/JE, stores the absolute target offset in out.imm
 * (block ids are assigned later in build_cfg).
 * @return false only when @p i is already past @p n
 */
bool decode_one(const uint8_t* p, size_t n, size_t& i, IRInsn& out) {
    if (i >= n)
        return false;
    const uint8_t b0 = p[i];

    if (b0 == 0x90) {
        out = IRInsn{Op::NOP, 0, -1, {}};
        i += 1;
        return true;
    }
    if (b0 == 0x66 && i + 1 < n && p[i + 1] == 0x90) {
        out = IRInsn{Op::NOP, 0, -1, {}};
        i += 2;
        return true;
    }
    if (b0 == 0x0F && i + 1 < n && p[i + 1] == 0x1F) {
        size_t len = 2;
        if (i + 2 < n) {
            uint8_t modrm = p[i + 2];
            len = 3;
            if ((modrm & 0xC0) != 0xC0) {
                if ((modrm & 0x07) == 0x04)
                    len++;
                if ((modrm & 0xC0) == 0x40)
                    len++;
                if ((modrm & 0xC0) == 0x80)
                    len += 4;
                if ((modrm & 0xC0) == 0x00 && (modrm & 0x07) == 0x05)
                    len += 4;
            }
        }
        if (i + len <= n) {
            out = IRInsn{Op::NOP, 0, -1, {}};
            i += len;
            return true;
        }
    }
    if (b0 == 0xC3) {
        out = IRInsn{Op::RET, 0, -1, {}};
        i += 1;
        return true;
    }
    if (b0 == 0x50) {
        out = IRInsn{Op::PUSH_RAX, 0, -1, {}};
        i += 1;
        return true;
    }
    if (b0 == 0x58) {
        out = IRInsn{Op::POP_RAX, 0, -1, {}};
        i += 1;
        return true;
    }
    if (b0 == 0x48 && i + 2 < n && p[i + 1] == 0x87 && p[i + 2] == 0xC0) {
        out = IRInsn{Op::XCHG_RAX_RAX, 0, -1, {}};
        i += 3;
        return true;
    }
    if (b0 == 0x87 && i + 1 < n && p[i + 1] == 0xC0) {
        out = IRInsn{Op::XCHG_RAX_RAX, 0, -1, {}};
        i += 2;
        return true;
    }
    if (b0 == 0x48 && i + 1 < n && p[i + 1] == 0xFF && i + 2 < n) {
        if (p[i + 2] == 0xC0) {
            out = IRInsn{Op::INC_RAX, 0, -1, {}};
            i += 3;
            return true;
        }
        if (p[i + 2] == 0xC8) {
            out = IRInsn{Op::DEC_RAX, 0, -1, {}};
            i += 3;
            return true;
        }
    }
    if (b0 == 0x48 && i + 2 < n) {
        if (p[i + 1] == 0x31 && p[i + 2] == 0xC0) {
            out = IRInsn{Op::CLEAR_RAX, 0, -1, {}};
            i += 3;
            return true;
        }
        if (p[i + 1] == 0x29 && p[i + 2] == 0xC0) {
            out = IRInsn{Op::CLEAR_RAX, 0, -1, {}};
            i += 3;
            return true;
        }
        if (p[i + 1] == 0x31 && p[i + 2] == 0xC9) {
            out = IRInsn{Op::CLEAR_RCX, 0, -1, {}};
            i += 3;
            return true;
        }
        if (p[i + 1] == 0x29 && p[i + 2] == 0xC9) {
            out = IRInsn{Op::CLEAR_RCX, 0, -1, {}};
            i += 3;
            return true;
        }
        if (p[i + 1] == 0xC7 && p[i + 2] == 0xC0 && i + 7 <= n) {
            int32_t imm;
            memcpy(&imm, p + i + 3, 4);
            out = IRInsn{Op::MOV_RAX_IMM, imm, -1, {}};
            i += 7;
            return true;
        }
        if (p[i + 1] == 0xC7 && p[i + 2] == 0xC1 && i + 7 <= n) {
            int32_t imm;
            memcpy(&imm, p + i + 3, 4);
            out = IRInsn{Op::MOV_RCX_IMM, imm, -1, {}};
            i += 7;
            return true;
        }
    }
    if (b0 == 0x31 && i + 1 < n && p[i + 1] == 0xC0) {
        out = IRInsn{Op::CLEAR_RAX, 0, -1, {}};
        i += 2;
        return true;
    }
    if (b0 == 0x31 && i + 1 < n && p[i + 1] == 0xC9) {
        out = IRInsn{Op::CLEAR_RCX, 0, -1, {}};
        i += 2;
        return true;
    }
    if (b0 == 0xB8 && i + 5 <= n) {
        int32_t imm;
        memcpy(&imm, p + i + 1, 4);
        out = IRInsn{Op::MOV_RAX_IMM, imm, -1, {}};
        i += 5;
        return true;
    }
    if (b0 == 0xB9 && i + 5 <= n) {
        int32_t imm;
        memcpy(&imm, p + i + 1, 4);
        out = IRInsn{Op::MOV_RCX_IMM, imm, -1, {}};
        i += 5;
        return true;
    }
    // jmp/je: store absolute source offset in imm; build_cfg maps to block ids
    if (b0 == 0xEB && i + 2 <= n) {
        int8_t rel = (int8_t)p[i + 1];
        out = IRInsn{Op::JMP, (int32_t)(i + 2 + rel), -1, {}};
        i += 2;
        return true;
    }
    if (b0 == 0xE9 && i + 5 <= n) {
        int32_t rel;
        memcpy(&rel, p + i + 1, 4);
        out = IRInsn{Op::JMP, (int32_t)(i + 5 + rel), -1, {}};
        i += 5;
        return true;
    }
    if (b0 == 0x74 && i + 2 <= n) {
        int8_t rel = (int8_t)p[i + 1];
        out = IRInsn{Op::JE, (int32_t)(i + 2 + rel), -1, {}};
        i += 2;
        return true;
    }
    if (b0 == 0x0F && i + 1 < n && p[i + 1] == 0x84 && i + 6 <= n) {
        int32_t rel;
        memcpy(&rel, p + i + 2, 4);
        out = IRInsn{Op::JE, (int32_t)(i + 6 + rel), -1, {}};
        i += 6;
        return true;
    }

    // cmp rcx, imm — required for flatten state machine (assemble CMP_STATE)
    if (b0 == 0x83 && i + 2 < n && p[i + 1] == 0xF9) {
        out = IRInsn{Op::CMP_STATE, (int32_t)(int8_t)p[i + 2], -1, {}};
        i += 3;
        return true;
    }
    if (b0 == 0x81 && i + 1 < n && p[i + 1] == 0xF9 && i + 6 <= n) {
        int32_t imm;
        memcpy(&imm, p + i + 2, 4);
        out = IRInsn{Op::CMP_STATE, imm, -1, {}};
        i += 6;
        return true;
    }

    // Catalogue CLEAR_RAX form: push imm8; pop rax
    if (b0 == 0x6A && i + 2 < n && p[i + 2] == 0x58) {
        if (p[i + 1] == 0x00)
            out = IRInsn{Op::CLEAR_RAX, 0, -1, {}};
        else
            out = IRInsn{Op::MOV_RAX_IMM, (int32_t)(int8_t)p[i + 1], -1, {}};
        i += 3;
        return true;
    }

    // Identity / junk forms emitted by catalogue (keep round-trip pure)
    if (b0 == 0x51 && i + 1 < n && p[i + 1] == 0x59) {
        out = IRInsn{Op::JUNK, 0, -1, {}};
        i += 2;
        return true;
    }
    if (b0 == 0x48 && i + 2 < n && p[i + 1] == 0x8D && p[i + 2] == 0x00) {
        out = IRInsn{Op::JUNK, 0, -1, {}}; // lea rax, [rax]
        i += 3;
        return true;
    }
    if (b0 == 0x48 && i + 4 < n && p[i + 1] == 0x8D && p[i + 2] == 0x64 && p[i + 3] == 0x24 &&
        p[i + 4] == 0x00) {
        out = IRInsn{Op::JUNK, 0, -1, {}}; // lea rsp, [rsp+0]
        i += 5;
        return true;
    }
    if (b0 == 0x0F && i + 3 < n && p[i + 1] == 0x1F && p[i + 2] == 0x40 && p[i + 3] == 0x00) {
        out = IRInsn{Op::NOP, 0, -1, {}};
        i += 4;
        return true;
    }
    if (b0 == 0x0F && i + 4 < n && p[i + 1] == 0x1F && p[i + 2] == 0x44 && p[i + 3] == 0x00 &&
        p[i + 4] == 0x00) {
        out = IRInsn{Op::NOP, 0, -1, {}};
        i += 5;
        return true;
    }

    out = IRInsn{Op::RAW, 0, -1, {b0}};
    i += 1;
    return true;
}

/**
 * Splits a linear IR stream into basic blocks and wires control-flow edges.
 * Marks leaders at index 0, branch targets, and the instruction after each
 * branch/ret; also splits long straight-line runs for permutation surface.
 * Converts JMP/JE out.imm absolute offsets into out.target block ids and sets
 * each block's fallthrough successor (or -1 for JMP/RET).
 * @param stream   decoded instructions in program order
 * @param insn_off insn_off[k] = source byte offset of stream[k]
 * @return IRFunc with entry = 0
 */
IRFunc build_cfg(const std::vector<IRInsn>& stream, const std::vector<size_t>& insn_off) {
    const size_t n = stream.size();
    std::vector<char> is_leader(n, 0);
    if (n)
        is_leader[0] = 1;

    auto offset_to_index = [&](int32_t abs_off) -> int {
        for (size_t k = 0; k < insn_off.size(); k++) {
            if ((int32_t)insn_off[k] == abs_off)
                return (int)k;
        }
        return -1;
    };

    for (size_t k = 0; k < n; k++) {
        const auto& in = stream[k];
        if (in.op == Op::JMP || in.op == Op::JE) {
            int t = offset_to_index(in.imm);
            if (t >= 0)
                is_leader[t] = 1;
            if (k + 1 < n)
                is_leader[k + 1] = 1;
        } else if (in.op == Op::RET) {
            if (k + 1 < n)
                is_leader[k + 1] = 1;
        }
    }

    // Split oversized straight-line runs for permutation surface.
    size_t run = 0;
    for (size_t k = 0; k < n; k++) {
        if (is_leader[k])
            run = 0;
        run++;
        if (run > (size_t)rnd(8, 14) && k + 1 < n) {
            is_leader[k + 1] = 1;
            run = 0;
        }
    }

    IRFunc f;
    std::vector<int> insn_to_block(n, -1);
    int bid = 0;
    for (size_t k = 0; k < n;) {
        IRBlock blk;
        blk.id = bid;
        size_t start = k;
        k++;
        while (k < n && !is_leader[k])
            k++;
        for (size_t j = start; j < k; j++) {
            insn_to_block[j] = bid;
            blk.insns.push_back(stream[j]);
        }
        f.blocks.push_back(std::move(blk));
        bid++;
    }

    for (size_t bi = 0; bi < f.blocks.size(); bi++) {
        auto& blk = f.blocks[bi];
        if (blk.insns.empty()) {
            blk.fallthrough = (bi + 1 < f.blocks.size()) ? (int)(bi + 1) : -1;
            continue;
        }
        IRInsn& last = blk.insns.back();
        if (last.op == Op::JMP || last.op == Op::JE) {
            int ti = offset_to_index(last.imm);
            last.target = (ti >= 0) ? insn_to_block[ti] : -1;
            last.imm = 0;
            if (last.op == Op::JE)
                blk.fallthrough = (bi + 1 < f.blocks.size()) ? (int)(bi + 1) : -1;
            else
                blk.fallthrough = -1;
        } else if (last.op == Op::RET) {
            blk.fallthrough = -1;
        } else {
            blk.fallthrough = (bi + 1 < f.blocks.size()) ? (int)(bi + 1) : -1;
        }
    }

    f.entry = 0;
    return f;
}

} // namespace

/**
 * Turns a raw machine-code buffer into an IR function with basic blocks.
 * Linear decode → coalesce RAW → build_cfg (leaders, targets, fallthrough).
 * @return IRFunc ready for metamorphic transforms
 */
IRFunc disasm(const uint8_t* code, size_t len) {
    std::vector<IRInsn> stream;
    std::vector<size_t> offs;
    size_t i = 0;
    while (i < len) {
        size_t at = i;
        IRInsn in;
        if (!decode_one(code, len, i, in))
            break;
        if (in.op == Op::RAW && !stream.empty() && stream.back().op == Op::RAW) {
            stream.back().raw.push_back(in.raw[0]);
        } else {
            offs.push_back(at);
            stream.push_back(std::move(in));
        }
    }
    return build_cfg(stream, offs);
}

std::vector<uint8_t> normalize_reachable(const uint8_t* code, size_t len) {
    if (!code || !len)
        return {};
    IRFunc f = disasm(code, len);
    if (f.blocks.empty() || f.entry < 0 || f.entry >= (int)f.blocks.size())
        return {};

    const int n = (int)f.blocks.size();
    std::vector<char> reach((size_t)n, 0);
    std::queue<int> q;
    q.push(f.entry);
    reach[(size_t)f.entry] = 1;
    while (!q.empty()) {
        int b = q.front();
        q.pop();
        const IRBlock& blk = f.blocks[(size_t)b];
        if (blk.fallthrough >= 0 && blk.fallthrough < n && !reach[(size_t)blk.fallthrough]) {
            reach[(size_t)blk.fallthrough] = 1;
            q.push(blk.fallthrough);
        }
        for (const auto& in : blk.insns) {
            if ((in.op == Op::JMP || in.op == Op::JE) && in.target >= 0 && in.target < n &&
                !reach[(size_t)in.target]) {
                reach[(size_t)in.target] = 1;
                q.push(in.target);
            }
        }
    }

    std::vector<int> old_to_new((size_t)n, -1);
    int nid = 0;
    for (int i = 0; i < n; i++) {
        if (reach[(size_t)i])
            old_to_new[(size_t)i] = nid++;
    }
    if (nid == 0)
        return {};

    IRFunc g;
    g.blocks.resize((size_t)nid);
    g.entry = old_to_new[(size_t)f.entry];
    for (int i = 0; i < n; i++) {
        if (!reach[(size_t)i])
            continue;
        const int j = old_to_new[(size_t)i];
        IRBlock nb = f.blocks[(size_t)i];
        nb.id = j;
        if (nb.fallthrough >= 0 && nb.fallthrough < n)
            nb.fallthrough = old_to_new[(size_t)nb.fallthrough];
        else
            nb.fallthrough = -1;
        for (auto& in : nb.insns) {
            if ((in.op == Op::JMP || in.op == Op::JE) && in.target >= 0 && in.target < n)
                in.target = old_to_new[(size_t)in.target];
        }
        g.blocks[(size_t)j] = std::move(nb);
    }
    return assemble(g);
}

} // namespace aether
