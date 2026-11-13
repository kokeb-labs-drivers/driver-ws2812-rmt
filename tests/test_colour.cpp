/**
 * @file test_colour.cpp
 * @brief Brightness scaling, the gamma table, white extraction and the colour
 * wheel.
 */
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>

#include "mock_ws2812_rmt.hpp"
#include "test_harness.hpp"
#include "ws2812_rmt_functions.hpp"

using namespace ws2812_rmt;

TEST_CASE("Scale8 over all 65536 pairs: the rounded quotient, identity at 255, "
          "zero at 0") {
  for (uint32_t v = 0; v < 256U; ++v) {
    for (uint32_t b = 0; b < 256U; ++b) {
      // round(v * b / 255), halves up, written as a different integer
      // expression than the driver's
      const uint32_t expected = ((2U * v * b) + 255U) / 510U;
      CHECK_EQ(Scale8(static_cast<uint8_t>(v), static_cast<uint8_t>(b)), expected);
    }
    CHECK_EQ(Scale8(static_cast<uint8_t>(v), 255), v);
    CHECK_EQ(Scale8(static_cast<uint8_t>(v), 0), 0U);
    CHECK_EQ(Scale8(255, static_cast<uint8_t>(v)), v);
  }
}

TEST_CASE("Scale8 is monotone in both arguments and within half a step of the "
          "exact value") {
  for (uint32_t a = 0; a < 255U; ++a) {
    for (uint32_t b = 0; b < 256U; ++b) {
      CHECK(Scale8(static_cast<uint8_t>(a + 1U), static_cast<uint8_t>(b)) >=
            Scale8(static_cast<uint8_t>(a), static_cast<uint8_t>(b)));
      CHECK(Scale8(static_cast<uint8_t>(b), static_cast<uint8_t>(a + 1U)) >=
            Scale8(static_cast<uint8_t>(b), static_cast<uint8_t>(a)));
    }
  }
  for (uint32_t v = 0; v < 256U; ++v) {
    for (uint32_t b = 0; b < 256U; ++b) {
      const long exact_x510 = static_cast<long>(2U) * static_cast<long>(v * b);
      const long got_x510 =
          static_cast<long>(Scale8(static_cast<uint8_t>(v), static_cast<uint8_t>(b))) * 510L;
      CHECK(std::labs(got_x510 - exact_x510) <= 255L);
    }
  }
}

TEST_CASE("The gamma table equals std::pow, is monotone, and has its endpoints") {
  for (int32_t v = 0; v < 256; ++v) {
    const double exact = 255.0 * std::pow(static_cast<double>(v) / 255.0, 2.2);
    CHECK_EQ(static_cast<int32_t>(kGamma22[static_cast<size_t>(v)]),
             static_cast<int32_t>(std::floor(exact + 0.5)));
    if (v > 0) {
      CHECK(kGamma22[static_cast<size_t>(v)] >= kGamma22[static_cast<size_t>(v - 1)]);
    }
  }
  CHECK_EQ(kGamma22[0], 0U);
  CHECK_EQ(kGamma22[255], 255U);
  CHECK_EQ(ApplyGamma(100, Gamma::None), 100U);
  CHECK_EQ(ApplyGamma(100, Gamma::Gamma22), kGamma22[100]);
  CHECK_EQ(ApplyGamma(100, ws2812_rmt_test::RawEnum<Gamma>(9)), 100U);
}

TEST_CASE("Gamma is applied before brightness") {
  for (uint32_t v = 0; v < 256U; v += 5U) {
    for (uint32_t b = 0; b < 256U; b += 7U) {
      const uint8_t expected = Scale8(kGamma22[v], static_cast<uint8_t>(b));
      CHECK_EQ(WireValue(static_cast<uint8_t>(v), static_cast<uint8_t>(b), Gamma::Gamma22),
               expected);
    }
  }
  CHECK(WireValue(128, 128, Gamma::Gamma22) != kGamma22[Scale8(128, 128)]);
}

TEST_CASE("ExtractWhite moves the common part into white") {
  for (uint32_t r = 0; r < 256U; r += 15U) {
    for (uint32_t g = 0; g < 256U; g += 17U) {
      for (uint32_t b = 0; b < 256U; b += 19U) {
        const Colour in{static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b),
                        99};
        const Colour out = ExtractWhite(in);
        CHECK_EQ(static_cast<uint32_t>(out.r) + out.w, r);
        CHECK_EQ(static_cast<uint32_t>(out.g) + out.w, g);
        CHECK_EQ(static_cast<uint32_t>(out.b) + out.w, b);
        CHECK((out.r == 0U) || (out.g == 0U) || (out.b == 0U));
      }
    }
  }
  CHECK(ExtractWhite(Colour{200, 100, 50, 0}) == (Colour{150, 50, 0, 50}));
}

TEST_CASE("Wheel: the three segment endpoints and continuity") {
  CHECK(Wheel(0) == (Colour{255, 0, 0, 0}));
  CHECK(Wheel(85) == (Colour{0, 255, 0, 0}));
  CHECK(Wheel(170) == (Colour{0, 0, 255, 0}));
  CHECK(Wheel(255) == (Colour{255, 0, 0, 0}));
  for (uint32_t p = 0; p < 255U; ++p) {
    const Colour a = Wheel(static_cast<uint8_t>(p));
    const Colour b = Wheel(static_cast<uint8_t>(p + 1U));
    CHECK(std::abs(static_cast<int32_t>(a.r) - static_cast<int32_t>(b.r)) <= 3);
    CHECK(std::abs(static_cast<int32_t>(a.g) - static_cast<int32_t>(b.g)) <= 3);
    CHECK(std::abs(static_cast<int32_t>(a.b) - static_cast<int32_t>(b.b)) <= 3);
    CHECK_EQ(a.w, 0U);
  }
}
