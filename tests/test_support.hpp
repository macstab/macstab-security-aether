#pragma once

namespace aether::tests {

void expect(bool condition, const char* expression, const char* test_name, int line);
int failure_count();

} // namespace aether::tests

#define TEST_CHECK(test_name, expression)                                                          \
    aether::tests::expect((expression), #expression, (test_name), __LINE__)
