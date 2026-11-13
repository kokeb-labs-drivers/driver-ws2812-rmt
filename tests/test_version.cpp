/**
 * @file test_version.cpp
 * @brief The version getters follow the generated ws2812_rmt_version.h, so they
 * cannot drift from CMake.
 *
 * Built twice by tests/CMakeLists.txt: against the real generated header (the
 * getters must equal the CMake project version, passed as EXPECTED_*), and
 * against a probe header generated from the same template with version 9.8.7 (a
 * getter that returns a literal instead of reading the header fails there).
 */
#include <cstdio>
#include <cstring>

#include "ws2812_rmt_types.hpp"

static_assert(ws2812_rmt::GetDriverVersionMajor() == EXPECTED_MAJOR,
              "major version differs from CMake");
static_assert(ws2812_rmt::GetDriverVersionMinor() == EXPECTED_MINOR,
              "minor version differs from CMake");
static_assert(ws2812_rmt::GetDriverVersionPatch() == EXPECTED_PATCH,
              "patch version differs from CMake");

int main() {
  if (std::strcmp(ws2812_rmt::GetDriverVersion(), EXPECTED_STRING) != 0) {
    static_cast<void>(std::printf("version string %s, expected %s\n",
                                  ws2812_rmt::GetDriverVersion(), EXPECTED_STRING));
    return 1;
  }
  static_cast<void>(std::printf("version %s: PASS\n", ws2812_rmt::GetDriverVersion()));
  return 0;
}
