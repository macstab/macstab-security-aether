#include "test_support.hpp"

#include "aether/common/rng.hpp"
#include "aether/runtime/lazy_jsr.hpp"

#include <vector>

void test_lazy_jsr() {
    constexpr const char* kTest = "lazy_jsr";
    const std::vector<uint8_t> seed = {0x48, 0x31, 0xC0, 0xC3};

    aether::LazyConfig idle_only;
    idle_only.idle_only = true;
    idle_only.max_idle_rounds = 2;
    idle_only.post_clear_rounds = 0;
    const aether::LazyResult idle =
        aether::run_lazy_jsr(seed.data(), seed.size(), idle_only, nullptr);
    TEST_CHECK(kTest, idle.idle_rounds == 2);
    TEST_CHECK(kTest, !idle.trigger_armed);
    TEST_CHECK(kTest, !idle.generated);

    aether::seed_rng();
    aether::LazyConfig fire;
    fire.force_fire = true;
    fire.max_idle_rounds = 8;
    fire.post_clear_rounds = 0;
    fire.min_layers = 1;
    fire.max_layers = 1;
    fire.continue_pct = 0;
    std::vector<uint8_t> body;
    const aether::LazyResult generated =
        aether::run_lazy_jsr(seed.data(), seed.size(), fire, &body);
    TEST_CHECK(kTest, generated.trigger_armed);
    TEST_CHECK(kTest, generated.payload_entered);
    TEST_CHECK(kTest, generated.jsr_cleared);
    TEST_CHECK(kTest, generated.generated);
    TEST_CHECK(kTest, generated.wiped);
    TEST_CHECK(kTest, generated.layers == 1);
    TEST_CHECK(kTest, !body.empty());
}
