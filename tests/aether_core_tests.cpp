/** @file aether_core_tests.cpp @brief Small runner for independently selectable unit cases. */

#include "test_support.hpp"

#include <cstdio>
#include <cstring>

void test_rng();
void test_noise();
void test_mutate();
void test_catalogue();
void test_decode_assemble();
void test_decode_real();
void test_equivalence();
void test_morph_real();
void test_morph_engine();
void test_binary_rewrite();
void test_ssa_analysis();
void test_bench();
void test_transforms();
void test_stages();
void test_pipeline();
void test_crypto_cascade();
void test_uniqueness();
void test_lazy_jsr();
void test_infect();
void test_stubs();

namespace aether::tests {
namespace {

int g_failures = 0;

} // namespace

void expect(bool condition, const char* expression, const char* test_name, int line) {
    if (!condition) {
        std::fprintf(stderr, "[FAIL] %s:%d: %s\n", test_name, line, expression);
        ++g_failures;
    }
}

int failure_count() {
    return g_failures;
}

} // namespace aether::tests

namespace {

struct TestCase {
    const char* name;
    void (*run)();
};

const TestCase kTestCases[] = {
    {"rng", test_rng},
    {"noise", test_noise},
    {"mutate", test_mutate},
    {"catalogue", test_catalogue},
    {"decode_assemble", test_decode_assemble},
    {"decode_real", test_decode_real},
    {"equivalence", test_equivalence},
    {"morph_real", test_morph_real},
    {"morph_engine", test_morph_engine},
    {"binary_rewrite", test_binary_rewrite},
    {"ssa_analysis", test_ssa_analysis},
    {"bench", test_bench},
    {"transforms", test_transforms},
    {"stages", test_stages},
    {"pipeline", test_pipeline},
    {"crypto_cascade", test_crypto_cascade},
    {"uniqueness", test_uniqueness},
    {"lazy_jsr", test_lazy_jsr},
    {"infect", test_infect},
    {"stubs", test_stubs},
};

} // namespace

int main(int argc, char** argv) {
    const char* selected = argc == 2 ? argv[1] : nullptr;
    if (argc > 2) {
        std::fprintf(stderr, "Usage: %s [test-case]\n", argv[0]);
        return 2;
    }

    bool ran = false;
    for (const auto& test : kTestCases) {
        if (selected && std::strcmp(selected, test.name) != 0)
            continue;
        ran = true;
        const int before = aether::tests::failure_count();
        test.run();
        if (aether::tests::failure_count() == before)
            std::printf("[PASS] %s\n", test.name);
    }

    if (!ran) {
        std::fprintf(stderr, "Unknown test case: %s\n", selected);
        return 2;
    }
    if (aether::tests::failure_count() == 0)
        return 0;

    std::fprintf(stderr, "[FAIL] %d assertion(s) failed\n", aether::tests::failure_count());
    return 1;
}
