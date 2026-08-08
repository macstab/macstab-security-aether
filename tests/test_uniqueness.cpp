/**
 * @file test_uniqueness.cpp
 * @brief A: prove generations are always different across many runs.
 */

#include "test_support.hpp"

#include "aether/common/rng.hpp"
#include "aether/meta/assemble.hpp"
#include "aether/meta/crypto_cascade.hpp"
#include "aether/meta/decode.hpp"
#include "aether/meta/stages.hpp"
#include "aether/meta/transforms.hpp"

#include <set>
#include <vector>

namespace {

const uint8_t kSeed[] = {
    0x48,
    0x31,
    0xC0,
    0x90,
    0xB8,
    0x01,
    0x00,
    0x00,
    0x00,
    0x48,
    0xFF,
    0xC0,
    0x50,
    0x58,
    0xC3,
};

} // namespace

void test_uniqueness() {
    constexpr const char* kTest = "uniqueness";
    constexpr int kRuns = 200;

    // --- morph_stage uniqueness ---
    {
        std::set<uint64_t> hashes;
        for (int i = 0; i < kRuns; i++) {
            aether::seed_rng(); // full entropy reseed each "execution"
            auto code = aether::morph_stage(kSeed, sizeof(kSeed), nullptr);
            TEST_CHECK(kTest, !code.empty());
            hashes.insert(aether::hash64(code));
        }
        TEST_CHECK(kTest, (int)hashes.size() == kRuns);
    }

    // --- cascade outer package uniqueness ---
    {
        std::set<uint64_t> hashes;
        const std::vector<uint8_t> leaf(kSeed, kSeed + sizeof(kSeed));
        for (int i = 0; i < kRuns; i++) {
            aether::seed_rng();
            auto onion = aether::cascade_build(leaf, 4, nullptr);
            TEST_CHECK(kTest, aether::is_cascade_package(onion));
            hashes.insert(aether::hash64(onion));
        }
        TEST_CHECK(kTest, (int)hashes.size() == kRuns);
    }

    // --- world_class_permute output uniqueness (fixed IR, many streams) ---
    {
        std::set<uint64_t> hashes;
        aether::IRFunc base = aether::disasm(kSeed, sizeof(kSeed));
        // Grow surface so permute has room to differ.
        aether::expand(base);
        aether::expand(base);

        for (int i = 0; i < kRuns; i++) {
            aether::seed_rng_u64(0xAE711400ULL ^ (uint64_t)(i * 0x10007));
            aether::IRFunc f = base;
            aether::world_class_permute(f);
            auto code = aether::assemble(f);
            TEST_CHECK(kTest, !code.empty());
            hashes.insert(aether::hash64(code));
        }
        // Allow a tiny collision budget on small IR, but require high uniqueness.
        TEST_CHECK(kTest, (int)hashes.size() >= kRuns - 2);
        TEST_CHECK(kTest, (int)hashes.size() > kRuns * 9 / 10);
    }
}
