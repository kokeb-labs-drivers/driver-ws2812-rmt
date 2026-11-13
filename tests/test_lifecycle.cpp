/**
 * @file test_lifecycle.cpp
 * @brief Calls before start-up, the start-up sequence, `Validate()` (every
 * rejection), the pixel accessors and the `Custom` profile.
 */
#include <array>
#include <cstdint>
#include <memory>
#include <string_view>

#include "mock_ws2812_rmt.hpp"
#include "test_harness.hpp"
#include "ws2812_rmt.hpp"

using namespace ws2812_rmt;
using ws2812_rmt_test::FullPort;
using ws2812_rmt_test::MinimalPort;

static_assert(Validate(Config::Default(), 60).ok(), "the default configuration is valid");
static_assert(!Validate(Config::Default(), 0).ok(), "zero pixels are refused");
static_assert(Validate(Config::Default(), kMaxPixels).ok());
static_assert(!Validate(Config::Default(), kMaxPixels + 1U).ok());

namespace {

constexpr std::array<Variant, 8> kNamed = {
    Variant::Ws2812,  Variant::Ws2812bPreV5, Variant::Ws2812bV5,  Variant::Ws2812c,
    Variant::Ws2812d, Variant::Sk6812,       Variant::Sk6812Rgbw, Variant::Sk6812Mini};

bool Rejects(const Config& c, size_t capacity = 60) {
  const auto r = Validate(c, capacity);
  return !r.ok() && (r.error == Error::InvalidArgument);
}

Config CustomLike(Variant v) {
  Config c = Config::Default();
  c.variant = Variant::Custom;
  c.custom = ProfileTiming(v);
  c.use_default_order = false;
  c.order = DefaultOrder(v);
  return c;
}

} // namespace

TEST_CASE("Construction does no I/O; calls before EnsureInitialized() return "
          "NotInitialized") {
  FullPort port;
  Ws2812Rmt<FullPort, 8> strip(port);
  CHECK_EQ(port.init_calls, 0);
  CHECK(!strip.IsInitialized());
  CHECK_EQ(strip.Show().error, Error::NotInitialized);
  CHECK_EQ(strip.CurrentWaveform().error, Error::NotInitialized);
  CHECK_EQ(strip.FrameTimeUs().error, Error::NotInitialized);
  CHECK_EQ(strip.PowerOn().error, Error::NotInitialized);
  CHECK_EQ(strip.PowerOff().error, Error::NotInitialized);
  CHECK_EQ(port.transmit_calls, 0);
  CHECK_EQ(port.power_calls, 0);
  CHECK_EQ(port.init_calls + port.resolution_calls, 0);
}

TEST_CASE("The setters work before start-up and only store") {
  FullPort port;
  Ws2812Rmt<FullPort, 8> strip(port);
  REQUIRE(strip.SetPixel(3, Colour{1, 2, 3, 4}).ok());
  const auto p = strip.GetPixel(3);
  REQUIRE(p.ok());
  CHECK(p.value == (Colour{1, 2, 3, 4}));
  strip.Fill(Colour{9, 9, 9, 9});
  CHECK(strip.GetPixel(7).value == (Colour{9, 9, 9, 9}));
  strip.Clear();
  CHECK(strip.GetPixel(0).value == (Colour{}));
  strip.SetBrightness(17);
  CHECK_EQ(strip.Brightness(), 17U);
  strip.SetGamma(ws2812_rmt_test::RawEnum<Gamma>(9)); // an unknown value acts as None
  strip.SetGamma(Gamma::Gamma22);
  CHECK_EQ(port.transmit_calls + port.init_calls, 0);
}

TEST_CASE("An index out of range is InvalidArgument and changes nothing") {
  FullPort port;
  Ws2812Rmt<FullPort, 8> strip(port);
  strip.Fill(Colour{5, 5, 5, 5});
  CHECK_EQ(strip.SetPixel(8, Colour{1, 1, 1, 1}).error, Error::InvalidArgument);
  CHECK_EQ(strip.SetPixel(1000, Colour{1, 1, 1, 1}).error, Error::InvalidArgument);
  CHECK_EQ(strip.GetPixel(8).error, Error::InvalidArgument);
  CHECK(strip.GetPixel(7).value == (Colour{5, 5, 5, 5}));
}

TEST_CASE("EnsureInitialized() starts the port once and resolves the waveform") {
  FullPort port;
  Ws2812Rmt<FullPort, 8> strip(port);
  REQUIRE(strip.EnsureInitialized().ok());
  REQUIRE(strip.EnsureInitialized().ok());
  REQUIRE(strip.PowerOn().ok()); // the port has a supply hook, so it starts unpowered
  CHECK(strip.IsInitialized());
  CHECK_EQ(port.init_calls, 1);
  CHECK_EQ(port.resolution_calls, 1);
  const auto w = strip.CurrentWaveform();
  REQUIRE(w.ok());
  CHECK(w.value == ResolveWaveform(ProfileTiming(Variant::Ws2812bV5), 20'000'000, 0).value);
  CHECK_EQ(port.transmit_calls, 0); // start-up sends nothing
  REQUIRE(strip.Show().ok());
  CHECK_EQ(port.transmit_calls, 1);
  CHECK_EQ(port.last_wire.size(), 8U * 3U);
  CHECK(port.last_wave == w.value);
}

TEST_CASE("A port rate the timing cannot meet gives TimingUnreachable and no frame") {
  FullPort port;
  port.resolution_hz = 5'000'000;
  Config c = Config::Default();
  c.variant = Variant::Ws2812bPreV5;
  Ws2812Rmt<FullPort, 8> strip(port, c);
  CHECK_EQ(strip.EnsureInitialized().error, Error::TimingUnreachable);
  CHECK(!strip.IsInitialized());
  CHECK_EQ(strip.Show().error, Error::NotInitialized);
  CHECK(port.log_lines > 0);
  for (const uint32_t hz : {0U, 1U, 4'294'967'295U}) {
    FullPort bad;
    bad.resolution_hz = hz;
    Ws2812Rmt<FullPort, 8> s(bad);
    CHECK_EQ(s.EnsureInitialized().error, Error::TimingUnreachable);
    CHECK_EQ(bad.transmit_calls, 0);
  }
}

TEST_CASE("A named variant below kMinResolutionHz is refused: at 10 MHz the V5 profile sits on a "
          "window edge and a clock error of 1000 ppm leaves it") {
  for (const Variant v :
       {Variant::Ws2812, Variant::Ws2812bPreV5, Variant::Ws2812bV5, Variant::Ws2812c,
        Variant::Ws2812d, Variant::Sk6812, Variant::Sk6812Rgbw, Variant::Sk6812Mini}) {
    FullPort low;
    low.resolution_hz = kMinResolutionHz - 1U;
    Config c = Config::Default();
    c.variant = v;
    c.use_default_order = true;
    Ws2812Rmt<FullPort, 4> strip(low, c);
    CHECK_EQ(strip.EnsureInitialized().error, Error::TimingUnreachable);
    CHECK_EQ(low.transmit_calls, 0);
    FullPort ok;
    ok.resolution_hz = kMinResolutionHz;
    Ws2812Rmt<FullPort, 4> strip2(ok, c);
    CHECK(strip2.EnsureInitialized().ok());
  }
  // A Custom timing is the caller's responsibility: no floor.
  FullPort slow;
  slow.resolution_hz = 10'000'000;
  Config c = Config::Default();
  c.variant = Variant::Custom;
  c.use_default_order = false;
  c.order = kOrderGrb;
  c.custom = ProfileTiming(Variant::Ws2812bV5);
  Ws2812Rmt<FullPort, 4> custom(slow, c);
  CHECK(custom.EnsureInitialized().ok());
}

TEST_CASE("An invalid configuration is refused before any I/O") {
  FullPort port;
  Config c = Config::Default();
  c.brightness = 255;
  c.reset_us = 100; // not above the 280 us minimum
  Ws2812Rmt<FullPort, 8> strip(port, c);
  CHECK_EQ(strip.EnsureInitialized().error, Error::InvalidArgument);
  CHECK_EQ(port.init_calls, 0);
  CHECK(!strip.IsInitialized());
}

TEST_CASE("Validate() rejects each bad field") {
  Config c = Config::Default();
  c.variant = ws2812_rmt_test::RawEnum<Variant>(9);
  CHECK(Rejects(c));
  c = Config::Default();
  c.gamma = ws2812_rmt_test::RawEnum<Gamma>(5);
  CHECK(Rejects(c));
  c = Config::Default();
  c.custom.reset_min_ns = 1; // a custom timing on a named variant
  CHECK(Rejects(c));
  c = Config::Default();
  c.use_default_order = false;
  c.order = kOrderGrbw; // four channels on a three-channel part
  CHECK(Rejects(c));
  c.order = kOrderRgb;
  CHECK(!Rejects(c));
  c = Config::Default();
  c.variant = Variant::Sk6812Rgbw;
  c.use_default_order = false;
  c.order = kOrderGrb; // three channels on the four-channel part
  CHECK(Rejects(c));
  c.order = kOrderRgbw;
  CHECK(!Rejects(c));
  c = Config::Default();
  c.use_default_order = false;
  c.order = ColourOrder{{Channel::G, Channel::G, Channel::B, Channel::W}, 3}; // repeated channel
  CHECK(Rejects(c));
  c.order = ColourOrder{{Channel::G, Channel::R, Channel::W, Channel::B},
                        3}; // white in a three-channel order
  CHECK(Rejects(c));
  c.order = ColourOrder{{Channel::G, Channel::R, Channel::B, Channel::W}, 5};
  CHECK(Rejects(c));
  c = Config::Default();
  c.reset_us = 280; // equal to the minimum: must be above
  CHECK(Rejects(c));
  c.reset_us = 281;
  CHECK(!Rejects(c));
  c.reset_us = kMaxResetUs + 1U;
  CHECK(Rejects(c));
  c.reset_us = kMaxResetUs;
  CHECK(!Rejects(c));
  c = Config::Default();
  c.pixel_count = 61;
  CHECK(Rejects(c, 60));
  c.pixel_count = 60;
  CHECK(!Rejects(c, 60));
  CHECK(Rejects(Config::Default(), 0));
  CHECK(Rejects(Config::Default(), kMaxPixels + 1U));
  c = Config::Default();
  c.power_on_settle_us = kMaxSettleUs + 1U;
  CHECK(Rejects(c));
  c = Config::Default();
  c.transmit_slack_ms = kMaxSlackMs + 1U;
  CHECK(Rejects(c));
}

TEST_CASE("Validate() rejects each inconsistent Custom timing and accepts a "
          "consistent one") {
  Config good = CustomLike(Variant::Ws2812bV5);
  CHECK(!Rejects(good));
  Config c = good;
  c.use_default_order = true; // a Custom part states its own order
  CHECK(Rejects(c));
  c = good;
  c.order.count = 3;
  c.order.ch = kOrderRgb.ch;
  CHECK(!Rejects(c));
  c = good;
  c.custom.t0h.min_ns = 0;
  CHECK(Rejects(c));
  c = good;
  c.custom.t0h.nominal_ns = c.custom.t0h.min_ns - 1U; // min above nominal
  CHECK(Rejects(c));
  c = good;
  c.custom.t1h.nominal_ns = c.custom.t1h.max_ns + 1U; // nominal above max
  CHECK(Rejects(c));
  c = good;
  c.custom.t1l.max_ns = c.custom.t1l.min_ns - 1U; // max below min
  CHECK(Rejects(c));
  c = good;
  c.custom.t0l.max_ns = kMaxDurationNs + 1U;
  CHECK(Rejects(c));
  c = good;
  c.custom.t0l.nominal_ns = kMaxDurationNs + 1U;
  c.custom.t0l.max_ns = 0;
  CHECK(Rejects(c));
  c = good;
  c.custom.period_min_ns = kMaxPeriodNs + 1U;
  CHECK(Rejects(c));
  c = good;
  c.custom.period_max_ns = 1000; // below period_min
  CHECK(Rejects(c));
  c = good;
  c.custom.period_max_ns =
      1260; // above period_min but below t0h.min + t0l.min? 220+580 = 800: fine
  CHECK(!Rejects(c));
  c.custom.period_max_ns = 1250;
  c.custom.period_min_ns = 1250;
  CHECK(!Rejects(c));
  c.custom.period_max_ns = 700; // below period_min (1250) and below the sum of the minima
  CHECK(Rejects(c));
  c = good;
  c.custom.period_min_ns = 0;
  c.custom.period_max_ns = 790; // the smallest bit (220 + 580) does not fit
  CHECK(Rejects(c));
  c.custom.period_max_ns = kMaxPeriodNs + 1U;
  CHECK(Rejects(c));
  c = good;
  c.custom.reset_min_ns = 0;
  CHECK(Rejects(c));
  c = good;
  c.custom.reset_min_ns = (kMaxResetUs * 1000U) + 1U;
  CHECK(Rejects(c));
  c = good;
  c.custom.max_low_ns = c.custom.reset_min_ns + 1U;
  CHECK(Rejects(c));
}

TEST_CASE("Custom with the numbers of a named profile gives that profile's "
          "waveform") {
  for (const Variant v : kNamed) {
    FullPort a;
    FullPort b;
    Config named = Config::Default();
    named.variant = v;
    Ws2812Rmt<FullPort, 4> sa(a, named);
    Ws2812Rmt<FullPort, 4> sb(b, CustomLike(v));
    REQUIRE(ws2812_rmt_test::Start(sa));
    REQUIRE(ws2812_rmt_test::Start(sb));
    CHECK(sa.CurrentWaveform().value == sb.CurrentWaveform().value);
    CHECK_EQ(sa.ChannelsPerPixel(), sb.ChannelsPerPixel());
  }
}

TEST_CASE("Length follows pixel_count, ChannelsPerPixel follows the variant "
          "and the order") {
  FullPort port;
  Config c = Config::Default();
  c.pixel_count = 5;
  Ws2812Rmt<FullPort, 8> strip(port, c);
  CHECK_EQ(strip.Length(), 5U);
  CHECK_EQ(strip.ChannelsPerPixel(), 3U);
  strip.Fill(Colour{1, 2, 3, 0});
  CHECK(strip.GetPixel(5).error == Error::InvalidArgument);
  REQUIRE(ws2812_rmt_test::Start(strip));
  REQUIRE(strip.Show().ok());
  CHECK_EQ(port.last_wire.size(), 15U);
  Config rgbw = Config::Default();
  rgbw.variant = Variant::Sk6812Rgbw;
  FullPort port2;
  Ws2812Rmt<FullPort, 8> strip2(port2, rgbw);
  CHECK_EQ(strip2.ChannelsPerPixel(), 4U);
  CHECK_EQ(strip2.GetConfig().variant, Variant::Sk6812Rgbw);
  Config over = Config::Default();
  over.pixel_count = 99; // above the capacity: refused at start-up, Length
                         // stays within the buffers
  Ws2812Rmt<FullPort, 8> strip3(port2, over);
  CHECK_EQ(strip3.Length(), 8U);
  CHECK_EQ(strip3.EnsureInitialized().error, Error::InvalidArgument);
}

TEST_CASE("Brightness and gamma change what is sent, not what is stored") {
  FullPort port;
  Ws2812Rmt<FullPort, 2> strip(port);
  REQUIRE(ws2812_rmt_test::Start(strip));
  strip.Fill(Colour{200, 100, 50, 0});
  strip.SetBrightness(0);
  REQUIRE(strip.Show().ok());
  CHECK_EQ(static_cast<unsigned>(port.last_wire[0] + port.last_wire[1] + port.last_wire[2]), 0U);
  strip.SetBrightness(255);
  REQUIRE(strip.Show().ok());
  CHECK_EQ(port.last_wire[0], 100U); // GRB
  CHECK_EQ(port.last_wire[1], 200U);
  CHECK_EQ(port.last_wire[2], 50U);
  CHECK(strip.GetPixel(0).value == (Colour{200, 100, 50, 0}));
  strip.SetGamma(Gamma::Gamma22);
  REQUIRE(strip.Show().ok());
  CHECK_EQ(port.last_wire[0], kGamma22[100]);
}

TEST_CASE("Power control works with the hook and is Unsupported without it") {
  FullPort port;
  Config c = Config::Default();
  c.power_on_settle_us = 1234;
  Ws2812Rmt<FullPort, 2> strip(port, c);
  REQUIRE(strip.EnsureInitialized().ok());
  CHECK_EQ(port.power_calls,
           0); // start-up leaves the power pin to the port and to PowerOn()
  REQUIRE(strip.PowerOn().ok());
  CHECK(port.powered);
  CHECK_EQ(port.delayed_us, 1234U);
  REQUIRE(strip.PowerOff().ok());
  CHECK(!port.powered);
  // Off: DIN must not be driven (the input diode back-feeds the supply): nothing is sent.
  const int32_t sent = port.transmit_calls;
  CHECK_EQ(strip.Show().error, Error::NotPowered);
  const std::array<uint8_t, 3> raw = {1, 2, 3};
  CHECK_EQ(strip.TraceWire(raw).error, Error::NotPowered);
  CHECK_EQ(port.transmit_calls, sent);
  REQUIRE(strip.PowerOn().ok());
  REQUIRE(strip.Show().ok()); // the waveform is kept; no re-initialisation
  CHECK_EQ(port.transmit_calls, sent + 1);
  CHECK_EQ(port.init_calls, 1);
  MinimalPort bare;
  Ws2812Rmt<MinimalPort, 2> plain(bare);
  REQUIRE(ws2812_rmt_test::Start(plain));
  CHECK_EQ(plain.PowerOn().error, Error::Unsupported);
  CHECK_EQ(plain.PowerOff().error, Error::Unsupported);
}

TEST_CASE("A port with a supply hook starts unpowered: Show() and TraceWire() before PowerOn() "
          "send nothing") {
  FullPort port;
  Ws2812Rmt<FullPort, 2> strip(port);
  REQUIRE(strip.EnsureInitialized().ok());
  const std::array<uint8_t, 3> raw = {1, 2, 3};
  CHECK_EQ(strip.Show().error, Error::NotPowered);
  CHECK_EQ(strip.TraceWire(raw).error, Error::NotPowered);
  CHECK_EQ(port.transmit_calls, 0);
  REQUIRE(strip.PowerOn().ok());
  CHECK(strip.Show().ok());
  CHECK_EQ(port.transmit_calls, 1);
}

TEST_CASE("After a failed PowerOn() the strip is still unpowered: nothing is sent") {
  FullPort port;
  Ws2812Rmt<FullPort, 2> strip(port);
  REQUIRE(strip.EnsureInitialized().ok());
  port.power_ok = false;
  CHECK_EQ(strip.PowerOn().error, Error::Bus);
  const std::array<uint8_t, 3> raw = {1, 2, 3};
  CHECK_EQ(strip.Show().error, Error::NotPowered);
  CHECK_EQ(strip.TraceWire(raw).error, Error::NotPowered);
  CHECK_EQ(port.transmit_calls, 0);
  port.power_ok = true; // the retry works
  REQUIRE(strip.PowerOn().ok());
  CHECK(strip.Show().ok());
}

TEST_CASE("A port without a supply hook has nothing to switch: Show() works at once") {
  MinimalPort bare;
  Ws2812Rmt<MinimalPort, 2> strip(bare);
  REQUIRE(strip.EnsureInitialized().ok());
  CHECK(strip.Show().ok());
}

TEST_CASE("The driver version getters follow the generated header") {
  CHECK(GetDriverVersionMajor() == 0U);
  CHECK(GetDriverVersionMinor() == 1U);
  CHECK(GetDriverVersionPatch() == 0U);
  CHECK(std::string_view(GetDriverVersion()).starts_with("0."));
}

TEST_CASE("TraceWire() sends the bytes as given, GetLastWire() shows what "
          "Show() encoded") {
  FullPort port;
  Ws2812Rmt<FullPort, 2> strip(port);
  const std::array<uint8_t, 3> raw = {0xDE, 0xAD, 0xBE};
  CHECK_EQ(strip.TraceWire(raw).error, Error::NotInitialized);
  CHECK_EQ(port.transmit_calls, 0);
  REQUIRE(ws2812_rmt_test::Start(strip));
  CHECK(strip.GetLastWire().empty());
  CHECK_EQ(strip.TraceWire(std::span<const uint8_t>()).error, Error::InvalidArgument);
  const std::array<uint8_t, 9> too_long{};
  CHECK_EQ(strip.TraceWire(too_long).error, Error::InvalidArgument);
  CHECK_EQ(port.transmit_calls, 0);
  REQUIRE(strip.TraceWire(raw).ok());
  CHECK_EQ(port.transmit_calls, 1);
  CHECK_EQ(port.last_wire.size(), 3U);
  CHECK_EQ(port.last_wire[0], 0xDEU);
  CHECK(strip.GetLastWire().empty()); // a raw send is not a Show()
  port.fail_transmit_at = port.transmit_calls + 1;
  CHECK_EQ(strip.TraceWire(raw).error, Error::Bus);
  port.fail_transmit_at = 0;
  REQUIRE(strip.SetPixel(1, Colour{1, 2, 3, 0}).ok());
  REQUIRE(strip.Show().ok());
  CHECK_EQ(strip.GetLastWire().size(), 6U);
  CHECK_EQ(strip.GetLastWire()[3], 2U); // pixel 1 green first
}
