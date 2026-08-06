#include "test_support.hpp"

#include "aether/common/rng.hpp"
#include "aether/noise/noise.hpp"

void test_noise() {
    constexpr const char* kTest = "noise";
    aether::seed_rng();
    aether::do_random_noise(1);
    TEST_CHECK(kTest, true);
}
