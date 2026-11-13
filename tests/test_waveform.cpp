/**
 * @file test_waveform.cpp
 * @brief `NsToTicks`, `ResolveWaveform` and `WaveformInWindow` against the
 * model's per-document windows (datasheet timing matrix rows 1 to 8,
 * docs/datasheet.md).
 */
#include <array>
#include <cstdint>
#include <cstdio>
#include <string_view>

#include "mock_ws2812_rmt.hpp"
#include "test_harness.hpp"
#include "ws2812_rmt_functions.hpp"

using namespace ws2812_rmt;
using ws2812_rmt_test::kDocuments;
using ws2812_rmt_test::WaveInDocument;

// Build-time pins of the resolver, one per profile at 20 MHz (50 ns ticks): t0h, t0l, t1h, t1l,
// reset, in-frame gap limit. The values are the datasheet windows' nearest tick counts
// (docs/timing.md).
namespace pins {
constexpr Waveform Wave(Variant v) {
  return ResolveWaveform(ProfileTiming(v), 20'000'000, 0).value;
}
constexpr bool Is(const Waveform& w, uint32_t t0h, uint32_t t0l, uint32_t t1h, uint32_t t1l,
                  uint32_t reset, uint32_t gap) {
  return (w.t0h_ticks == t0h) && (w.t0l_ticks == t0l) && (w.t1h_ticks == t1h) &&
         (w.t1l_ticks == t1l) && (w.reset_ticks == reset) && (w.max_gap_ticks == gap);
}
static_assert(Is(Wave(Variant::Ws2812), 7, 16, 14, 12, 1400, 899), "WS2812: 350/800/700/600 ns");
static_assert(Is(Wave(Variant::Ws2812bPreV5), 6, 17, 16, 7, 6400, 899),
              "WS2812B 2013 to 2017: reset above 300 us, gap below the 50 us latch");
static_assert(Is(Wave(Variant::Ws2812bV5), 7, 19, 13, 13, 6000, 5039),
              "WS2812B V5, V6: 350/950/650/650 ns, period 1300 ns");
static_assert(Is(Wave(Variant::Ws2812c), 6, 18, 14, 7, 6000, 5039), "WS2812C");
static_assert(Is(Wave(Variant::Ws2812d), 6, 18, 17, 6, 6000, 5039), "WS2812D-F8");
static_assert(Is(Wave(Variant::Sk6812), 6, 18, 12, 12, 2000, 899), "SK6812 Rev 01");
static_assert(Is(Wave(Variant::Sk6812Rgbw), 6, 19, 13, 12, 2000, 359),
              "SK6812RGBW, low below 20 us");
static_assert(Is(Wave(Variant::Sk6812Mini), 6, 19, 13, 12, 2000, 359),
              "SK6812MINI, low below 20 us");
// The encoder and the decoder are inverses at full brightness, and brightness 0 sends zeros.
constexpr bool RoundTrip() {
  const std::array<Colour, 2> px{Colour{.r = 200, .g = 100, .b = 50, .w = 7},
                                 Colour{.r = 1, .g = 2, .b = 3, .w = 4}};
  std::array<uint8_t, 8> wire{};
  std::array<Colour, 2> back{};
  const bool encoded = EncodeFrame(px, kOrderGrbw, 255, Gamma::None, wire) == 8U;
  const bool decoded = DecodeWire(wire, kOrderGrbw, back) == 2U;
  std::array<uint8_t, 8> dark{};
  const bool off = (EncodeFrame(px, kOrderGrbw, 0, Gamma::None, dark) == 8U) && (dark[0] == 0U);
  return encoded && decoded && off && (back[0] == px[0]) && (back[1] == px[1]) &&
         (wire[0] == 100U) && (wire[1] == 200U) && (wire[2] == 50U) && (wire[3] == 7U);
}
static_assert(RoundTrip(), "encode then decode returns the pixels, in the order GRBW");
static_assert((Scale8(255, 255) == 255U) && (Scale8(255, 0) == 0U) && (Scale8(128, 255) == 128U),
              "brightness scaling is exact at the ends");
} // namespace pins

namespace {

constexpr std::array<Variant, 8> kNamed = {
    Variant::Ws2812,  Variant::Ws2812bPreV5, Variant::Ws2812bV5,  Variant::Ws2812c,
    Variant::Ws2812d, Variant::Sk6812,       Variant::Sk6812Rgbw, Variant::Sk6812Mini};
constexpr std::array<uint32_t, 9> kRates = {5'000'000,  8'000'000,  10'000'000,
                                            16'000'000, 20'000'000, 32'000'000,
                                            40'000'000, 64'000'000, 80'000'000};

/// Reference rounding without a 128-bit type: split the seconds off first.
uint64_t ReferenceTicks(uint64_t ns, uint64_t hz) {
  const uint64_t whole = ns / 1'000'000'000ULL;
  const uint64_t rest = ns % 1'000'000'000ULL;
  return (whole * hz) + (((rest * hz) + 500'000'000ULL) / 1'000'000'000ULL);
}

} // namespace

TEST_CASE("NsToTicks equals the reference on a grid, halves up") {
  const std::array<uint64_t, 14> ns = {0,
                                       1,
                                       49,
                                       50,
                                       51,
                                       99,
                                       100,
                                       350,
                                       1'000,
                                       280'000,
                                       999'999'999,
                                       1'000'000'000,
                                       4'294'967'295ULL,
                                       3'000'000'001ULL};
  const std::array<uint64_t, 9> hz = {
      1,          7,          1'000'000,     5'000'000,       10'000'000,
      20'000'000, 80'000'000, 1'000'000'000, 4'294'967'295ULL};
  for (const uint64_t n : ns) {
    for (const uint64_t h : hz) {
      CHECK_EQ(NsToTicks(n, h), ReferenceTicks(n, h));
    }
  }
  CHECK_EQ(NsToTicks(25, 20'000'000), 1U); // 0.5 tick rounds up
  CHECK_EQ(NsToTicks(24, 20'000'000), 0U);
  CHECK_EQ(NsToTicks(75, 20'000'000), 2U);
}

TEST_CASE("Every named variant at every rate: unreachable, or inside every "
          "document of the variant") {
  int32_t reachable = 0;
  for (const Variant v : kNamed) {
    for (const uint32_t hz : kRates) {
      const auto w = ResolveWaveform(ProfileTiming(v), hz, 0);
      if (!w) {
        CHECK_EQ(w.error, Error::TimingUnreachable);
        continue;
      }
      ++reachable;
      CHECK_EQ(w.value.resolution_hz, hz);
      int32_t documents = 0;
      for (const auto& d : kDocuments) {
        if (d.variant == v) {
          ++documents;
          if (!WaveInDocument(d, w.value)) {
            static_cast<void>(std::printf("    variant %" PRId32 " at %u Hz outside document %s\n",
                                          static_cast<int32_t>(v), static_cast<unsigned>(hz),
                                          d.id));
          }
          CHECK(WaveInDocument(d, w.value));
        }
      }
      CHECK(documents > 0);
      CHECK(WaveformInWindow(v, w.value));
      CHECK(w.value.max_gap_ticks > 0U);
      CHECK(w.value.reset_ticks > w.value.max_gap_ticks);
    }
  }
  CHECK(reachable > 30);
}

TEST_CASE("The recommended and the minimum rate reach every named variant") {
  for (const Variant v : kNamed) {
    CHECK(ResolveWaveform(ProfileTiming(v), kRecommendedResolutionHz, 0).ok());
    CHECK(ResolveWaveform(ProfileTiming(v), kMinResolutionHz, 0).ok());
  }
}

TEST_CASE("Rates the profile cannot meet are reported, never turned into a bad "
          "waveform") {
  CHECK_EQ(ResolveWaveform(ProfileTiming(Variant::Ws2812bPreV5), 5'000'000, 0).error,
           Error::TimingUnreachable);
  CHECK_EQ(ResolveWaveform(ProfileTiming(Variant::Ws2812bV5), 0, 0).error,
           Error::TimingUnreachable);
  CHECK_EQ(ResolveWaveform(ProfileTiming(Variant::Ws2812bV5), 1, 0).error,
           Error::TimingUnreachable);
  CHECK_EQ(ResolveWaveform(ProfileTiming(Variant::Ws2812bV5), 4'294'967'295U, 0).error,
           Error::TimingUnreachable);
  CHECK_EQ(ResolveWaveform(ProfileTiming(Variant::Ws2812bV5), kMaxResolutionHz + 1U, 0).error,
           Error::TimingUnreachable);
  CHECK_EQ(ResolveWaveform(ProfileTiming(Variant::Ws2812bV5), 20'000'000, kMaxResetUs + 1U).error,
           Error::TimingUnreachable);
  CHECK_EQ(ResolveWaveform(Timing{}, 20'000'000, 0).error,
           Error::TimingUnreachable); // not a valid timing
  for (uint32_t hz = 1; hz < 2'000'000; hz += 99'991U) {
    for (const Variant v : kNamed) {
      const auto w = ResolveWaveform(ProfileTiming(v), hz, 0);
      CHECK(!w.ok()); // below 2 MHz no tick count fits a 100 to 400 ns window
    }
  }
  const auto top = ResolveWaveform(ProfileTiming(Variant::Ws2812bV5), kMaxResolutionHz, 0);
  CHECK(top.ok()); // 1 ns ticks reach every window
  CHECK(WaveformInWindow(Variant::Ws2812bV5, top.value));
}

TEST_CASE("Known waveforms at 20 MHz (50 ns ticks)") {
  const auto v5 = ResolveWaveform(ProfileTiming(Variant::Ws2812bV5), 20'000'000, 0);
  REQUIRE(v5.ok());
  CHECK_EQ(v5.value.t0h_ticks,
           7U); // 350 ns (a 300 ns aim would put the period on its 1250 ns minimum)
  CHECK_EQ(v5.value.t0l_ticks, 19U);       // 950 ns: the period is 1300 ns
  CHECK_EQ(v5.value.t1h_ticks, 13U);       // 650 ns
  CHECK_EQ(v5.value.t1l_ticks, 13U);       // 650 ns: 70 ns from both edges, bit period 1300 ns
  CHECK_EQ(v5.value.reset_ticks, 6000U);   // 280 us plus 20 us
  CHECK_EQ(v5.value.max_gap_ticks, 5039U); // 252 us (280 us less 10 percent), strictly below
  const auto sk = ResolveWaveform(ProfileTiming(Variant::Sk6812), 20'000'000, 0);
  REQUIRE(sk.ok());
  CHECK_EQ(sk.value.reset_ticks, 2000U); // 80 us plus 20 us
  const auto rgbw = ResolveWaveform(ProfileTiming(Variant::Sk6812Rgbw), 20'000'000, 0);
  REQUIRE(rgbw.ok());
  CHECK_EQ(rgbw.value.max_gap_ticks, 359U); // 18 us (20 us less 10 percent), strictly below
}

TEST_CASE("The period minimum lengthens the low of a bit; a period maximum "
          "that cannot be met is "
          "unreachable") {
  Timing t = ProfileTiming(Variant::Ws2812bV5);
  t.t0l.nominal_ns = 600; // the period would be 900 ns
  const auto w = ResolveWaveform(t, 20'000'000, 0);
  REQUIRE(w.ok());
  CHECK_EQ(w.value.t0h_ticks + w.value.t0l_ticks, 25U); // raised to 1250 ns
  Timing tight = ProfileTiming(Variant::Ws2812bV5);
  tight.period_max_ns = 1300;
  tight.period_min_ns = 1250;
  tight.t0h.nominal_ns = 380;
  tight.t0l.nominal_ns = 1000; // 1380 ns, above the maximum
  CHECK_EQ(ResolveWaveform(tight, 20'000'000, 0).error, Error::TimingUnreachable);
  Timing raise = ProfileTiming(Variant::Ws2812bV5);
  raise.t0l.max_ns = 700; // the low cannot be raised far enough: 300 + 700 < 1250
  raise.t0l.nominal_ns = 700;
  CHECK_EQ(ResolveWaveform(raise, 20'000'000, 0).error, Error::TimingUnreachable);
}

TEST_CASE("The reset is strictly above the minimum and follows reset_us") {
  const Timing t = ProfileTiming(Variant::Ws2812bV5);
  for (const uint32_t hz : {10'000'000U, 20'000'000U, 40'000'000U, 1'000'000'000U}) {
    const auto def = ResolveWaveform(t, hz, 0);
    REQUIRE(def.ok());
    CHECK(static_cast<uint64_t>(def.value.reset_ticks) * 1'000'000'000ULL > 280'000ULL * hz);
    const auto at_min =
        ResolveWaveform(t, hz, 280); // not above: raised by one tick, never at the minimum
    REQUIRE(at_min.ok());
    CHECK(static_cast<uint64_t>(at_min.value.reset_ticks) * 1'000'000'000ULL > 280'000ULL * hz);
  }
  const auto longer = ResolveWaveform(t, 20'000'000, 1000);
  REQUIRE(longer.ok());
  CHECK_EQ(longer.value.reset_ticks, 20000U);
}

TEST_CASE("WaveformInWindow: windows are inclusive, the reset is strict (1 "
          "GHz, 1 ns ticks)") {
  Waveform w{};
  w.resolution_hz = 1'000'000'000;
  w.t0h_ticks = 250;
  w.t0l_ticks = 1000;
  w.t1h_ticks = 580;
  w.t1l_ticks = 700;
  w.reset_ticks = 280'001;
  w.max_gap_ticks = 1;
  CHECK(WaveformInWindow(Variant::Ws2812bV5, w));
  w.t0h_ticks = 219;
  CHECK(!WaveformInWindow(Variant::Ws2812bV5, w));
  w.t0h_ticks = 380;
  CHECK(WaveformInWindow(Variant::Ws2812bV5, w));
  w.t0h_ticks = 381;
  CHECK(!WaveformInWindow(Variant::Ws2812bV5, w));
  w.t0h_ticks = 300;
  w.t0l_ticks = 1001;
  CHECK(!WaveformInWindow(Variant::Ws2812bV5, w));
  w.t0l_ticks = 950;
  w.t1l_ticks = 579;
  CHECK(!WaveformInWindow(Variant::Ws2812bV5, w));
  w.t1l_ticks = 700;
  w.reset_ticks = 280'000;
  CHECK(!WaveformInWindow(Variant::Ws2812bV5, w));
  w.reset_ticks = 280'001;
  CHECK(WaveformInWindow(Variant::Ws2812bV5, w));
  w.t0l_ticks = 900; // 300 + 900 is below the 1250 ns data cycle
  CHECK(!WaveformInWindow(Variant::Ws2812bV5, w));
  w.t0l_ticks = 950;
  CHECK(!WaveformInWindow(Variant::Custom, w));
  CHECK(!WaveformInWindow(ws2812_rmt_test::RawEnum<Variant>(77), w));
  w.resolution_hz = 0;
  CHECK(!WaveformInWindow(Variant::Ws2812bV5, w));
}

TEST_CASE("WaveformInWindow truth table: the V5 nominals fit no WS2812B document of an "
          "earlier generation") {
  const auto v5 = ResolveWaveform(ProfileTiming(Variant::Ws2812bV5), 20'000'000, 0);
  REQUIRE(v5.ok());
  CHECK(WaveformInWindow(Variant::Ws2812bV5, v5.value));
  CHECK(!WaveformInWindow(Variant::Ws2812bPreV5, v5.value));
  CHECK(WaveformInWindow(Variant::Sk6812Rgbw,
                         v5.value)); // the V5 waveform is also inside the SK6812RGBW windows
  for (const auto& d : kDocuments) {
    const bool inside = WaveInDocument(d, v5.value);
    const std::string_view id = d.id;
    if ((id == "b-af") || (id == "b-mar17") || (id == "b-cn") || (id == "d-f8")) {
      CHECK(!inside);
    }
  }
  const auto old = ResolveWaveform(ProfileTiming(Variant::Ws2812bPreV5), 20'000'000, 0);
  REQUIRE(old.ok());
  CHECK(!WaveformInWindow(Variant::Ws2812bV5, old.value));
}

TEST_CASE("Hostile rates: a swept rate never yields an out-of-window waveform") {
  uint32_t hz = 1'500'000;
  for (int32_t i = 0; i < 400; ++i) {
    hz = hz + 2'399'999U;
    for (const Variant v : kNamed) {
      const auto w = ResolveWaveform(ProfileTiming(v), hz, 0);
      if (w) {
        CHECK(WaveformInWindow(v, w.value));
        for (const auto& d : kDocuments) {
          if (d.variant == v) {
            CHECK(WaveInDocument(d, w.value));
          }
        }
      }
    }
  }
}

TEST_CASE("A low phase above the gap limit handed to the port is refused, not sent") {
  // The nominal of an unbounded low sits near the cap: 48 us is 960 ticks at 20 MHz, above the
  // 899 ticks (10 percent below the 50 us cap, less one tick) the port is told never to exceed.
  Timing t = ProfileTiming(Variant::Ws2812bPreV5);
  t.period_max_ns = 0; // only the gap limit can refuse a long low
  t.t1l = MakeRange(300, 0, 48'000);
  CHECK_EQ(ResolveWaveform(t, 20'000'000, 0).error, Error::TimingUnreachable);
  t.t1l = MakeRange(300, 0, 40'000); // 800 ticks: below the limit, accepted
  const auto ok = ResolveWaveform(t, 20'000'000, 0);
  REQUIRE(ok.ok());
  CHECK(ok.value.t1l_ticks <= ok.value.max_gap_ticks);
  // The same for the 0 code.
  t = ProfileTiming(Variant::Ws2812bPreV5);
  t.period_max_ns = 0;
  t.t0l = MakeRange(750, 0, 48'000);
  CHECK_EQ(ResolveWaveform(t, 20'000'000, 0).error, Error::TimingUnreachable);
}

TEST_CASE("ResolveRmtRate: the divider is rounded to the nearest integer, as ESP-IDF v5.5 does") {
  CHECK_EQ(ResolveRmtRate(80'000'000U, 20'000'000U).value, 20'000'000U);
  CHECK_EQ(ResolveRmtRate(160'000'000U, 20'000'000U).value, 20'000'000U);
  // A source just below four times the request: (79.999 + 10) / 20 rounds to 4, so the channel
  // runs at 19.99975 MHz, 12.5 ppm below the request (a floored divider, 3, would give 26.7 MHz).
  CHECK_EQ(ResolveRmtRate(79'999'000U, 20'000'000U).value, 19'999'750U);
  // A request just above a quarter of the clock: 80 / 20.01 = 3.998 rounds to 4, 20 MHz, 500 ppm
  // below the request; the quotient (3) would be 26.7 MHz and refused.
  CHECK_EQ(ResolveRmtRate(80'000'000U, 20'010'000U).value, 20'000'000U);
  // 500 ppm high is accepted and reported as it is; 1250 ppm is refused, unless the caller allows
  // it.
  const auto near = ResolveRmtRate(80'040'000U, 20'000'000U);
  REQUIRE(near.ok());
  CHECK_EQ(near.value, 20'010'000U);
  CHECK_EQ(ResolveRmtRate(80'100'000U, 20'000'000U).error, Error::TimingUnreachable);
  CHECK(ResolveRmtRate(80'100'000U, 20'000'000U, 2000U).ok());
  // The tolerance is inclusive: 20.02 MHz against 20 MHz is exactly 1000 ppm.
  CHECK_EQ(ResolveRmtRate(80'080'000U, 20'000'000U).value, 20'020'000U);
  CHECK_EQ(ResolveRmtRate(80'080'000U, 20'000'000U, 999U).error, Error::TimingUnreachable);
  // A divider above 256 moves to the group prescaler: 80.0625 MHz / 100 kHz needs 801; the group
  // prescaler 4 gives 20.015625 MHz and the channel divider 200, so 100'078 Hz.
  const auto grouped = ResolveRmtRate(80'062'500U, 100'000U);
  REQUIRE(grouped.ok());
  CHECK_EQ(grouped.value, 100'078U);
  // ESP32-H2: the default RMT clock is the 32 MHz crystal; 20 MHz is not reachable (the divider 2
  // gives 16 MHz), 32 MHz is.
  CHECK_EQ(ResolveRmtRate(32'000'000U, 20'000'000U).error, Error::TimingUnreachable);
  CHECK_EQ(ResolveRmtRate(32'000'000U, 32'000'000U).value, 32'000'000U);
  // Not reachable at all.
  CHECK_EQ(ResolveRmtRate(40'000'000U, 16'000'000U).error, Error::TimingUnreachable); // 13.3 MHz
  CHECK_EQ(ResolveRmtRate(10'000'000U, 20'000'000U).error, Error::TimingUnreachable);
  CHECK_EQ(ResolveRmtRate(10'000'000U, 25'000'000U).error, Error::TimingUnreachable); // divider 0
  CHECK_EQ(ResolveRmtRate(80'000'000U, 0U).error, Error::TimingUnreachable);
  CHECK_EQ(ResolveRmtRate(0U, 20'000'000U).error, Error::TimingUnreachable);
}
