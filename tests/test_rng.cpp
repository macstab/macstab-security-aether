#include "test_support.hpp"

#include "aether/common/rng.hpp"

void test_rng() {
    constexpr const char* kTest = "rng";
    aether::seed_rng();

    TEST_CHECK(kTest, &aether::rng() == &aether::rng());
    for (int i = 0; i < 512; ++i) {
        const int value = aether::rnd(-7, 11);
        TEST_CHECK(kTest, value >= -7);
        TEST_CHECK(kTest, value <= 11);
    }
}
