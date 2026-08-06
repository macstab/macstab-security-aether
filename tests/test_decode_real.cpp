#include "test_support.hpp"

#include "aether/meta/decode_real.hpp"
#include "aether/meta/elf_view.hpp"

#include <cstring>
#include <string>

void test_decode_real() {
    constexpr const char* kTest = "decode_real";
    TEST_CHECK(kTest, aether::has_real_disasm());

    // Classic x86-64 snippet: xor rax,rax; nop; ret
    const uint8_t code[] = {0x48, 0x31, 0xC0, 0x90, 0xC3};
    aether::RealFunc f = aether::disasm_real(code, sizeof(code), 0x401000);
    TEST_CHECK(kTest, f.insns.size() >= 3);
    TEST_CHECK(kTest, f.blocks.size() >= 1);
    TEST_CHECK(kTest, f.insns[0].length == 3);
    TEST_CHECK(kTest, f.insns.back().is_ret || f.insns.back().text.find("ret") != std::string::npos);

    // Branch target resolution: jmp +0 (eb 00)
    const uint8_t jmp[] = {0xEB, 0x00, 0x90, 0xC3};
    aether::RealFunc j = aether::disasm_real(jmp, sizeof(jmp), 0x1000);
    TEST_CHECK(kTest, j.insns.size() >= 2);
    bool found_jmp = false;
    for (const auto& in : j.insns) {
        if (in.is_uncond_jump && in.has_imm_target) {
            found_jmp = true;
            TEST_CHECK(kTest, in.branch_target == 0x1002); // after eb 00
        }
    }
    TEST_CHECK(kTest, found_jmp);

    // ELF path if victim_clean present
    aether::ElfTextRegion reg = aether::load_elf64_text("victim_clean");
    if (!reg.ok) {
        // try from build cwd parent
        reg = aether::load_elf64_text("../victim_clean");
    }
    if (reg.ok) {
        TEST_CHECK(kTest, reg.size > 0);
        const uint8_t* p = aether::elf_text_data(reg);
        TEST_CHECK(kTest, p != nullptr);
        size_t n = reg.size > 4096 ? 4096 : reg.size;
        aether::RealFunc ef = aether::disasm_real(p, n, reg.vaddr);
        TEST_CHECK(kTest, ef.insns.size() > 10);
        TEST_CHECK(kTest, ef.blocks.size() >= 1);
        TEST_CHECK(kTest, !ef.insns[0].text.empty());
    }
}
