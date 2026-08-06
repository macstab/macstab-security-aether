#include "test_support.hpp"

#include "aether/api/aether.h"
#include "aether/common/rng.hpp"
#include "aether/meta/morph_engine.hpp"
#include "aether/meta/real_func_extract.hpp"

#include <cstdio>
#include <vector>

void test_morph_engine() {
    constexpr const char* kTest = "morph_engine";

    TEST_CHECK(kTest, aether::industry_framework_selftest(48));
    TEST_CHECK(kTest, aether_framework_selftest(32) == AETHER_OK);

    aether::seed_rng_u64(0xE001u);
    auto corpus = aether::generate_pure_corpus(40, 0xE001u);
    std::vector<std::vector<uint8_t>> inputs;
    for (const auto& ef : corpus) {
        // zero-arg pure only for generic batch config (args default 0)
        if (ef.pure_interpretable && ef.arg_rdi == 0 && ef.arg_rsi == 0)
            inputs.push_back(ef.bytes);
    }
    TEST_CHECK(kTest, inputs.size() >= 10);

    aether::MorphEngineConfig cfg;
    cfg.policy = aether::MorphPolicy::Safe;
    cfg.verify_pure = true;
    // Batch without per-item expected — engine will interpret input as expected
    aether::MorphEngine eng(cfg);
    auto results = eng.morph_batch(inputs, false);
    auto rep = aether::summarize_batch(results);
    std::fputs(aether::format_batch_report(rep).c_str(), stdout);
    TEST_CHECK(kTest, rep.jobs == results.size());
    TEST_CHECK(kTest, rep.fail == 0);
    TEST_CHECK(kTest, rep.pass);

    // Lab policy
    cfg.policy = aether::MorphPolicy::Lab;
    aether::MorphEngine lab(cfg);
    size_t ok = 0;
    for (size_t i = 0; i < inputs.size() && i < 20; i++) {
        auto r = lab.morph(inputs[i]);
        if (r.ok && r.pure_verified)
            ++ok;
    }
    TEST_CHECK(kTest, ok >= 15);

    // Stages recorded
    auto one = eng.morph(inputs[0]);
    TEST_CHECK(kTest, one.ok);
    TEST_CHECK(kTest, one.stages_run.size() >= 3);

    // Dual product: Industry experimental still produces output
    aether::MorphEngineConfig ind;
    ind.product = aether::ProductMode::IndustryExperimental;
    ind.policy = aether::MorphPolicy::Lab;
    ind.verify_pure = true;
    aether::MorphEngine ind_eng(ind);
    auto ir = ind_eng.morph(inputs[0]);
    TEST_CHECK(kTest, ir.ok && !ir.bytes.empty());

    uint8_t* out = nullptr;
    size_t out_len = 0;
    TEST_CHECK(kTest,
               aether_morph_buffer_ex(inputs[0].data(), inputs[0].size(), 0x1000,
                                      AETHER_PRODUCT_LAB, AETHER_AGGR_SAFE, &out, &out_len) ==
                   AETHER_OK);
    aether_free(out);
}
