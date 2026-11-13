/**
 * @file test_model.cpp
 * @brief The model's constants against the driver's, checked at build time, so
 * a wrong constant in the driver cannot hide behind the same constant in the
 * model (handbook KB-TST-030).
 */
#include <string_view>

#include "mock_ws2812_rmt.hpp"
#include "test_harness.hpp"
#include "ws2812_rmt_functions.hpp"

using namespace ws2812_rmt;

// The RMT symbol limit the model splits a long reset at: 15 bits (datasheet of
// the RMT peripheral; the port header repeats it).
static_assert(kRmtHalfSymbolMaxTicks == 32'767U);
// 24 bit frames for the RGB parts, 32 bit for the RGBW part (datasheet
// "composition of 24bit data" and "data structure of 32bit").
static_assert((kOrderGrb.count == 3U) && (kOrderGrbw.count == 4U) && (kOrderRgbw.count == 4U));
static_assert((FrameBits(1, 3) == 24U) && (FrameBits(1, 4) == 32U));
// The model's reset minimum of the strictest WS2812B document is the driver's
// profile value.
static_assert(ProfileTiming(Variant::Ws2812bPreV5).reset_min_ns == 300'000U);
static_assert(ProfileTiming(Variant::Ws2812bV5).reset_min_ns == 280'000U);
static_assert(ProfileTiming(Variant::Sk6812).reset_min_ns == 80'000U);
static_assert(ws2812_rmt_test::kDocuments.size() == 22U);
// Cascade of at least 1024 pixels at 30 frames per second.
static_assert(MaxPixelsAtRate(30, 3, Waveform{20'000'000, 6, 19, 14, 12, 6000, 5599}) >= 1024U);

TEST_CASE("The model's window rows are well formed") {
  for (const auto& d : ws2812_rmt_test::kDocuments) {
    for (const auto& w : {d.t0h, d.t0l, d.t1h, d.t1l}) {
      CHECK(w.min_ns > 0U);
      CHECK((w.max_ns == 0U) || (w.max_ns >= w.min_ns));
    }
    CHECK(d.reset_min_ns >= 50'000U);
    CHECK(std::string_view(d.order).size() >= 3U);
  }
}
