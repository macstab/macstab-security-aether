/**
 * SPDX-License-Identifier: LicenseRef-Aether-Research
 * Copyright (c) 2024–2026 Macstab GmbH.
 *
 * Licensed under the Aether Research License in the repository root file
 * LICENSE. Authorized lab / lawful private use only as defined there.
 * By using this file you accept LICENSE. See docs/LICENSE_ACCEPTANCE.md.
 */

#include "aether/meta/stages.hpp"

#include "aether/common/rng.hpp"
#include "aether/meta/assemble.hpp"
#include "aether/meta/catalogue.hpp"
#include "aether/meta/decode.hpp"
#include "aether/meta/transforms.hpp"

// normalize_reachable used after each multi-stage gen

#include <algorithm>
#include <cstring>

namespace aether {
namespace {

/** Soft cap so nested multi-stage demos stay article-sized. */
constexpr size_t kMaxStageBytes = 4096;

/**
 * Appends a random trailing NOP sled (never prepend — would break relocs).
 */
void pad_trailing_nops(std::vector<uint8_t>& code) {
    const int n = rnd(2, 6);
    for (int i = 0; i < n; i++)
        emit_form(code, pick(cat_nop()));
}

/**
 * XOR-encodes @p buf in place with a single-byte key.
 */
void xor_bytes(std::vector<uint8_t>& buf, uint8_t key) {
    for (auto& b : buf)
        b = static_cast<uint8_t>(b ^ key);
}

/**
 * Packages morph'd outer + independent morph'd child (child not re-disasm'd).
 * Layout: [outer] E9 rel32 'A''e' key u32len [xored child] C3
 */
std::vector<uint8_t>
package_nested(std::vector<uint8_t> outer, std::vector<uint8_t> child_xored, uint8_t key) {
    if (!outer.empty() && outer.back() == 0xC3)
        outer.pop_back();

    pad_trailing_nops(outer);

    const size_t jmp_at = outer.size();
    outer.push_back(0xE9);
    outer.insert(outer.end(), {0, 0, 0, 0});

    outer.push_back(static_cast<uint8_t>('A'));
    outer.push_back(static_cast<uint8_t>('e'));
    outer.push_back(key);
    const uint32_t clen = static_cast<uint32_t>(child_xored.size());
    outer.push_back(static_cast<uint8_t>(clen));
    outer.push_back(static_cast<uint8_t>(clen >> 8));
    outer.push_back(static_cast<uint8_t>(clen >> 16));
    outer.push_back(static_cast<uint8_t>(clen >> 24));
    outer.insert(outer.end(), child_xored.begin(), child_xored.end());

    const size_t after = outer.size();
    const int32_t rel = static_cast<int32_t>(after - (jmp_at + 5));
    std::memcpy(outer.data() + jmp_at + 1, &rel, 4);

    outer.push_back(0xC3);
    return outer;
}

} // namespace

/**
 * Runs a single metamorphic stage: random transforms + random encodings.
 */
std::vector<uint8_t> morph_stage(const uint8_t* src, size_t len, StageReport* report) {
    StageReport local;
    StageReport& r = report ? *report : local;
    r = {};
    r.kind = "morph";
    r.bytes_in = len;

    if (!src || !len)
        return {};

    const bool light = (len > kMaxStageBytes);

    IRFunc ir = disasm(src, len);
    r.blocks = ir.blocks.size();
    for (const auto& b : ir.blocks)
        r.insns += b.insns.size();

    if (light) {
        // Keep uniqueness without exponential CFG growth.
        diversify_implementation(ir);
        if (ir.blocks.size() >= 3)
            permute_blocks(ir);
        expand(ir);
        r.transform_steps = 3;
        r.flattened = false;
    } else {
        const int min_s = rnd(3, 5);
        const int max_s = rnd(min_s, min_s + 3);
        r.transform_steps = max_s;
        r.flattened = apply_random_transform_schedule(ir, min_s, max_s);
        if (rnd(0, 1) == 0)
            diversify_implementation(ir);
    }

    auto code = assemble(ir);
    // Pure entry path only, then trailing NOP sled for byte diversity
    // (multi_stage strips sled via normalize before re-disasm).
    code = normalize_reachable(code);
    if (code.empty())
        code = assemble(ir);
    pad_trailing_nops(code);

    if (code.size() > kMaxStageBytes * 2) {
        auto clipped = normalize_reachable(code.data(), std::min(code.size(), kMaxStageBytes * 2));
        if (!clipped.empty()) {
            code = std::move(clipped);
            pad_trailing_nops(code);
        } else
            code.resize(kMaxStageBytes * 2);
    }

    r.bytes_out = code.size();
    return code;
}

/**
 * Morph with an explicit structural mode (permute-heavy, flatten, wide, …).
 */
std::vector<uint8_t>
morph_stage_mode(const uint8_t* src, size_t len, LayerMode mode, StageReport* report) {
    StageReport local;
    StageReport& r = report ? *report : local;
    r = {};
    r.kind = layer_mode_name(mode);
    r.bytes_in = len;

    if (!src || !len)
        return {};

    IRFunc ir = disasm(src, len);
    r.blocks = ir.blocks.size();
    for (const auto& b : ir.blocks)
        r.insns += b.insns.size();

    apply_layer_mode(ir, mode);
    r.flattened = (mode == LayerMode::Flatten);
    r.transform_steps = 1;

    // Count blocks after mode (structure changed).
    r.blocks = ir.blocks.size();

    auto code = assemble(ir);
    code = normalize_reachable(code);
    if (code.empty())
        code = assemble(ir);
    pad_trailing_nops(code);
    if (code.size() > kMaxStageBytes * 2) {
        auto clipped = normalize_reachable(code.data(), std::min(code.size(), kMaxStageBytes * 2));
        if (!clipped.empty()) {
            code = std::move(clipped);
            pad_trailing_nops(code);
        } else
            code.resize(kMaxStageBytes * 2);
    }
    r.bytes_out = code.size();
    return code;
}

/**
 * One multi-stage generation that is re-lift safe:
 * no flatten (state-machine re-disasm is still fragile under junk),
 * normalize_reachable before emit, trailing nops only after pure path.
 */
std::vector<uint8_t> morph_stage_relift_safe(const uint8_t* src, size_t len, StageReport* report) {
    StageReport local;
    StageReport& r = report ? *report : local;
    r = {};
    r.kind = "morph-relift";
    r.bytes_in = len;
    r.flattened = false;
    if (!src || !len)
        return {};

    IRFunc ir = disasm(src, len);
    r.blocks = ir.blocks.size();
    for (const auto& b : ir.blocks)
        r.insns += b.insns.size();

    // Explicit schedule without Flatten (re-lift safe).
    diversify_implementation(ir);
    if (ir.blocks.size() >= 3)
        permute_blocks(ir);
    expand(ir);
    safe_permute_insns(ir);
    if (rnd(0, 1) == 0)
        heavy_permute(ir);
    r.transform_steps = 4;

    auto code = assemble(ir);
    code = normalize_reachable(code);
    if (code.empty())
        code = assemble(ir);
    pad_trailing_nops(code);
    r.bytes_out = code.size();
    return code;
}

/**
 * Feed-forward multi-stage: each stage re-morphs the previous machine code.
 * Uses relift-safe morph (no flatten) + normalize between gens.
 */
std::vector<uint8_t> multi_stage_morph(const uint8_t* src,
                                       size_t len,
                                       std::vector<StageReport>* reports,
                                       int min_stages,
                                       int max_stages,
                                       const char* kind) {
    if (!src || !len)
        return {};
    if (min_stages < 1)
        min_stages = 1;
    if (max_stages < min_stages)
        max_stages = min_stages;
    if (!kind)
        kind = "morph";

    const int stages = rnd(min_stages, max_stages);
    std::vector<uint8_t> cur(src, src + len);
    const int base_index = reports ? static_cast<int>(reports->size()) : 0;

    for (int i = 0; i < stages; i++) {
        StageReport rep;
        cur = morph_stage_relift_safe(cur.data(), cur.size(), &rep);
        if (!cur.empty())
            cur = normalize_reachable(cur);
        rep.index = base_index + i;
        rep.kind = kind;
        if (reports)
            reports->push_back(rep);
        if (cur.empty())
            break;
    }
    return cur;
}

/**
 * Nested generation: multi-stage child XOR-wrapped inside multi-stage outer.
 */
std::vector<uint8_t>
nested_generation(const uint8_t* src, size_t len, std::vector<StageReport>* reports) {
    if (!src || !len)
        return {};

    // Child lineage (code generated dynamically) — 2–3 stages.
    auto child = multi_stage_morph(src, len, reports, 2, 3, "nested-child");
    if (child.empty())
        child.assign(src, src + len);

    // Outer lineage — independent multi-stage morph of the same seed.
    auto outer = multi_stage_morph(src, len, reports, 2, 3, "morph");
    if (outer.empty())
        outer.assign(src, src + len);

    const uint8_t key = static_cast<uint8_t>(rnd(1, 255));
    xor_bytes(child, key);

    auto packed = package_nested(std::move(outer), std::move(child), key);

    if (reports) {
        StageReport wrap;
        wrap.index = static_cast<int>(reports->size());
        wrap.kind = "nested-wrap";
        wrap.bytes_in = len;
        wrap.bytes_out = packed.size();
        reports->push_back(wrap);
    }
    return packed;
}

/**
 * Full engine: multi-stage feed-forward, usually plus nested child generation.
 */
std::vector<uint8_t>
engine_pipeline(const uint8_t* src, size_t len, std::vector<StageReport>* reports) {
    if (reports)
        reports->clear();
    if (!src || !len)
        return {};

    // ~80% nested multi-stage; else pure multi-stage (2–4 gens).
    if (rnd(0, 4) != 0) {
        auto nested = nested_generation(src, len, reports);
        if (!nested.empty())
            return nested;
    }

    return multi_stage_morph(src, len, reports, 2, 4, "morph");
}

} // namespace aether
