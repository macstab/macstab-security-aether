/**
 * SPDX-License-Identifier: LicenseRef-Aether-Research
 * Copyright (c) 2024–2026 Macstab GmbH.
 *
 * Licensed under the Aether Research License in the repository root file
 * LICENSE. Authorized lab / lawful private use only as defined there.
 * By using this file you accept LICENSE. See docs/LICENSE_ACCEPTANCE.md.
 */

#include "aether/aether.h"

#include "aether/meta/bench.hpp"
#include "aether/meta/binary_rewrite.hpp"
#include "aether/meta/morph_engine.hpp"
#include "aether/meta/morph_real.hpp"

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <vector>

namespace {
bool g_license_accepted = false;

bool env_license_ok() {
    const char* e = std::getenv("AETHER_LICENSE_ACCEPTED");
    return e && std::strcmp(e, "I_ACCEPT_AETHER_LICENSE") == 0;
}

aether_status require_license() {
    if (g_license_accepted || env_license_ok())
        return AETHER_OK;
    return AETHER_ERR_LICENSE;
}
} // namespace

namespace {

aether::MorphPolicy to_policy(aether_aggression aggr) {
    switch (aggr) {
    case AETHER_AGGR_IDENTITY:
        return aether::MorphPolicy::Identity;
    case AETHER_AGGR_LAB:
    case AETHER_AGGR_INDUSTRY_EXPERIMENTAL:
        return aether::MorphPolicy::Lab;
    default:
        return aether::MorphPolicy::Safe;
    }
}

} // namespace

extern "C" {

int aether_license_is_accepted(void) {
    return (g_license_accepted || env_license_ok()) ? 1 : 0;
}

void aether_accept_license(void) {
    g_license_accepted = true;
}

const char* aether_license_notice(void) {
    return "Aether Research License — see LICENSE. "
           "Authorized lab / lawful private use only. "
           "Set AETHER_LICENSE_ACCEPTED=I_ACCEPT_AETHER_LICENSE or call aether_accept_license(). "
           "docs/LICENSE_ACCEPTANCE.md";
}

const char* aether_version(void) {
    return AETHER_VERSION_STRING;
}

const char* aether_scope(void) {
    return "x86-64 morph library: MorphEngine+SSA dataflow, multi-input oracle, "
           "ELF/PE function rewrite; lab proof — see docs/LIBRARY.md + SCOPE.md";
}

unsigned aether_api_version(void) {
    return AETHER_API_VERSION;
}

int aether_api_compatible(unsigned client_api_version) {
    const unsigned lib_major = (AETHER_API_VERSION >> 16) & 0xFF;
    const unsigned client_major = (client_api_version >> 16) & 0xFF;
    if (lib_major != client_major)
        return 0;
    return client_api_version <= AETHER_API_VERSION ? 1 : 0;
}

void aether_free(void* p) {
    std::free(p);
}

aether_status aether_morph_buffer_ex(const uint8_t* in,
                                     size_t in_len,
                                     uint64_t base_address,
                                     aether_product_mode product,
                                     aether_aggression aggr,
                                     uint8_t** out_code,
                                     size_t* out_len) {
    if (require_license() != AETHER_OK)
        return AETHER_ERR_LICENSE;
    if (!in || !in_len || !out_code || !out_len)
        return AETHER_ERR_INVAL;
    *out_code = nullptr;
    *out_len = 0;

    aether::MorphEngineConfig cfg;
    cfg.base_address = base_address;
    cfg.policy = to_policy(aggr);
    if (product == AETHER_PRODUCT_INDUSTRY)
        cfg.product = aether::ProductMode::Industry;
    else if (product == AETHER_PRODUCT_INDUSTRY_EXPERIMENTAL)
        cfg.product = aether::ProductMode::IndustryExperimental;
    else
        cfg.product = aether::ProductMode::Lab;
    cfg.verify_pure = true;
    cfg.allow_identity_fallback = true;
    if (aggr == AETHER_AGGR_INDUSTRY_EXPERIMENTAL && product != AETHER_PRODUCT_INDUSTRY)
        cfg.product = aether::ProductMode::IndustryExperimental;
    if (cfg.product == aether::ProductMode::Industry) {
        cfg.multi_input_verify = true;
        cfg.domain = aether::MorphDomain::RegsFlags;
    }

    aether::MorphEngine eng(cfg);
    auto r = eng.morph(in, in_len);
    if (!r.ok || r.bytes.empty())
        return AETHER_ERR_MORPH;

    uint8_t* buf = static_cast<uint8_t*>(std::malloc(r.bytes.size()));
    if (!buf)
        return AETHER_ERR_NOMEM;
    std::memcpy(buf, r.bytes.data(), r.bytes.size());
    *out_code = buf;
    *out_len = r.bytes.size();
    return AETHER_OK;
}

aether_status aether_morph_buffer(const uint8_t* in,
                                  size_t in_len,
                                  uint64_t base_address,
                                  aether_aggression aggr,
                                  uint8_t** out_code,
                                  size_t* out_len) {
    aether_product_mode prod = AETHER_PRODUCT_LAB;
    if (aggr == AETHER_AGGR_INDUSTRY_EXPERIMENTAL)
        prod = AETHER_PRODUCT_INDUSTRY_EXPERIMENTAL;
    return aether_morph_buffer_ex(in, in_len, base_address, prod, aggr, out_code, out_len);
}

aether_status aether_morph_file(const char* in_path, const char* out_path, aether_aggression aggr) {
    if (require_license() != AETHER_OK)
        return AETHER_ERR_LICENSE;
    if (!in_path || !out_path)
        return AETHER_ERR_INVAL;
    std::ifstream in(in_path, std::ios::binary);
    if (!in)
        return AETHER_ERR_IO;
    std::vector<uint8_t> buf((std::istreambuf_iterator<char>(in)),
                             std::istreambuf_iterator<char>());
    if (buf.empty())
        return AETHER_ERR_INVAL;
    uint8_t* out = nullptr;
    size_t out_len = 0;
    aether_status st = aether_morph_buffer(buf.data(), buf.size(), 0x1000, aggr, &out, &out_len);
    if (st != AETHER_OK)
        return st;
    std::ofstream o(out_path, std::ios::binary);
    if (!o) {
        aether_free(out);
        return AETHER_ERR_IO;
    }
    o.write(reinterpret_cast<const char*>(out), (std::streamsize)out_len);
    aether_free(out);
    return o ? AETHER_OK : AETHER_ERR_IO;
}

aether_status aether_framework_selftest(size_t pure_samples) {
    if (require_license() != AETHER_OK)
        return AETHER_ERR_LICENSE;
    if (pure_samples < 8)
        pure_samples = 64;
    return aether::industry_framework_selftest(pure_samples) ? AETHER_OK : AETHER_ERR_MORPH;
}

aether_status aether_morph_binary_file(const char* in_path,
                                       const char* out_path,
                                       aether_product_mode product,
                                       aether_aggression aggr) {
    if (require_license() != AETHER_OK)
        return AETHER_ERR_LICENSE;
    if (!in_path)
        return AETHER_ERR_INVAL;
    aether::MorphEngineConfig cfg;
    cfg.policy = to_policy(aggr);
    if (product == AETHER_PRODUCT_INDUSTRY)
        cfg.product = aether::ProductMode::Industry;
    else if (product == AETHER_PRODUCT_INDUSTRY_EXPERIMENTAL)
        cfg.product = aether::ProductMode::IndustryExperimental;
    else
        cfg.product = aether::ProductMode::Lab;
    cfg.verify_pure = false; // whole modules rarely pure
    auto rr = aether::rewrite_binary_file(in_path, out_path ? out_path : "", cfg);
    return rr.ok ? AETHER_OK : AETHER_ERR_MORPH;
}

aether_status aether_industry_selftest(void) {
    if (require_license() != AETHER_OK)
        return AETHER_ERR_LICENSE;
    return aether::industry_finish_selftest() ? AETHER_OK : AETHER_ERR_MORPH;
}

aether_status aether_run_bench(size_t pure_count,
                               const char* elf_path,
                               const char* json_out_path,
                               int* out_pass) {
    if (require_license() != AETHER_OK)
        return AETHER_ERR_LICENSE;
    if (pure_count < 1)
        pure_count = 1000;
    aether::BenchReport r =
        aether::run_morph_bench(pure_count, elf_path ? elf_path : "victim_clean", 0xBEACu, 2);
    if (json_out_path && json_out_path[0]) {
        if (!aether::write_bench_report_json(r, json_out_path))
            return AETHER_ERR_IO;
    }
    if (out_pass)
        *out_pass = r.pass ? 1 : 0;
    return r.pass ? AETHER_OK : AETHER_ERR_MORPH;
}

} // extern "C"
