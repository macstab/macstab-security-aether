/**
 * @file test_crypto_cascade.cpp
 * @brief C: cascade round-trip, multi-depth, uniqueness, peel correctness.
 */

#include "test_support.hpp"

#include "aether/common/rng.hpp"
#include "aether/meta/crypto_cascade.hpp"

#include <cstring>
#include <set>
#include <vector>

void test_crypto_cascade() {
    constexpr const char* kTest = "crypto_cascade";
    aether::seed_rng();
    const std::vector<uint8_t> leaf = {0x90, 0x48, 0x31, 0xC0, 0xC3};

    // Round-trip 3 layers
    {
        std::vector<aether::CascadeStep> build_steps;
        std::vector<uint8_t> package = aether::cascade_build(leaf, 3, &build_steps);

        TEST_CHECK(kTest, aether::is_cascade_package(package));
        TEST_CHECK(kTest, build_steps.size() == 3);

        std::vector<aether::CascadeStep> peel_steps;
        TEST_CHECK(kTest, aether::cascade_peel(package, &peel_steps));
        TEST_CHECK(kTest, package == leaf);
        TEST_CHECK(kTest, peel_steps.size() == 3);
        TEST_CHECK(kTest, peel_steps.back().is_leaf);
    }

    // Round-trip many depths
    for (int depth = 1; depth <= 8; depth++) {
        aether::seed_rng();
        auto package = aether::cascade_build(leaf, depth, nullptr);
        TEST_CHECK(kTest, aether::is_cascade_package(package) || depth == 0);
        TEST_CHECK(kTest, aether::cascade_peel(package, nullptr));
        TEST_CHECK(kTest, package == leaf);
    }

    // Stream crypt is involution
    {
        aether::seed_rng_u64(42);
        std::vector<uint8_t> buf = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        uint8_t key[32];
        uint8_t nonce[12];
        for (int i = 0; i < 32; i++)
            key[i] = (uint8_t)i;
        for (int i = 0; i < 12; i++)
            nonce[i] = (uint8_t)(100 + i);
        auto orig = buf;
        aether::stream_crypt(buf, key, nonce);
        TEST_CHECK(kTest, buf != orig);
        aether::stream_crypt(buf, key, nonce);
        TEST_CHECK(kTest, buf == orig);
    }

    // Truncated package fails peel
    {
        std::vector<uint8_t> truncated(53, 0);
        std::memcpy(truncated.data(), "AeC1", 4);
        truncated[49] = 1;
        TEST_CHECK(kTest, aether::is_cascade_package(truncated));
        TEST_CHECK(kTest, !aether::cascade_peel(truncated));
    }

    // Onion uniqueness across builds (same leaf, different keys)
    {
        std::set<uint64_t> hashes;
        constexpr int kN = 50;
        for (int i = 0; i < kN; i++) {
            aether::seed_rng();
            auto p = aether::cascade_build(leaf, 5, nullptr);
            hashes.insert(aether::hash64(p));
        }
        TEST_CHECK(kTest, (int)hashes.size() == kN);
    }
}
