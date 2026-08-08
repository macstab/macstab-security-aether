/**
 * SPDX-License-Identifier: LicenseRef-Aether-Research
 * Copyright (c) 2024–2026 Macstab GmbH.
 *
 * Licensed under the Aether Research License in the repository root file
 * LICENSE. Authorized lab / lawful private use only as defined there.
 * By using this file you accept LICENSE. See docs/LICENSE_ACCEPTANCE.md.
 */

/**
 * Industry-style morph CLI: in-buffer/file → morphed out + optional JSON stats.
 *
 *   aether_morph --in seed.bin --out morph.bin [--policy safe|lab|identity]
 *   aether_morph --hex B807000000C3 --out morph.bin
 */
#include "aether/common/rng.hpp"
#include "aether/meta/decode_real.hpp"
#include "aether/meta/morph_real.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace {

std::vector<uint8_t> read_file(const char* path) {
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return {};
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)),
                                std::istreambuf_iterator<char>());
}

bool write_file(const char* path, const std::vector<uint8_t>& data) {
    std::ofstream out(path, std::ios::binary);
    if (!out)
        return false;
    out.write(reinterpret_cast<const char*>(data.data()), (std::streamsize)data.size());
    return true;
}

int hex_nibble(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

std::vector<uint8_t> parse_hex(const char* s) {
    std::vector<uint8_t> out;
    int hi = -1;
    for (const char* p = s; *p; p++) {
        if (*p == ' ' || *p == '\n' || *p == '\t')
            continue;
        int n = hex_nibble(*p);
        if (n < 0)
            return {};
        if (hi < 0)
            hi = n;
        else {
            out.push_back((uint8_t)((hi << 4) | n));
            hi = -1;
        }
    }
    if (hi >= 0)
        return {};
    return out;
}

aether::MorphPolicy parse_policy(const char* s) {
    if (!s)
        return aether::MorphPolicy::Safe;
    if (!std::strcmp(s, "identity") || !std::strcmp(s, "0"))
        return aether::MorphPolicy::Identity;
    if (!std::strcmp(s, "lab") || !std::strcmp(s, "2"))
        return aether::MorphPolicy::Lab;
    return aether::MorphPolicy::Safe;
}

} // namespace

int main(int argc, char** argv) {
    const char* in_path = nullptr;
    const char* out_path = "morph.bin";
    const char* hex = nullptr;
    const char* policy_s = "safe";
    uint64_t seed = 0;

    for (int i = 1; i < argc; i++) {
        if (!std::strcmp(argv[i], "--in") && i + 1 < argc)
            in_path = argv[++i];
        else if (!std::strcmp(argv[i], "--out") && i + 1 < argc)
            out_path = argv[++i];
        else if (!std::strcmp(argv[i], "--hex") && i + 1 < argc)
            hex = argv[++i];
        else if (!std::strcmp(argv[i], "--policy") && i + 1 < argc)
            policy_s = argv[++i];
        else if (!std::strcmp(argv[i], "--seed") && i + 1 < argc)
            seed = std::strtoull(argv[++i], nullptr, 0);
        else if (!std::strcmp(argv[i], "--help")) {
            std::printf(
                "Usage: %s (--in file | --hex HEX) [--out file] [--policy safe|lab|identity]\n",
                argv[0]);
            return 0;
        }
    }

    std::vector<uint8_t> in;
    if (hex)
        in = parse_hex(hex);
    else if (in_path)
        in = read_file(in_path);
    else {
        std::fprintf(stderr, "need --in or --hex\n");
        return 2;
    }
    if (in.empty()) {
        std::fprintf(stderr, "empty input\n");
        return 2;
    }

    if (seed)
        aether::seed_rng_u64(seed);
    else
        aether::seed_rng();

    auto pol = parse_policy(policy_s);
    auto out = aether::morph_real(in.data(), in.size(), 0x1000, pol);
    if (out.empty()) {
        std::fprintf(stderr, "morph failed\n");
        return 1;
    }
    if (!write_file(out_path, out)) {
        std::fprintf(stderr, "write failed: %s\n", out_path);
        return 1;
    }

    auto before = aether::interpret_real_pure(aether::disasm_real(in.data(), in.size(), 0x1000));
    auto after = aether::interpret_real_pure(aether::disasm_real(out.data(), out.size(), 0x1000));
    std::printf("in=%zu out=%zu policy=%s", in.size(), out.size(), policy_s);
    if (before && after)
        std::printf(" pure_rax %u -> %u %s", *before, *after, (*before == *after) ? "OK" : "BREAK");
    std::printf("\n wrote %s\n", out_path);
    return (before && after && *before != *after) ? 1 : 0;
}
