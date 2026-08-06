#include "test_support.hpp"

#include "aether/common/rng.hpp"
#include "aether/meta/decode_real.hpp"
#include "aether/meta/morph_real.hpp"

#include <set>
#include <vector>

void test_morph_real() {
    constexpr const char* kTest = "morph_real";

    // mov eax, 7; ret
    const uint8_t seed[] = {0xB8, 0x07, 0x00, 0x00, 0x00, 0xC3};

    aether::RealFunc base = aether::disasm_real(seed, sizeof(seed), 0x1000);
    TEST_CHECK(kTest, !base.insns.empty());
    auto g = aether::interpret_real_pure(base);
    TEST_CHECK(kTest, g.has_value() && *g == 7u);

    aether::seed_rng_u64(0x1B000001u);
    std::set<uint64_t> hashes;
    for (int i = 0; i < 40; i++) {
        auto code = aether::morph_real_restricted(seed, sizeof(seed), 0x1000);
        TEST_CHECK(kTest, !code.empty());
        hashes.insert(aether::hash64(code));
        aether::RealFunc m = aether::disasm_real(code.data(), code.size(), 0x1000);
        auto v = aether::interpret_real_pure(m);
        TEST_CHECK(kTest, v.has_value());
        if (v)
            TEST_CHECK(kTest, *v == 7u);
    }
    // Diversity: restricted morph must not be a pure no-op pipeline
    TEST_CHECK(kTest, hashes.size() >= 5);

    // jmp-skip pure: xor rax; jmp +3; nops; ret → 0
    const uint8_t jmp_skip[] = {0x48, 0x31, 0xC0, 0xEB, 0x03, 0x90, 0x90, 0x90, 0xC3};
    auto jcode = aether::morph_real_restricted(jmp_skip, sizeof(jmp_skip), 0x2000);
    TEST_CHECK(kTest, !jcode.empty());
    auto jv = aether::interpret_real_pure(aether::disasm_real(jcode.data(), jcode.size(), 0x2000));
    TEST_CHECK(kTest, jv.has_value() && *jv == 0u);
}
