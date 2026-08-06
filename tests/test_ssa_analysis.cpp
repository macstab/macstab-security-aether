#include "test_support.hpp"

#include "aether/meta/decode_real.hpp"
#include "aether/meta/morph_engine.hpp"
#include "aether/meta/ssa_analysis.hpp"

#include <vector>

void test_ssa_analysis() {
    constexpr const char* kTest = "ssa_analysis";
    // xor eax,eax; ret
    std::vector<uint8_t> code = {0x31, 0xC0, 0xC3};
    aether::RealFunc f = aether::disasm_real(code.data(), code.size(), 0x1000);
    aether::FuncSsa s = aether::analyze_func_ssa(f);
    TEST_CHECK(kTest, s.analysis_ok);
    TEST_CHECK(kTest, s.per_insn.size() >= 2);

    // Independent: nop and pure if we had two pure different regs — push rax; xor ecx,ecx
    std::vector<uint8_t> c2 = {0x50, 0x31, 0xC9, 0x58, 0xC3};
    aether::RealFunc f2 = aether::disasm_real(c2.data(), c2.size(), 0x1000);
    aether::FuncSsa s2 = aether::analyze_func_ssa(f2);
    TEST_CHECK(kTest, s2.analysis_ok);
    // push and xor ecx: independent
    if (s2.per_insn.size() >= 2)
        TEST_CHECK(kTest, aether::ssa_pair_independent(s2, 0, 1));

    aether::MorphEngineConfig cfg;
    cfg.policy = aether::MorphPolicy::Safe;
    cfg.verify_pure = true;
    aether::MorphEngine eng(cfg);
    auto r = eng.morph(code);
    TEST_CHECK(kTest, r.ok && r.pure_verified);
}
