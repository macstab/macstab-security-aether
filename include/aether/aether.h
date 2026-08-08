/**
 * SPDX-License-Identifier: LicenseRef-Aether-Research
 * Copyright (c) 2024–2026 Macstab GmbH.
 *
 * Licensed under the Aether Research License in the repository root file
 * LICENSE. Authorized lab / lawful private use only as defined there.
 * By using this file you accept LICENSE. See docs/LICENSE_ACCEPTANCE.md.
 */

/**
 * @file aether.h
 * @brief Stable C API for Aether research morph engine (Macstab GmbH lab).
 *
 * Versioning: AETHER_VERSION_* macros match VERSION file at release.
 * Link: libaether_core + Zydis.
 *
 * Scope: see docs/SCOPE.md — x86-64 research morph, not full industrial rewriter.
 */
#ifndef AETHER_H
#define AETHER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Stable API: do not break signatures without bumping VERSION + major. */
#define AETHER_VERSION_MAJOR 3
#define AETHER_VERSION_MINOR 0
#define AETHER_VERSION_PATCH 0
#define AETHER_VERSION_STRING "1.0.0"

/** Compile-time ABI check helper for clients. */
#define AETHER_API_VERSION 0x00030000u

/** Morph aggression (matches SCOPE.md). */
typedef enum aether_aggression {
    AETHER_AGGR_IDENTITY = 0,
    AETHER_AGGR_SAFE = 1, /**< nops + permute + encoding diversify + shuffle */
    AETHER_AGGR_LAB = 2,  /**< Safe + extra morph passes */
    AETHER_AGGR_INDUSTRY_EXPERIMENTAL = 3 /**< best-effort structural; pure still checked when possible */
} aether_aggression;

/** Product guarantee mode. */
typedef enum aether_product_mode {
    AETHER_PRODUCT_LAB = 0,                  /**< 0 pure breaks required (lab 10) */
    AETHER_PRODUCT_INDUSTRY_EXPERIMENTAL = 1,/**< best-effort structural */
    AETHER_PRODUCT_INDUSTRY = 2              /**< hard pure + multi-input; binary rewrite path */
} aether_product_mode;

/** Result codes. */
typedef enum aether_status {
    AETHER_OK = 0,
    AETHER_ERR_INVAL = 1,
    AETHER_ERR_MORPH = 2,
    AETHER_ERR_IO = 3,
    AETHER_ERR_NOMEM = 4,
    AETHER_ERR_LICENSE = 5 /**< LICENSE not accepted — see docs/LICENSE_ACCEPTANCE.md */
} aether_status;

/**
 * License acceptance (required for morph APIs).
 * Env: AETHER_LICENSE_ACCEPTED=I_ACCEPT_AETHER_LICENSE
 * Or call aether_accept_license() after operator review of LICENSE.
 */
int aether_license_is_accepted(void);
void aether_accept_license(void);
const char* aether_license_notice(void);

/**
 * Morph a raw x86-64 code buffer (analysis-gated restricted morph).
 * Caller frees *out_code with aether_free().
 */
aether_status aether_morph_buffer(const uint8_t* in,
                                  size_t in_len,
                                  uint64_t base_address,
                                  aether_aggression aggr,
                                  uint8_t** out_code,
                                  size_t* out_len);

/**
 * Morph with explicit product mode (Lab vs Industry experimental).
 * Lab: pure verify hard-fail. Industry experimental: best-effort output.
 */
aether_status aether_morph_buffer_ex(const uint8_t* in,
                                     size_t in_len,
                                     uint64_t base_address,
                                     aether_product_mode product,
                                     aether_aggression aggr,
                                     uint8_t** out_code,
                                     size_t* out_len);

/**
 * Morph a raw binary file (entire file as code buffer).
 * Writes output file; for ELF/PE prefer extracting .text first via tools.
 */
aether_status aether_morph_file(const char* in_path,
                                const char* out_path,
                                aether_aggression aggr);

/** Free buffer from aether_morph_buffer. */
void aether_free(void* p);

/**
 * Run morph bench (≥1k pure + ELF extract). Writes JSON report if path non-NULL.
 * @return AETHER_OK if break_rate==0 and corpus≥1000
 */
aether_status aether_run_bench(size_t pure_count,
                               const char* elf_path,
                               const char* json_out_path,
                               int* out_pass);

/**
 * Industry binary rewrite: morph ELF/PE .text in-place if size fits (nop pad).
 * Raw buffers may grow. out_path may be NULL to only validate (still needs writable?).
 * Writes out_path when non-NULL.
 */
aether_status aether_morph_binary_file(const char* in_path,
                                       const char* out_path,
                                       aether_product_mode product,
                                       aether_aggression aggr);

/** Industry product + rewrite self-tests. */
aether_status aether_industry_selftest(void);

/** Version string (static). */
const char* aether_version(void);

/** Honest one-line scope (static). */
const char* aether_scope(void);

/** Runtime API version (AETHER_API_VERSION). */
unsigned aether_api_version(void);

/**
 * True if client-compiled AETHER_API_VERSION is compatible with this library
 * (same major, client minor/patch <= library).
 */
int aether_api_compatible(unsigned client_api_version);

/**
 * Industry MorphEngine batch self-test (multi-policy pure verify).
 * @return AETHER_OK if framework self-test passes
 */
aether_status aether_framework_selftest(size_t pure_samples);

#ifdef __cplusplus
}
#endif

#endif /* AETHER_H */
