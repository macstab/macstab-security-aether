#include "test_support.hpp"

#include "aether/common/rng.hpp"
#include "aether/meta/catalogue.hpp"

#include <vector>

void test_catalogue() {
    constexpr const char* kTest = "catalogue";
    aether::seed_rng();

    TEST_CHECK(kTest, !aether::cat_clear_rax().empty());
    TEST_CHECK(kTest, !aether::cat_clear_rcx().empty());
    TEST_CHECK(kTest, !aether::cat_nop().empty());
    TEST_CHECK(kTest, !aether::cat_junk().empty());

    std::vector<uint8_t> bytes;
    aether::emit_form(bytes, aether::pick(aether::cat_nop()));
    TEST_CHECK(kTest, !bytes.empty());

    bytes.clear();
    aether::emit_u32(bytes, 0x78563412U);
    TEST_CHECK(kTest, bytes == std::vector<uint8_t>({0x12, 0x34, 0x56, 0x78}));

    bytes.clear();
    aether::emit_mov_rax_imm(bytes, 3U);
    TEST_CHECK(kTest, bytes.size() == 5);
    TEST_CHECK(kTest, bytes[0] == 0xB8);
}
