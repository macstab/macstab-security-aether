#include "test_support.hpp"

#include "architecture_stubs.hpp"

void test_stubs() {
    constexpr const char* kTest = "stubs";
    using namespace aether::stubs;

    TEST_CHECK(kTest, !ProcessHollowing::hollow_process("example"));
    TEST_CHECK(kTest, !ProcessHollowing::module_stomp("example"));
    TEST_CHECK(kTest, !EBPFRootkit::load_hide_programs());
    TEST_CHECK(kTest, !EBPFRootkit::hide_from_bpftool());
    TEST_CHECK(kTest, !FilelessLOTL::memfd_exec({}));
    TEST_CHECK(kTest, !FilelessLOTL::living_off_the_land());
    TEST_CHECK(kTest, !C2Persistence::start_c2());
    TEST_CHECK(kTest, !C2Persistence::install_persistence());
    TEST_CHECK(kTest, !C2Persistence::anti_forensics());
}
