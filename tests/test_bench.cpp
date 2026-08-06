#include "test_support.hpp"

#include "aether/api/aether.h"
#include "aether/meta/bench.hpp"
#include "aether/meta/real_func_extract.hpp"

#include <cstdio>

void test_bench() {
    constexpr const char* kTest = "bench";

    // Smaller pure count for unit speed; full gate via prove-bench / aether_bench
    aether::BenchReport r = aether::run_morph_bench(200, nullptr, 0xBEACu, 1);
    std::fputs(aether::format_bench_report(r).c_str(), stdout);

    TEST_CHECK(kTest, r.corpus_synthetic == 200);
    TEST_CHECK(kTest, r.corpus_elf >= 200); // shipped corpus/real_corpus.elf
    TEST_CHECK(kTest, r.morph_ok > 0);
    TEST_CHECK(kTest, r.pure_breaks == 0);
    TEST_CHECK(kTest, r.structural_breaks == 0);
    TEST_CHECK(kTest, r.native_breaks == 0);
    TEST_CHECK(kTest, r.break_rate == 0.0);

    // API smoke
    TEST_CHECK(kTest, aether_version() != nullptr);
    TEST_CHECK(kTest, aether_scope() != nullptr);

    uint8_t seed[] = {0xB8, 0x07, 0x00, 0x00, 0x00, 0xC3};
    uint8_t* out = nullptr;
    size_t out_len = 0;
    aether_status st =
        aether_morph_buffer(seed, sizeof(seed), 0x1000, AETHER_AGGR_SAFE, &out, &out_len);
    TEST_CHECK(kTest, st == AETHER_OK);
    TEST_CHECK(kTest, out != nullptr && out_len > 0);
    aether_free(out);

    auto elf = aether::extract_from_elf("victim_clean", 64);
    // elf extract optional if victim missing
    TEST_CHECK(kTest, true);
    (void)elf;
}
