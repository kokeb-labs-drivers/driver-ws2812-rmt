/**
 * @file test_bus_faults.cpp
 * @brief Every failing hook gives `Bus` and leaves the pixel buffer intact; the
 * driver stays usable; an RMT underrun is visible in the model as a gap and as
 * stale pixels (docs/contract.md).
 */
#include <array>
#include <cstdint>
#include <memory>

#include "mock_ws2812_rmt.hpp"
#include "test_harness.hpp"
#include "ws2812_rmt.hpp"

using namespace ws2812_rmt;
using ws2812_rmt_test::FullPort;
using ws2812_rmt_test::kDocuments;
using ws2812_rmt_test::MinimalPort;
using ws2812_rmt_test::PixelChainModel;

TEST_CASE("A failing EnsureInitialized() or ResolutionHz() gives Bus and the "
          "next call starts again") {
  FullPort port;
  port.fail_init = true;
  Ws2812Rmt<FullPort, 8> strip(port);
  CHECK_EQ(strip.EnsureInitialized().error, Error::Bus);
  CHECK(!strip.IsInitialized());
  CHECK_EQ(port.resolution_calls, 0);
  port.fail_init = false;
  port.fail_resolution = true;
  CHECK_EQ(strip.EnsureInitialized().error, Error::Bus);
  CHECK_EQ(port.init_calls, 2);
  port.fail_resolution = false;
  CHECK(strip.EnsureInitialized().ok());
  CHECK(strip.IsInitialized());
  CHECK(port.log_lines >= 2);
}

TEST_CASE("A failed Transmit() leaves the pixels intact; the next Show() "
          "latches the same colours") {
  FullPort port;
  auto strip = std::make_unique<Ws2812Rmt<FullPort, 6>>(port);
  REQUIRE(ws2812_rmt_test::Start(*strip));
  for (size_t i = 0; i < 6; ++i) {
    REQUIRE(strip
                ->SetPixel(i, Colour{static_cast<uint8_t>(i * 40U),
                                     static_cast<uint8_t>(255U - (i * 40U)), 7, 0})
                .ok());
  }
  port.fail_transmit_at = 1;
  CHECK_EQ(strip->Show().error, Error::Bus);
  CHECK(port.symbols.empty());
  CHECK_EQ(strip->GetPixel(2).value.r, 80U);
  CHECK(strip->IsInitialized());
  REQUIRE(strip->Show().ok()); // the 2nd call is not the failing one
  PixelChainModel model(kDocuments[5], 6);
  model.Consume(port.symbols, port.EffectiveHz());
  CHECK_EQ(model.latches, 1);
  CHECK_EQ(model.violations + model.gaps_too_long, 0);
  CHECK_EQ(model.Latched()[2].r, 80U);
  CHECK_EQ(model.Latched()[2].g, 175U);
  port.timeout_on_transmit = true;
  CHECK_EQ(strip->Show().error,
           Error::Bus); // a timeout is a Bus failure; the port reports false
  port.timeout_on_transmit = false;
  CHECK(strip->Show().ok());
}

TEST_CASE("A failing power pin gives Bus, in both directions") {
  FullPort port;
  Ws2812Rmt<FullPort, 2> strip(port);
  REQUIRE(ws2812_rmt_test::Start(strip));
  const uint64_t settled = port.delayed_us; // the settle of the first PowerOn()
  port.power_ok = false;
  CHECK_EQ(strip.PowerOn().error, Error::Bus);
  CHECK_EQ(strip.PowerOff().error, Error::Bus);
  CHECK_EQ(port.delayed_us, settled); // no settle after a failed switch-on
  port.power_ok = true;
  CHECK(strip.PowerOn().ok());
  CHECK(port.log_lines >= 2);
}

TEST_CASE("max_gap_ticks is what the port is given: below the smallest latch time of the "
          "variant's documents, less 10 percent") {
  FullPort v5;
  Ws2812Rmt<FullPort, 2> a(v5);
  REQUIRE(ws2812_rmt_test::Start(a));
  REQUIRE(a.Show().ok());
  CHECK_EQ(v5.last_wave.max_gap_ticks, 5039U); // 280 us less 10 percent is 252 us: 5040 ticks
  FullPort sk;
  Config c = Config::Default();
  c.variant = Variant::Sk6812Mini;
  Ws2812Rmt<FullPort, 2> b(sk, c);
  REQUIRE(ws2812_rmt_test::Start(b));
  REQUIRE(b.Show().ok());
  CHECK_EQ(sk.last_wave.max_gap_ticks, 359U); // 20 us less 10 percent is 18 us: 360 ticks
}

TEST_CASE("An underrun inside a frame is a gap in the model; one above the "
          "reset time cuts the frame") {
  const auto& doc = kDocuments[5]; // b-v5
  {
    FullPort port;
    port.underrun_after_byte = 4;
    port.underrun_gap_ticks = 400; // 20 us: far above the 1 us window, far below the reset
    auto strip = std::make_unique<Ws2812Rmt<FullPort, 4>>(port);
    REQUIRE(ws2812_rmt_test::Start(*strip));
    strip->Fill(Colour{10, 20, 30, 0});
    REQUIRE(strip->Show().ok());
    PixelChainModel model(doc, 4);
    model.Consume(port.symbols, port.EffectiveHz());
    CHECK_EQ(model.gaps_too_long, 1);
    CHECK_EQ(model.latches, 1);
    CHECK(model.Latched()[3] ==
          (Colour{10, 20, 30, 0})); // the pixels still arrive: below the reset time
    CHECK(400U < port.last_wave.reset_ticks);
  }
  {
    FullPort port;
    port.underrun_after_byte = 5;   // after the second pixel
    port.underrun_gap_ticks = 6000; // 300 us: above the reset minimum
    auto strip = std::make_unique<Ws2812Rmt<FullPort, 4>>(port);
    REQUIRE(ws2812_rmt_test::Start(*strip));
    strip->Fill(Colour{10, 20, 30, 0});
    REQUIRE(strip->Show().ok());
    PixelChainModel model(doc, 4);
    model.Consume(port.symbols, port.EffectiveHz());
    CHECK_EQ(model.latches,
             2); // the frame was cut in two: the first two pixels latched early
    CHECK(model.Latched()[0] == (Colour{10, 20, 30, 0}));
    CHECK(model.Latched()[3] == (Colour{})); // the last pixels stayed stale
  }
}

TEST_CASE("A port without optional hooks compiles and runs; Log is optional") {
  ws2812_rmt_test::MinimalPort port;
  Ws2812Rmt<ws2812_rmt_test::MinimalPort, 3> strip(port);
  CHECK_EQ(strip.EnsureInitialized().error, Error::Ok);
  port.fail_transmit_at = 1;
  CHECK_EQ(strip.Show().error, Error::Bus);
}

TEST_CASE("A frame cut mid-way is latched by a reset before the next frame; without it the "
          "next frame would be shifted") {
  const auto& doc = kDocuments[5]; // b-v5
  FullPort port;
  port.cut_transmit_at = 1;
  port.cut_after_bits = 37; // a pixel and a half: the pixels hold half a frame
  auto strip = std::make_unique<Ws2812Rmt<FullPort, 4>>(port);
  REQUIRE(ws2812_rmt_test::Start(*strip));
  strip->Fill(Colour{10, 20, 30, 0});
  const uint64_t settled = port.delayed_us; // the settle of PowerOn()
  CHECK_EQ(strip->Show().error, Error::Bus);
  CHECK_EQ(port.delayed_us, settled); // nothing to wait for yet
  REQUIRE(strip->Show().ok());
  // The driver waited strictly longer than the waveform's reset with the line low first.
  const auto wave = strip->CurrentWaveform();
  REQUIRE(wave.ok());
  CHECK(((port.delayed_us - settled) * port.resolution_hz) >
        (static_cast<uint64_t>(wave.value.reset_ticks) * 1'000'000U));
  PixelChainModel model(doc, 4);
  model.Consume(port.symbols, port.EffectiveHz());
  CHECK_EQ(model.violations, 0);
  CHECK_EQ(model.latches, 2);           // the cut frame, then the good one
  CHECK_EQ(model.wrong_size_frames, 1); // only the cut one
  for (const Colour& c : model.Latched()) {
    CHECK(c == (Colour{10, 20, 30, 0})); // the good frame is not shifted
  }
  // A healthy frame afterwards needs no wait.
  const uint64_t waited = port.delayed_us;
  REQUIRE(strip->Show().ok());
  CHECK_EQ(port.delayed_us, waited);
}

TEST_CASE("The reset after a failed send is also inserted before TraceWire(), and once per "
          "failure") {
  const auto& doc = kDocuments[5];
  MinimalPort port;
  port.cut_transmit_at = 1;
  port.cut_after_bits = 5;
  Ws2812Rmt<MinimalPort, 2> strip(port);
  REQUIRE(ws2812_rmt_test::Start(strip));
  const std::array<uint8_t, 6> raw = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
  CHECK_EQ(strip.TraceWire(raw).error, Error::Bus);
  port.fail_transmit_at = 3; // the third send fails too, nothing emitted
  REQUIRE(strip.TraceWire(raw).ok());
  CHECK_EQ(strip.TraceWire(raw).error, Error::Bus);
  const uint64_t after_second = port.delayed_us;
  REQUIRE(strip.TraceWire(raw).ok());
  CHECK(port.delayed_us > after_second); // a reset again after the second failure
  PixelChainModel model(doc, 2);
  model.Consume(port.symbols, port.EffectiveHz());
  CHECK_EQ(model.wrong_size_frames, 1);                       // only the cut one reaches the pixels
  CHECK(model.Latched()[0] == (Colour{0x22, 0x11, 0x33, 0})); // G R B on the wire, as sent
}

TEST_CASE("A failed PowerOff() still refuses to send: the supply state is in doubt") {
  FullPort port;
  Ws2812Rmt<FullPort, 2> strip(port);
  REQUIRE(ws2812_rmt_test::Start(strip));
  REQUIRE(strip.PowerOn().ok());
  port.power_ok = false;
  CHECK_EQ(strip.PowerOff().error, Error::Bus);
  const int32_t sent = port.transmit_calls;
  CHECK_EQ(strip.Show().error, Error::NotPowered);
  CHECK_EQ(port.transmit_calls, sent);
  port.power_ok = true;
  REQUIRE(strip.PowerOn().ok());
  CHECK(strip.Show().ok());
}
