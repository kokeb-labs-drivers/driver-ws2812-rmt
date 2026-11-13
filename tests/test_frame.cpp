/**
 * @file test_frame.cpp
 * @brief Frame time, refresh rate, cascade length and the logic-level truth
 * table (datasheet rows 9, 14, 15 and 16 of the timing matrix).
 */
#include <cstdint>

#include "mock_ws2812_rmt.hpp"
#include "test_harness.hpp"
#include "ws2812_rmt.hpp"

using namespace ws2812_rmt;
using ws2812_rmt_test::FullPort;

namespace {
Waveform V5At20Mhz() {
  const auto w = ResolveWaveform(ProfileTiming(Variant::Ws2812bV5), 20'000'000, 0);
  return w.value;
}
} // namespace

TEST_CASE("60 pixels GRB at 20 MHz, V5: 1440 bits x 1300 ns plus a 300 us "
          "reset is 2.172 ms") {
  const Waveform w = V5At20Mhz();
  CHECK_EQ(FrameBits(60, 3), 1440U);
  CHECK_EQ(FrameTimeNs(60, 3, w), 2'172'000U);
  CHECK_EQ(MaxRefreshHz(60, 3, w), 460U);
  CHECK_EQ(FrameTimeNs(0, 3, w), 300'000U);
  CHECK_EQ(FrameTimeNs(1, 4, w), 300'000U + (32U * 1300U));
}

TEST_CASE("A cascade of at least 1024 pixels refreshes at 30 frames per second") {
  const Waveform w = V5At20Mhz();
  CHECK(MaxPixelsAtRate(30, 3, w) >= 1024U);
  CHECK_EQ(MaxPixelsAtRate(30, 3, w), 1058U);
  CHECK(FrameTimeNs(MaxPixelsAtRate(30, 3, w), 3, w) <= 33'333'333U);
  CHECK(FrameTimeNs(MaxPixelsAtRate(30, 3, w) + 1U, 3, w) > 33'333'333U);
  CHECK_EQ(MaxPixelsAtRate(0, 3, w), 0U);
  CHECK_EQ(MaxPixelsAtRate(10'000, 3, w),
           0U); // 100 us frames cannot hold the 300 us reset
  CHECK_EQ(MaxRefreshHz(0, 3, Waveform{}), 0U);
  CHECK_EQ(MaxPixelsAtRate(30, 3, Waveform{}), 0U);
  CHECK_EQ(TicksToNs(5, 0), 0U);
}

TEST_CASE("The port timeout is the frame time in milliseconds, rounded up, "
          "plus the slack") {
  FullPort port;
  Ws2812Rmt<FullPort, 60> strip(port);
  REQUIRE(ws2812_rmt_test::Start(strip));
  CHECK_EQ(strip.Length(), 60U);
  CHECK_EQ(strip.ChannelsPerPixel(), 3U);
  const auto us = strip.FrameTimeUs();
  REQUIRE(us.ok());
  CHECK_EQ(us.value, 2172U);
  REQUIRE(strip.Show().ok());
  CHECK_EQ(port.last_timeout_ms, 3U + 20U);
  Config c = Config::Default();
  c.transmit_slack_ms = 0;
  FullPort port2;
  Ws2812Rmt<FullPort, 60> strip2(port2, c);
  REQUIRE(ws2812_rmt_test::Start(strip2));
  REQUIRE(strip2.Show().ok());
  CHECK_EQ(port2.last_timeout_ms, 3U);
}

TEST_CASE("MinLogicHighMv truth table: the V5 profile takes the strictest of its documents "
          "(0.63 x VDD, 2.7 V floor); older parts and SK6812 need more than 3.3 V") {
  CHECK_EQ(MinLogicHighMv(Variant::Ws2812bV5, 5000), 3150U); // V6.0 and V6.1: 0.63 x VDD
  CHECK_EQ(MinLogicHighMv(Variant::Ws2812bV5, 3300), 2700U); // the V5.0 floor
  CHECK_EQ(MinLogicHighMv(Variant::Ws2812bV5, 3700), 2700U);
  CHECK_EQ(MinLogicHighMv(Variant::Ws2812bV5, 4300), 2709U);
  // A 3.3 V output meets it at 5.0 V, but not at the 5.3 V absolute maximum.
  CHECK(MinLogicHighMv(Variant::Ws2812bV5, 5000) < 3300U);
  CHECK(MinLogicHighMv(Variant::Ws2812bV5, 5300) > 3300U);
  CHECK_EQ(MinLogicHighMv(Variant::Ws2812bV5, 5300), 3339U);
  // An absurd supply is clamped, not wrapped.
  CHECK_EQ(MinLogicHighMv(Variant::Ws2812bV5, 0xFFFF'FFFFU), 3780U);
  CHECK_EQ(MinLogicHighMv(Variant::Ws2812, 0xFFFF'FFFFU), 4200U);
  CHECK_EQ(MinLogicHighMv(Variant::Ws2812bPreV5, 5000), 3500U);
  CHECK_EQ(MinLogicHighMv(Variant::Ws2812, 5000), 3500U);
  CHECK_EQ(MinLogicHighMv(Variant::Ws2812c, 5000), 3500U);
  CHECK_EQ(MinLogicHighMv(Variant::Ws2812d, 5000), 3500U);
  CHECK_EQ(MinLogicHighMv(Variant::Sk6812, 5000), 3500U);
  CHECK_EQ(MinLogicHighMv(Variant::Sk6812, 4500), 3400U);
  CHECK_EQ(MinLogicHighMv(Variant::Sk6812Rgbw, 5000), 3500U);
  CHECK_EQ(MinLogicHighMv(Variant::Sk6812Mini, 5000), 3500U);
  CHECK_EQ(MinLogicHighMv(Variant::Custom, 5000), 3500U);
  CHECK_EQ(MinLogicHighMv(ws2812_rmt_test::RawEnum<Variant>(50), 5000), 3500U);
  for (const Variant v :
       {Variant::Ws2812bPreV5, Variant::Ws2812, Variant::Sk6812, Variant::Sk6812Rgbw}) {
    CHECK(MinLogicHighMv(v, 5000) > 3300U); // a 3.3 V output needs a level shifter at 5 V
  }
}
