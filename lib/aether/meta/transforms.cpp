/**
 * SPDX-License-Identifier: LicenseRef-Aether-Research
 * Copyright (c) 2024–2026 Aether Research Project contributors.
 *
 * Licensed under the Aether Research License in the repository root file
 * LICENSE. Authorized lab / lawful private use only as defined there.
 * By using this file you accept LICENSE. See docs/LICENSE_ACCEPTANCE.md.
 */

#include "aether/meta/transforms.hpp"

#include "aether/common/rng.hpp"

#include <algorithm>
#include <utility>

namespace aether {

/**
 * Removes some redundant NOP / JUNK / XCHG_RAX_RAX instructions from each
 * block. Keeps the first filler in a run; later fillers are dropped with
 * probability 2/3. Does not change control-flow edges.
 */
void shrink(IRFunc& f) {
    for (auto& b : f.blocks) {
        std::vector<IRInsn> c;
        c.reserve(b.insns.size());
        int nop_run = 0;
        for (auto& in : b.insns) {
            if (in.op == Op::NOP || in.op == Op::XCHG_RAX_RAX || in.op == Op::JUNK) {
                nop_run++;
                if (nop_run > 1 && rnd(0, 2) != 0)
                    continue;
            } else {
                nop_run = 0;
            }
            c.push_back(std::move(in));
        }
        b.insns.swap(c);
    }
}

/**
 * Inserts extra NOP and JUNK after non-terminator ops to grow and diversify
 * the instruction stream. Never inserts after JMP, JE, or RET.
 */
void expand(IRFunc& f) {
    for (auto& b : f.blocks) {
        std::vector<IRInsn> e;
        e.reserve(b.insns.size() * 2);
        for (auto& in : b.insns) {
            e.push_back(in);
            if (in.op == Op::JMP || in.op == Op::JE || in.op == Op::RET)
                continue;
            if (rnd(0, 3) == 0)
                e.push_back(IRInsn{Op::NOP, 0, -1, {}});
            if (rnd(0, 5) == 0)
                e.push_back(IRInsn{Op::JUNK, 0, -1, {}});
        }
        b.insns.swap(e);
    }
}

namespace {
constexpr uint8_t kRax = 1u << 0;
constexpr uint8_t kRcx = 1u << 1;
constexpr uint8_t kZf = 1u << 2;
constexpr uint8_t kStack = 1u << 3;
constexpr uint8_t kCtrl = 1u << 4;
constexpr uint8_t kOpaque = 1u << 5;
} // namespace

InsnEffect effect_of(Op op) {
    InsnEffect e;
    switch (op) {
    case Op::NOP:
    case Op::JUNK:
    case Op::XCHG_RAX_RAX:
        break;
    case Op::CLEAR_RAX:
    case Op::MOV_RAX_IMM:
        e.defs = kRax;
        break;
    case Op::INC_RAX:
    case Op::DEC_RAX:
        e.uses = kRax;
        e.defs = kRax;
        break;
    case Op::PUSH_RAX:
        e.uses = kRax | kStack;
        e.defs = kStack;
        break;
    case Op::POP_RAX:
        e.uses = kStack;
        e.defs = kRax | kStack;
        break;
    case Op::CLEAR_RCX:
    case Op::MOV_RCX_IMM:
    case Op::SET_STATE:
        e.defs = kRcx;
        break;
    case Op::CMP_STATE:
        e.uses = kRcx;
        e.defs = kZf;
        break;
    case Op::JE:
        e.uses = kZf;
        e.defs = kCtrl;
        break;
    case Op::JMP:
    case Op::RET:
        e.defs = kCtrl;
        break;
    case Op::RAW:
        e.uses = e.defs = kOpaque | kRax | kRcx | kZf | kStack | kCtrl;
        break;
    }
    return e;
}

bool insn_pair_independent(Op a, Op b) {
    const InsnEffect ea = effect_of(a);
    const InsnEffect eb = effect_of(b);
    if ((ea.defs | ea.uses | eb.defs | eb.uses) & (kCtrl | kOpaque))
        return false;
    // classic independence: no write-read / read-write / write-write hazards
    if (ea.defs & eb.uses)
        return false;
    if (eb.defs & ea.uses)
        return false;
    if (ea.defs & eb.defs)
        return false;
    return true;
}

void safe_permute_insns(IRFunc& f) {
    for (auto& b : f.blocks) {
        if (b.insns.size() < 2)
            continue;
        // Multiple bubble passes of random independent adjacent swaps
        const int passes = rnd(1, 4);
        for (int p = 0; p < passes; p++) {
            for (size_t i = 0; i + 1 < b.insns.size(); i++) {
                Op oa = b.insns[i].op;
                Op ob = b.insns[i + 1].op;
                if (!insn_pair_independent(oa, ob))
                    continue;
                if (rnd(0, 1) == 0)
                    std::swap(b.insns[i], b.insns[i + 1]);
            }
        }
    }
}

/**
 * Physically reorders basic blocks (entry first), remaps ids/edges, and
 * inserts JMP when fallthrough is no longer adjacent in layout order.
 * No-op if fewer than 3 blocks.
 */
void permute_blocks(IRFunc& f) {
    if (f.blocks.size() < 3)
        return;

    const int entry = f.entry;
    std::vector<int> order;
    order.reserve(f.blocks.size());
    for (size_t i = 0; i < f.blocks.size(); i++)
        order.push_back((int)i);
    std::shuffle(order.begin() + 1, order.end(), rng());

    std::vector<IRBlock> nb;
    nb.reserve(f.blocks.size());
    std::vector<int> old_to_new(f.blocks.size());
    for (size_t ni = 0; ni < order.size(); ni++) {
        old_to_new[order[ni]] = (int)ni;
        nb.push_back(std::move(f.blocks[order[ni]]));
        nb.back().id = (int)ni;
    }

    for (auto& b : nb) {
        if (b.fallthrough >= 0)
            b.fallthrough = old_to_new[b.fallthrough];
        for (auto& in : b.insns) {
            if (in.target >= 0)
                in.target = old_to_new[in.target];
        }
    }

    for (size_t i = 0; i < nb.size(); i++) {
        auto& b = nb[i];
        if (b.fallthrough < 0)
            continue;
        bool terminal_jmp = !b.insns.empty() && b.insns.back().op == Op::JMP;
        bool terminal_ret = !b.insns.empty() && b.insns.back().op == Op::RET;
        if (terminal_jmp || terminal_ret)
            continue;
        int expected_next = (i + 1 < nb.size()) ? (int)(i + 1) : -1;
        if (b.fallthrough != expected_next) {
            IRInsn j;
            j.op = Op::JMP;
            j.target = b.fallthrough;
            b.insns.push_back(j);
            b.fallthrough = -1;
        }
    }

    f.blocks.swap(nb);
    f.entry = old_to_new[entry];
}

/**
 * Replaces the CFG with a rcx state-machine: dispatcher + entry stub +
 * bodies that set next state and jump back to the dispatcher.
 * No-op if fewer than 2 blocks.
 */
void flatten(IRFunc& f) {
    if (f.blocks.size() < 2)
        return;

    std::vector<IRBlock> bodies = std::move(f.blocks);
    const int n = (int)bodies.size();
    const int old_entry = f.entry;

    std::vector<int> state(n);
    for (int i = 0; i < n; i++)
        state[i] = 0x10 + i;
    std::shuffle(state.begin(), state.end(), rng());

    const int disp_id = 0;
    const int stub_id = 1;
    const int body0 = 2;

    std::vector<IRBlock> epilogues;

    /**
     * Removes the terminal control-flow instruction from block @p b and
     * reports how the block exited: RET, JMP (taken), JE (taken+fallthrough),
     * or plain fallthrough. Also drops any residual mid-block JMP/JE/RET.
     */
    auto strip_ctrl =
        [](IRBlock& b, int& taken, int& fthru, bool& was_ret, bool& was_jmp, bool& was_je) {
            taken = -1;
            fthru = b.fallthrough;
            was_ret = was_jmp = was_je = false;
            if (!b.insns.empty()) {
                auto& last = b.insns.back();
                if (last.op == Op::RET) {
                    was_ret = true;
                    b.insns.pop_back();
                } else if (last.op == Op::JMP) {
                    was_jmp = true;
                    taken = last.target;
                    b.insns.pop_back();
                    fthru = -1;
                } else if (last.op == Op::JE) {
                    was_je = true;
                    taken = last.target;
                    b.insns.pop_back();
                }
            }
            std::vector<IRInsn> cleaned;
            cleaned.reserve(b.insns.size());
            for (auto& in : b.insns) {
                if (in.op == Op::JMP || in.op == Op::JE || in.op == Op::RET)
                    continue;
                cleaned.push_back(std::move(in));
            }
            b.insns.swap(cleaned);
        };

    for (int i = 0; i < n; i++) {
        int taken, fthru;
        bool was_ret, was_jmp, was_je;
        strip_ctrl(bodies[i], taken, fthru, was_ret, was_jmp, was_je);
        bodies[i].id = body0 + i;
        bodies[i].fallthrough = -1;

        if (was_ret) {
            bodies[i].insns.push_back(IRInsn{Op::RET, 0, -1, {}});
        } else if (was_je && taken >= 0 && taken < n && fthru >= 0 && fthru < n) {
            int ep_id = body0 + n + (int)epilogues.size();

            IRBlock ep;
            ep.id = ep_id;
            ep.fallthrough = -1;
            ep.insns.push_back(IRInsn{Op::SET_STATE, state[taken], -1, {}});
            ep.insns.push_back(IRInsn{Op::JMP, 0, disp_id, {}});
            epilogues.push_back(std::move(ep));

            bodies[i].insns.push_back(IRInsn{Op::JE, 0, ep_id, {}});
            bodies[i].insns.push_back(IRInsn{Op::SET_STATE, state[fthru], -1, {}});
            bodies[i].insns.push_back(IRInsn{Op::JMP, 0, disp_id, {}});
        } else {
            int next = was_jmp ? taken : fthru;
            if (next >= 0 && next < n) {
                bodies[i].insns.push_back(IRInsn{Op::SET_STATE, state[next], -1, {}});
                bodies[i].insns.push_back(IRInsn{Op::JMP, 0, disp_id, {}});
            } else {
                bodies[i].insns.push_back(IRInsn{Op::RET, 0, -1, {}});
            }
        }
    }

    IRFunc flat;
    flat.entry = stub_id;
    flat.blocks.resize(body0 + n + epilogues.size());

    IRBlock& disp = flat.blocks[disp_id];
    disp.id = disp_id;
    disp.fallthrough = -1;
    for (int i = 0; i < n; i++) {
        disp.insns.push_back(IRInsn{Op::CMP_STATE, state[i], -1, {}});
        disp.insns.push_back(IRInsn{Op::JE, 0, body0 + i, {}});
    }
    disp.insns.push_back(IRInsn{Op::RET, 0, -1, {}});

    IRBlock& stub = flat.blocks[stub_id];
    stub.id = stub_id;
    stub.fallthrough = -1;
    stub.insns.push_back(IRInsn{Op::SET_STATE, state[old_entry], -1, {}});
    stub.insns.push_back(IRInsn{Op::JMP, 0, disp_id, {}});

    for (int i = 0; i < n; i++)
        flat.blocks[body0 + i] = std::move(bodies[i]);
    for (size_t e = 0; e < epilogues.size(); e++)
        flat.blocks[body0 + n + (int)e] = std::move(epilogues[e]);

    f = std::move(flat);
}

/**
 * Rewrites instructions into alternate semantic implementations at IR level.
 * CLEAR_RAX / constants / fillers become different op sequences so generations
 * differ in structure, not only in catalogue encoding.
 */
void diversify_implementation(IRFunc& f) {
    for (auto& b : f.blocks) {
        std::vector<IRInsn> out;
        out.reserve(b.insns.size() * 2);
        for (auto& in : b.insns) {
            // Never break terminators.
            if (in.op == Op::JMP || in.op == Op::JE || in.op == Op::RET || in.op == Op::SET_STATE ||
                in.op == Op::CMP_STATE || in.op == Op::RAW) {
                out.push_back(std::move(in));
                continue;
            }

            const int mode = rnd(0, 5);
            if (in.op == Op::CLEAR_RAX) {
                if (mode == 0) {
                    out.push_back(IRInsn{Op::MOV_RAX_IMM, 0, -1, {}});
                } else if (mode == 1) {
                    out.push_back(IRInsn{Op::PUSH_RAX, 0, -1, {}});
                    out.push_back(IRInsn{Op::POP_RAX, 0, -1, {}});
                    out.push_back(IRInsn{Op::CLEAR_RAX, 0, -1, {}});
                } else if (mode == 2) {
                    out.push_back(IRInsn{Op::CLEAR_RAX, 0, -1, {}});
                    out.push_back(IRInsn{Op::NOP, 0, -1, {}});
                } else {
                    out.push_back(std::move(in));
                }
            } else if (in.op == Op::CLEAR_RCX) {
                if (mode <= 1)
                    out.push_back(IRInsn{Op::MOV_RCX_IMM, 0, -1, {}});
                else
                    out.push_back(std::move(in));
            } else if (in.op == Op::MOV_RAX_IMM && in.imm >= 0 && in.imm <= 4 && mode <= 2) {
                // Rebuild small constants: xor rax; inc rax × imm
                out.push_back(IRInsn{Op::CLEAR_RAX, 0, -1, {}});
                for (int k = 0; k < in.imm; k++)
                    out.push_back(IRInsn{Op::INC_RAX, 0, -1, {}});
            } else if (in.op == Op::NOP) {
                if (mode == 0)
                    out.push_back(IRInsn{Op::XCHG_RAX_RAX, 0, -1, {}});
                else if (mode == 1)
                    out.push_back(IRInsn{Op::JUNK, 0, -1, {}});
                else if (mode == 2) {
                    out.push_back(IRInsn{Op::NOP, 0, -1, {}});
                    out.push_back(IRInsn{Op::JUNK, 0, -1, {}});
                } else {
                    out.push_back(std::move(in));
                }
            } else if (in.op == Op::INC_RAX && mode == 0) {
                // inc as: push; pop; inc  (noise around real op)
                out.push_back(IRInsn{Op::JUNK, 0, -1, {}});
                out.push_back(IRInsn{Op::INC_RAX, 0, -1, {}});
            } else {
                out.push_back(std::move(in));
            }
        }
        b.insns.swap(out);
    }
}

/**
 * Aggressive block permutation: 1–3 permute passes with optional expand.
 */
void heavy_permute(IRFunc& f) {
    const int passes = rnd(1, 3);
    for (int i = 0; i < passes; i++) {
        permute_blocks(f);
        if (rnd(0, 1) == 0)
            expand(f);
    }
}

/**
 * Split oversized straight-line blocks so later shuffles have more nodes.
 */
static void split_large_blocks(IRFunc& f) {
    std::vector<IRBlock> out;
    out.reserve(f.blocks.size() * 2);
    for (auto& b : f.blocks) {
        if (b.insns.size() <= 8) {
            out.push_back(std::move(b));
            continue;
        }
        // Split into chunks of 4–7 insns (keep terminators on last chunk).
        size_t i = 0;
        while (i < b.insns.size()) {
            size_t take = (size_t)rnd(4, 7);
            if (i + take >= b.insns.size())
                take = b.insns.size() - i;
            // Don't orphan a terminator alone mid-split awkwardly
            if (i + take < b.insns.size()) {
                auto& mid = b.insns[i + take - 1];
                if (mid.op == Op::JMP || mid.op == Op::JE || mid.op == Op::RET)
                    take = take > 1 ? take - 1 : take;
            }
            IRBlock nb;
            nb.insns.assign(b.insns.begin() + (std::ptrdiff_t)i,
                            b.insns.begin() + (std::ptrdiff_t)(i + take));
            nb.fallthrough = -1;
            out.push_back(std::move(nb));
            i += take;
        }
        // Chain fallthroughs of split pieces (last keeps original fallthrough/term).
        if (!out.empty()) {
            // Fix only the newly added pieces: find start index
        }
    }
    // Rebuild fallthrough for sequential split pieces: simpler re-id approach
    for (size_t i = 0; i < out.size(); i++) {
        out[i].id = (int)i;
        bool term = !out[i].insns.empty() &&
                    (out[i].insns.back().op == Op::JMP || out[i].insns.back().op == Op::JE ||
                     out[i].insns.back().op == Op::RET);
        if (!term)
            out[i].fallthrough = (i + 1 < out.size()) ? (int)(i + 1) : -1;
    }
    f.blocks = std::move(out);
    f.entry = 0;
}

/** Shuffle only pure filler runs inside a block (NOP/JUNK/XCHG). */
static void shuffle_filler_islands(IRFunc& f) {
    for (auto& b : f.blocks) {
        size_t i = 0;
        while (i < b.insns.size()) {
            auto is_fill = [](Op o) {
                return o == Op::NOP || o == Op::JUNK || o == Op::XCHG_RAX_RAX;
            };
            if (!is_fill(b.insns[i].op)) {
                i++;
                continue;
            }
            size_t j = i;
            while (j < b.insns.size() && is_fill(b.insns[j].op))
                j++;
            if (j - i >= 2)
                std::shuffle(b.insns.begin() + (std::ptrdiff_t)i,
                             b.insns.begin() + (std::ptrdiff_t)j,
                             rng());
            i = j;
        }
    }
}

/** Rotate non-entry block order left by k. */
static void rotate_non_entry(IRFunc& f, int k) {
    if (f.blocks.size() < 3)
        return;
    // Convert to order of ids, rotate, reapply via permute-like remap
    std::vector<int> order;
    for (size_t i = 0; i < f.blocks.size(); i++)
        order.push_back((int)i);
    if (order.size() < 3)
        return;
    k = k % (int)(order.size() - 1);
    if (k <= 0)
        return;
    std::rotate(order.begin() + 1, order.begin() + 1 + k, order.end());

    // Same remap as permute_blocks
    std::vector<IRBlock> nb(f.blocks.size());
    std::vector<int> old_to_new(f.blocks.size());
    for (size_t ni = 0; ni < order.size(); ni++) {
        old_to_new[order[ni]] = (int)ni;
        nb[ni] = std::move(f.blocks[order[ni]]);
        nb[ni].id = (int)ni;
    }
    for (auto& b : nb) {
        if (b.fallthrough >= 0)
            b.fallthrough = old_to_new[b.fallthrough];
        for (auto& in : b.insns)
            if (in.target >= 0)
                in.target = old_to_new[in.target];
    }
    for (size_t i = 0; i < nb.size(); i++) {
        auto& b = nb[i];
        if (b.fallthrough < 0)
            continue;
        bool terminal =
            !b.insns.empty() && (b.insns.back().op == Op::JMP || b.insns.back().op == Op::RET);
        if (terminal)
            continue;
        if (b.fallthrough != (int)(i + 1) && i + 1 <= nb.size()) {
            IRInsn j;
            j.op = Op::JMP;
            j.target = b.fallthrough;
            b.insns.push_back(j);
            b.fallthrough = -1;
        }
    }
    f.blocks.swap(nb);
    f.entry = old_to_new[f.entry];
}

/**
 * Strong metamorphic permutation suite.
 */
void world_class_permute(IRFunc& f) {
    if (f.blocks.empty())
        return;

    // Grow surface area.
    if (rnd(0, 1) == 0)
        expand(f);
    split_large_blocks(f);

    // Multi-strategy layout chaos.
    const int rounds = rnd(2, 5);
    for (int r = 0; r < rounds; r++) {
        switch (rnd(0, 3)) {
        case 0:
            permute_blocks(f);
            break;
        case 1:
            heavy_permute(f);
            break;
        case 2:
            rotate_non_entry(f, rnd(1, 5));
            permute_blocks(f);
            break;
        default:
            heavy_permute(f);
            if (f.blocks.size() >= 4) {
                // Reverse middle third physically via another shuffle pass.
                permute_blocks(f);
            }
            break;
        }
        shuffle_filler_islands(f);
        safe_permute_insns(f); // def-use gated intra-block swaps
        if (rnd(0, 2) == 0)
            diversify_implementation(f);
    }

    // Final repair pass: ensure JMP fallthroughs after all moves.
    heavy_permute(f);
    safe_permute_insns(f);
}

/**
 * Applies a random multiset of transforms in random order.
 * @return true if flatten ran at least once
 */
bool apply_random_transform_schedule(IRFunc& f, int min_steps, int max_steps) {
    if (min_steps < 1)
        min_steps = 1;
    if (max_steps < min_steps)
        max_steps = min_steps;

    enum class Step : int { Shrink = 0, Expand, Diversify, Permute, HeavyPermute, Flatten, Count };

    const int nsteps = rnd(min_steps, max_steps);
    bool did_flatten = false;

    for (int i = 0; i < nsteps; i++) {
        const auto step = static_cast<Step>(rnd(0, static_cast<int>(Step::Count) - 1));
        switch (step) {
        case Step::Shrink:
            shrink(f);
            break;
        case Step::Expand:
            expand(f);
            // Random intensity: second expand sometimes
            if (rnd(0, 2) == 0)
                expand(f);
            break;
        case Step::Diversify:
            diversify_implementation(f);
            break;
        case Step::Permute:
            permute_blocks(f);
            safe_permute_insns(f);
            break;
        case Step::HeavyPermute:
            heavy_permute(f);
            safe_permute_insns(f);
            break;
        case Step::Flatten:
            if (f.blocks.size() >= 2) {
                flatten(f);
                did_flatten = true;
            }
            break;
        default:
            break;
        }
    }

    // Guarantee at least one layout-changing transform when possible.
    if (f.blocks.size() >= 3 && rnd(0, 1) == 0)
        permute_blocks(f);
    if (f.blocks.size() >= 2 && !did_flatten && rnd(0, 2) == 0) {
        flatten(f);
        did_flatten = true;
    }

    return did_flatten;
}

LayerMode random_layer_mode() {
    return static_cast<LayerMode>(rnd(0, static_cast<int>(LayerMode::Count) - 1));
}

const char* layer_mode_name(LayerMode mode) {
    switch (mode) {
    case LayerMode::PermuteHeavy:
        return "permute-heavy";
    case LayerMode::Flatten:
        return "flatten";
    case LayerMode::Wide:
        return "wide";
    case LayerMode::Dense:
        return "dense";
    case LayerMode::Minimal:
        return "minimal";
    case LayerMode::Mixed:
        return "mixed";
    default:
        return "unknown";
    }
}

/**
 * Applies one named structural design so successive layers differ by design.
 */
void apply_layer_mode(IRFunc& f, LayerMode mode) {
    switch (mode) {
    case LayerMode::PermuteHeavy:
        world_class_permute(f);
        break;
    case LayerMode::Flatten:
        diversify_implementation(f);
        if (f.blocks.size() >= 2)
            flatten(f);
        expand(f);
        break;
    case LayerMode::Wide:
        expand(f);
        expand(f);
        diversify_implementation(f);
        expand(f);
        break;
    case LayerMode::Dense:
        diversify_implementation(f);
        shrink(f);
        diversify_implementation(f);
        shrink(f);
        break;
    case LayerMode::Minimal:
        if (rnd(0, 1) == 0)
            diversify_implementation(f);
        else
            permute_blocks(f);
        break;
    case LayerMode::Mixed:
    default:
        (void)apply_random_transform_schedule(f, 3, 6);
        break;
    }
}

} // namespace aether
