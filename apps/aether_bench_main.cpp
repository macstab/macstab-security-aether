/**
 * SPDX-License-Identifier: LicenseRef-Aether-Research
 * Copyright (c) 2024–2026 Macstab GmbH.
 *
 * Licensed under the Aether Research License in the repository root file
 * LICENSE. Authorized lab / lawful private use only as defined there.
 * By using this file you accept LICENSE. See docs/LICENSE_ACCEPTANCE.md.
 */

/**
 * @file aether_bench_main.cpp
 * @brief One-command morph benchmark (≥1k corpus, break-rate, JSON).
 *
 * Usage:
 *   aether_bench [--count N] [--elf path] [--json path] [--rounds R]
 */
#include "aether/meta/bench.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

int main(int argc, char** argv) {
    size_t count = 1000;
    const char* elf = "victim_clean";
    std::string json = "artifacts/bench_report.json";
    int rounds = 2;

    for (int i = 1; i < argc; i++) {
        if (!std::strcmp(argv[i], "--count") && i + 1 < argc)
            count = (size_t)std::strtoul(argv[++i], nullptr, 10);
        else if (!std::strcmp(argv[i], "--elf") && i + 1 < argc)
            elf = argv[++i];
        else if (!std::strcmp(argv[i], "--json") && i + 1 < argc)
            json = argv[++i];
        else if (!std::strcmp(argv[i], "--rounds") && i + 1 < argc)
            rounds = (int)std::strtol(argv[++i], nullptr, 10);
        else if (!std::strcmp(argv[i], "--help")) {
            std::printf("Usage: %s [--count N] [--elf path] [--json path] [--rounds R]\n",
                        argv[0]);
            return 0;
        }
    }

    aether::BenchReport r = aether::run_morph_bench(count, elf, 0xBEACu, rounds);
    std::fputs(aether::format_bench_report(r).c_str(), stdout);
    if (!aether::write_bench_report_json(r, json)) {
        // try creating parent via relative
        if (!aether::write_bench_report_json(r, "bench_report.json"))
            std::fprintf(stderr, "warn: could not write JSON report\n");
        else
            std::printf("wrote bench_report.json\n");
    } else {
        std::printf("wrote %s\n", json.c_str());
    }
    return r.pass ? 0 : 1;
}
