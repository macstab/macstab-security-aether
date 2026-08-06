#include "test_support.hpp"

#include "aether/mutate/mutate.hpp"

#include <algorithm>
#include <array>

void test_mutate() {
    constexpr const char* kTest = "mutate";
    aether::continuous_mutate(nullptr, 0);

    std::array<uint8_t, 64> bytes{};
    aether::continuous_mutate(bytes.data(), bytes.size());
    const bool changed =
        std::any_of(bytes.begin(), bytes.end(), [](uint8_t value) { return value != 0; });
    TEST_CHECK(kTest, changed);
}
