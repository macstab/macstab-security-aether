#include "test_support.hpp"

#include "aether/infect/infect.hpp"

#include <string>
#include <unistd.h>
#include <vector>

void test_infect() {
    constexpr const char* kTest = "infect";
    const std::string missing_path =
        "/private/tmp/aether-test-missing-" + std::to_string(getpid()) + "/target";
    const std::vector<uint8_t> payload = {0x90};

    // Only the no-write error path is covered: success intentionally mutates its target.
    TEST_CHECK(kTest, aether::infect(missing_path.c_str(), payload) != 0);
}
