#include "test_support.hpp"

#include "aether/common/rng.hpp"
#include "aether/meta/stages.hpp"

#include <vector>

void test_stages() {
    constexpr const char* kTest = "stages";
    aether::seed_rng();
    const std::vector<uint8_t> seed = {0x48, 0x31, 0xC0, 0x90, 0xC3};

    aether::StageReport single_report;
    const std::vector<uint8_t> single =
        aether::morph_stage(seed.data(), seed.size(), &single_report);
    TEST_CHECK(kTest, !single.empty());
    TEST_CHECK(kTest, single_report.bytes_in == seed.size());
    TEST_CHECK(kTest, single_report.bytes_out == single.size());

    std::vector<aether::StageReport> reports;
    const std::vector<uint8_t> multi =
        aether::multi_stage_morph(seed.data(), seed.size(), &reports, 2, 2, "test-morph");
    TEST_CHECK(kTest, !multi.empty());
    TEST_CHECK(kTest, reports.size() == 2);
    TEST_CHECK(kTest, reports[0].kind == "test-morph");
    TEST_CHECK(kTest, reports[1].kind == "test-morph");
}
