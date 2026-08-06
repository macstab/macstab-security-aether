#include "test_support.hpp"

#include "aether/meta/assemble.hpp"
#include "aether/meta/decode.hpp"

#include <vector>

namespace {

bool contains_op(const aether::IRFunc& function, aether::Op op) {
    for (const auto& block : function.blocks) {
        for (const auto& instruction : block.insns) {
            if (instruction.op == op)
                return true;
        }
    }
    return false;
}

} // namespace

void test_decode_assemble() {
    constexpr const char* kTest = "decode_assemble";
    const std::vector<uint8_t> seed = {
        0x48,
        0x31,
        0xC0, // xor rax, rax
        0xB8,
        0x02,
        0x00,
        0x00,
        0x00, // mov eax, 2
        0x48,
        0xFF,
        0xC0, // inc rax
        0xC3, // ret
    };

    const aether::IRFunc decoded = aether::disasm(seed.data(), seed.size());
    TEST_CHECK(kTest, decoded.entry == 0);
    TEST_CHECK(kTest, !decoded.blocks.empty());
    TEST_CHECK(kTest, contains_op(decoded, aether::Op::CLEAR_RAX));
    TEST_CHECK(kTest, contains_op(decoded, aether::Op::MOV_RAX_IMM));
    TEST_CHECK(kTest, contains_op(decoded, aether::Op::INC_RAX));
    TEST_CHECK(kTest, contains_op(decoded, aether::Op::RET));

    const std::vector<uint8_t> assembled = aether::assemble(decoded);
    TEST_CHECK(kTest, !assembled.empty());
    const aether::IRFunc redecoded = aether::disasm(assembled.data(), assembled.size());
    TEST_CHECK(kTest, contains_op(redecoded, aether::Op::RET));
}
