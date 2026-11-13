/**
 * @file test_main.cpp
 * @brief Runs every registered host test; the exit code is non-zero when one
 * fails.
 */
#include <cinttypes>
#include <cstdint>
#include <cstdio>

#include "test_harness.hpp"

int main() {
  ::ut::Registry& r = ::ut::GetRegistry();
  int32_t failed_cases = 0;
  for (int32_t i = 0; i < r.count; ++i) {
    const int32_t before = r.failures;
    static_cast<void>(std::printf("[ RUN  ] %s\n", r.tests[i].name));
    r.tests[i].fn();
    const bool ok = r.failures == before;
    static_cast<void>(std::printf("[ %s ] %s\n", ok ? " OK " : "FAIL", r.tests[i].name));
    if (!ok) {
      ++failed_cases;
    }
  }
  static_cast<void>(
      std::printf("%" PRId32 " test cases, %" PRId32 " failed\n", r.count, failed_cases));
  return ((failed_cases == 0) && (r.count > 0)) ? 0 : 1;
}
