#include "test_support.hpp"

#include "aether/common/rng.hpp"
#include "aether/meta/assemble.hpp"
#include "aether/meta/decode.hpp"
#include "aether/meta/equiv.hpp"
#include "aether/meta/transforms.hpp"

#include <cstddef>
#include <set>
#include <string>

namespace {

size_t instruction_count(const aether::IRFunc& function) {
    size_t count = 0;
    for (const auto& block : function.blocks)
        count += block.insns.size();
    return count;
}

bool branch_targets_are_in_range(const aether::IRFunc& function) {
    for (const auto& block : function.blocks) {
        for (const auto& instruction : block.insns) {
            if ((instruction.op == aether::Op::JMP || instruction.op == aether::Op::JE) &&
                (instruction.target < 0 ||
                 instruction.target >= static_cast<int>(function.blocks.size()))) {
                return false;
            }
        }
        if (block.fallthrough >= (int)function.blocks.size())
            return false;
    }
    return true;
}

aether::IRFunc sample_function() {
    aether::IRFunc function;
    function.blocks.resize(3);
    function.entry = 0;

    function.blocks[0].id = 0;
    function.blocks[0].insns = {
        {aether::Op::NOP, 0, -1, {}},
        {aether::Op::JMP, 0, 2, {}},
    };
    function.blocks[0].fallthrough = -1;

    function.blocks[1].id = 1;
    function.blocks[1].insns = {{aether::Op::RET, 0, -1, {}}};
    function.blocks[1].fallthrough = -1;

    function.blocks[2].id = 2;
    function.blocks[2].insns = {
        {aether::Op::NOP, 0, -1, {}},
        {aether::Op::RET, 0, -1, {}},
    };
    function.blocks[2].fallthrough = -1;
    return function;
}

/** Richer IR for multi-pass permute uniqueness. */
aether::IRFunc rich_function() {
    const uint8_t bytes[] = {
        0x48, 0x31, 0xC0, 0x90, 0x90, 0x90, 0xB8, 0x01, 0x00, 0x00, 0x00, 0x48, 0xFF, 0xC0,
        0x50, 0x58, 0x48, 0x31, 0xC9, 0x90, 0x66, 0x90, 0x0F, 0x1F, 0x00, 0x48, 0x29, 0xC0,
        0xEB, 0x03, 0x90, 0x90, 0x90, 0x48, 0xFF, 0xC0, 0xC3, 0x90, 0x90, 0x48, 0x31, 0xC0,
        0x90, 0xC3,
    };
    aether::IRFunc f = aether::disasm(bytes, sizeof(bytes));
    aether::expand(f);
    aether::expand(f);
    return f;
}

} // namespace

void test_transforms() {
    constexpr const char* kTest = "transforms";
    aether::seed_rng();
    aether::IRFunc function = sample_function();

    aether::permute_blocks(function);
    TEST_CHECK(kTest, function.blocks.size() == 3);
    TEST_CHECK(kTest, branch_targets_are_in_range(function));

    // Def-use independence
    TEST_CHECK(kTest, aether::insn_pair_independent(aether::Op::NOP, aether::Op::JUNK));
    TEST_CHECK(kTest, !aether::insn_pair_independent(aether::Op::MOV_RAX_IMM, aether::Op::INC_RAX));
    TEST_CHECK(kTest, !aether::insn_pair_independent(aether::Op::CMP_STATE, aether::Op::JE));
    {
        const uint8_t seed[] = {0xB8, 0x07, 0x00, 0x00, 0x00, 0xC3};
        aether::IRFunc pure = aether::disasm(seed, sizeof(seed));
        aether::expand(pure);
        aether::safe_permute_insns(pure);
        auto v = aether::interpret_rax(pure);
        TEST_CHECK(kTest, v.has_value() && *v == 7u);
    }

    aether::flatten(function);
    TEST_CHECK(kTest, function.entry == 1);
    TEST_CHECK(kTest, function.blocks.size() >= 5);
    TEST_CHECK(kTest, branch_targets_are_in_range(function));

    const size_t before_expand = instruction_count(function);
    aether::expand(function);
    TEST_CHECK(kTest, instruction_count(function) >= before_expand);
    const size_t before_shrink = instruction_count(function);
    aether::shrink(function);
    TEST_CHECK(kTest, instruction_count(function) <= before_shrink);

    // --- B: multi-pass permute streams + world_class_permute ---
    {
        aether::IRFunc rich = rich_function();
        TEST_CHECK(kTest, !rich.blocks.empty());

        aether::IRFunc a = rich;
        aether::seed_rng_u64(0x1111);
        aether::world_class_permute(a);
        TEST_CHECK(kTest, branch_targets_are_in_range(a));

        aether::IRFunc b = rich;
        aether::seed_rng_u64(0x2222);
        aether::world_class_permute(b);
        TEST_CHECK(kTest, branch_targets_are_in_range(b));

        auto ca = aether::assemble(a);
        auto cb = aether::assemble(b);
        TEST_CHECK(kTest, aether::hash64(ca) != aether::hash64(cb) || ca.size() != cb.size());
    }

    // Multi-pass uniqueness: distinct stream seeds per pass
    {
        aether::IRFunc rich = rich_function();
        std::set<uint64_t> hashes;
        constexpr int kN = 100;
        for (int i = 0; i < kN; i++) {
            aether::IRFunc f = rich;
            aether::seed_rng_u64(0xA11CE000ULL + (uint64_t)i); // multi-pass stream A
            aether::world_class_permute(f);
            aether::seed_rng_u64(0xB22DE000ULL + (uint64_t)i * 17); // multi-pass stream B
            aether::apply_layer_mode(f, aether::LayerMode::PermuteHeavy);
            TEST_CHECK(kTest, branch_targets_are_in_range(f));
            hashes.insert(aether::hash64(aether::assemble(f)));
        }
        TEST_CHECK(kTest, (int)hashes.size() >= kN - 3);
        TEST_CHECK(kTest, (int)hashes.size() > kN * 85 / 100);
    }

    // Named modes
    {
        aether::IRFunc f = rich_function();
        aether::apply_layer_mode(f, aether::LayerMode::PermuteHeavy);
        TEST_CHECK(kTest, branch_targets_are_in_range(f));
        TEST_CHECK(kTest, aether::layer_mode_name(aether::LayerMode::PermuteHeavy) != nullptr);
        TEST_CHECK(kTest,
                   std::string(aether::layer_mode_name(aether::LayerMode::PermuteHeavy)) ==
                       "permute-heavy");
    }
}
