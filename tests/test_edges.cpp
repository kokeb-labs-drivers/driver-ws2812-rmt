/**
 * @file test_edges.cpp
 * @brief The boundaries of the arithmetic a one-edit change would move unseen: the reset wait at a
 * tick rate that is not a whole number of megahertz, the rounded-up tick time, the longer of the
 * two bit codes in the frame time, and the exclusive cap of an unbounded low window.
 */
#include <array>
#include <cstdint>
#include <span>

#include "mock_ws2812_rmt.hpp"
#include "test_harness.hpp"
#include "ws2812_rmt.hpp"

using namespace ws2812_rmt;
using ws2812_rmt_test::FullPort;

namespace {

/// A part with one-tick and two-tick bit phases at about 3 MHz and a reset of 280.5 us.
Config SlowCustom() noexcept {
  Config c = Config::Default();
  c.variant = Variant::Custom;
  c.use_default_order = false;
  c.order = kOrderGrb;
  c.custom = MakeTiming(MakeRange(200U, 500U, 330U), MakeRange(500U, 1000U, 660U),
                        MakeRange(500U, 1000U, 660U), MakeRange(200U, 500U, 330U),
                        MakeLimits(0U, 0U, 280'500U, 0U));
  c.reset_us = 301U;
  return c;
}

} // namespace

TEST_CASE("After a failed send the wait is longer than the reset, also when the reset is not a "
          "whole number of microseconds") {
  FullPort port;
  port.resolution_hz = 3'100'000; // 322.6 ns per tick
  Ws2812Rmt<FullPort, 2> strip(port, SlowCustom());
  REQUIRE(ws2812_rmt_test::Start(strip));
  const auto wave = strip.CurrentWaveform();
  REQUIRE(wave.ok());
  // 301 us is 933.1 ticks: 933 ticks, 300.97 us, above the 280.5 us minimum.
  CHECK_EQ(wave.value.reset_ticks, 933U);
  port.fail_transmit_at = 1;
  CHECK_EQ(strip.Show().error, Error::Bus);
  const uint64_t before = port.delayed_us;
  REQUIRE(strip.Show().ok());
  const uint64_t waited = port.delayed_us - before;
  // Strictly longer than the reset: waited x hz > reset_ticks x 1e6. A wait of 300 us (the reset
  // rounded down) would leave the line low for less than the reset the pixels need.
  CHECK_EQ(waited, 301U);
  CHECK((waited * port.resolution_hz) >
        (static_cast<uint64_t>(wave.value.reset_ticks) * 1'000'000U));
}

TEST_CASE("At the V5 profile the wait after a failed send is one microsecond above the reset") {
  FullPort port;
  Ws2812Rmt<FullPort, 2> strip(port);
  REQUIRE(ws2812_rmt_test::Start(strip));
  const auto wave = strip.CurrentWaveform();
  REQUIRE(wave.ok());
  port.fail_transmit_at = 1;
  CHECK_EQ(strip.Show().error, Error::Bus);
  const uint64_t before = port.delayed_us;
  REQUIRE(strip.Show().ok());
  const uint64_t waited = port.delayed_us - before;
  CHECK_EQ(waited, 301U); // 6000 ticks at 20 MHz are 300 us
  CHECK((waited * port.resolution_hz) >
        (static_cast<uint64_t>(wave.value.reset_ticks) * 1'000'000U));
}

TEST_CASE("TicksToNs rounds up when a tick is not a whole number of nanoseconds") {
  CHECK_EQ(TicksToNs(1U, 3'000'000U), 334U); // 333.3 ns
  CHECK_EQ(TicksToNs(1U, 7'000'000U), 143U); // 142.9 ns
  CHECK_EQ(TicksToNs(3U, 3'000'000U), 1000U);
  CHECK_EQ(TicksToNs(7U, 7'000'000U), 1000U);
}

TEST_CASE("The frame time counts the longer bit code: on the WS2812C the 0 code is longer") {
  const auto wave = ResolveWaveform(ProfileTiming(Variant::Ws2812c), 20'000'000U, 0U);
  REQUIRE(wave.ok());
  const uint64_t zero = static_cast<uint64_t>(wave.value.t0h_ticks) + wave.value.t0l_ticks;
  const uint64_t one = static_cast<uint64_t>(wave.value.t1h_ticks) + wave.value.t1l_ticks;
  REQUIRE(zero > one); // 24 against 21 ticks
  // One pixel: 24 bits of the 0 code and the reset, 576 + 6000 ticks.
  CHECK_EQ(FrameTimeNs(1U, uint8_t{3}, wave.value), 328'800U);
  FullPort port;
  Config c = Config::Default();
  c.variant = Variant::Ws2812c;
  c.pixel_count = 1U;
  Ws2812Rmt<FullPort, 1> strip(port, c);
  REQUIRE(strip.EnsureInitialized().ok());
  CHECK_EQ(strip.FrameTimeUs().value, 329U);
}

TEST_CASE("A low window without a maximum is open at the in-frame cap: one tick below it is in, "
          "the cap itself is out") {
  const auto resolved = ResolveWaveform(ProfileTiming(Variant::Sk6812Mini), 20'000'000U, 0U);
  REQUIRE(resolved.ok());
  Waveform w = resolved.value;
  CHECK(WaveformInWindow(Variant::Sk6812Mini, w));
  // The SK6812MINI 0 code low has no maximum; the in-frame limit is 20 us, 400 ticks at 20 MHz.
  w.t0l_ticks = 399U;
  CHECK(WaveformInWindow(Variant::Sk6812Mini, w));
  w.t0l_ticks = 400U;
  CHECK(!WaveformInWindow(Variant::Sk6812Mini, w));
  w = resolved.value;
  w.t1l_ticks = 400U;
  CHECK(!WaveformInWindow(Variant::Sk6812Mini, w));
}

TEST_CASE(
    "After a failed PowerOff() the documented recovery sends a blank frame and switches off") {
  FullPort port;
  Ws2812Rmt<FullPort, 2> strip(port);
  REQUIRE(ws2812_rmt_test::Start(strip));
  strip.Fill(Colour{10, 20, 30, 0});
  REQUIRE(strip.Show().ok());
  port.power_ok = false;
  CHECK_EQ(strip.PowerOff().error, Error::Bus);
  strip.Clear();
  CHECK_EQ(strip.Show().error, Error::NotPowered); // in doubt: DIN is not driven
  port.power_ok = true;
  // Recovery (docs/contract.md): PowerOn(), Clear(), Show(), PowerOff().
  REQUIRE(strip.PowerOn().ok());
  REQUIRE(strip.Show().ok());
  CHECK_EQ(port.last_wire.size(), 6U);
  for (const uint8_t b : port.last_wire) {
    CHECK_EQ(b, 0U);
  }
  CHECK(strip.PowerOff().ok());
  CHECK(!port.powered);
}

TEST_CASE("TraceWire() takes exactly four bytes per pixel of capacity, and refuses one byte more") {
  ws2812_rmt_test::MinimalPort port;
  Ws2812Rmt<ws2812_rmt_test::MinimalPort, 2> strip(port);
  REQUIRE(ws2812_rmt_test::Start(strip));
  const std::array<uint8_t, 9> raw = {1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U};
  CHECK(strip.TraceWire(std::span<const uint8_t>(raw).first(8U)).ok());
  CHECK_EQ(port.last_wire.size(), 8U);
  CHECK_EQ(strip.TraceWire(raw).error, Error::InvalidArgument);
  CHECK_EQ(port.transmit_calls, 1);
}
