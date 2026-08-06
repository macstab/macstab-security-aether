#include "test_support.hpp"

#include "aether/api/aether.h"
#include "aether/meta/binary_rewrite.hpp"
#include "aether/meta/morph_engine.hpp"
#include "aether/meta/real_func_extract.hpp"

void test_binary_rewrite() {
    constexpr const char* kTest = "binary_rewrite";

    TEST_CHECK(kTest, aether::industry_rewrite_selftest());
    TEST_CHECK(kTest, aether::industry_product_selftest(32));
    TEST_CHECK(kTest, aether::industry_finish_selftest());
    TEST_CHECK(kTest, aether_industry_selftest() == AETHER_OK);

    auto corpus = aether::generate_pure_corpus(12, 0xC0DEu);
    aether::MorphEngineConfig cfg;
    cfg.product = aether::ProductMode::Industry;
    cfg.policy = aether::MorphPolicy::Safe;
    cfg.verify_pure = true;
    cfg.multi_input_verify = true;
    size_t ok = 0;
    for (const auto& ef : corpus) {
        if (!ef.pure_interpretable)
            continue;
        cfg.arg_rdi = ef.arg_rdi;
        cfg.arg_rsi = ef.arg_rsi;
        auto rr = aether::rewrite_binary_buffer(ef.bytes.data(), ef.bytes.size(), cfg);
        if (rr.ok)
            ++ok;
    }
    TEST_CHECK(kTest, ok >= 5);

    // detect formats
    const uint8_t raw[] = {0x31, 0xC0, 0xC3};
    TEST_CHECK(kTest, aether::detect_binary_format(raw, 3) == aether::BinaryFormat::Raw);
}
