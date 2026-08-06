#include "test_support.hpp"

#include "aether/common/rng.hpp"
#include "aether/meta/pipeline.hpp"

#include <vector>

void test_pipeline() {
    constexpr const char* kTest = "pipeline";
    aether::seed_rng();
    const std::vector<uint8_t> seed = {
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
        0xC3,
    };

    const std::vector<uint8_t> output = aether::full_pipeline(seed.data(), seed.size());
    const aether::PipelineStats& stats = aether::last_pipeline_stats();
    TEST_CHECK(kTest, !output.empty());
    TEST_CHECK(kTest, stats.bytes_out == output.size());
    TEST_CHECK(kTest, stats.blocks_in >= 1);
    TEST_CHECK(kTest, stats.insns_in >= 1);
    TEST_CHECK(kTest, stats.stages >= 2);
    TEST_CHECK(kTest, !aether::last_stage_reports().empty());
}
