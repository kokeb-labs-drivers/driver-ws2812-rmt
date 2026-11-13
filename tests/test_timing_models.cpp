/**
 * @file test_timing_models.cpp
 * @brief Each profile equals the intersection, computed here, of the windows of
 * the datasheets of its variant (docs/timing.md); a mistyped profile fails.
 * Also the fact that two WS2812B generations share no waveform.
 */
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <string_view>

#include "mock_ws2812_rmt.hpp"
#include "test_harness.hpp"
#include "ws2812_rmt_functions.hpp"

using namespace ws2812_rmt;
using ws2812_rmt_test::Document;
using ws2812_rmt_test::kDocuments;
using ws2812_rmt_test::Win;

namespace {

constexpr std::array<Variant, 8> kNamed = {
    Variant::Ws2812,  Variant::Ws2812bPreV5, Variant::Ws2812bV5,  Variant::Ws2812c,
    Variant::Ws2812d, Variant::Sk6812,       Variant::Sk6812Rgbw, Variant::Sk6812Mini};

/// The intersection of the windows; a maximum of 0 stays 0 only when no
/// document gives one.
Win Intersect(Variant v, Win Document::* field) {
  Win out{0, 0};
  for (const Document& d : kDocuments) {
    if (d.variant != v) {
      continue;
    }
    const Win w = d.*field;
    out.min_ns = std::max(out.min_ns, w.min_ns);
    if (w.max_ns != 0U) {
      out.max_ns = (out.max_ns == 0U) ? w.max_ns : std::min(out.max_ns, w.max_ns);
    }
  }
  return out;
}

uint32_t MaxOf(Variant v, uint32_t Document::* field) {
  uint32_t out = 0;
  for (const Document& d : kDocuments) {
    if (d.variant == v) {
      out = std::max(out, d.*field);
    }
  }
  return out;
}

/// The smallest low time inside a frame that any document of the variant lets latch: its
/// `max_low_ns`, or its reset minimum when it gives none.
uint32_t MinLatch(Variant v) {
  uint32_t out = 0;
  for (const Document& d : kDocuments) {
    if (d.variant == v) {
      const uint32_t cap = (d.max_low_ns != 0U) ? d.max_low_ns : d.reset_min_ns;
      out = (out == 0U) ? cap : std::min(out, cap);
    }
  }
  return out;
}

uint32_t MinNonZero(Variant v, uint32_t Document::* field) {
  uint32_t out = 0;
  for (const Document& d : kDocuments) {
    if ((d.variant == v) && ((d.*field) != 0U)) {
      out = (out == 0U) ? d.*field : std::min(out, d.*field);
    }
  }
  return out;
}

void CheckRange(const char* what, const Range& r, const Win& w) {
  if ((r.min_ns != w.min_ns) || (r.max_ns != w.max_ns)) {
    static_cast<void>(std::printf("    %s: profile %u..%u, intersection %u..%u\n", what,
                                  static_cast<unsigned>(r.min_ns), static_cast<unsigned>(r.max_ns),
                                  static_cast<unsigned>(w.min_ns),
                                  static_cast<unsigned>(w.max_ns)));
  }
  CHECK_EQ(r.min_ns, w.min_ns);
  CHECK_EQ(r.max_ns, w.max_ns);
  CHECK(r.nominal_ns >= w.min_ns);
  CHECK((w.max_ns == 0U) || (r.nominal_ns <= w.max_ns));
}

} // namespace

TEST_CASE("Every profile window equals the intersection of its documents") {
  for (const Variant v : kNamed) {
    const Timing t = ProfileTiming(v);
    CheckRange("t0h", t.t0h, Intersect(v, &Document::t0h));
    CheckRange("t0l", t.t0l, Intersect(v, &Document::t0l));
    CheckRange("t1h", t.t1h, Intersect(v, &Document::t1h));
    CheckRange("t1l", t.t1l, Intersect(v, &Document::t1l));
    CHECK_EQ(t.period_min_ns, MaxOf(v, &Document::period_min_ns));
    CHECK_EQ(t.period_max_ns, MinNonZero(v, &Document::period_max_ns));
    CHECK_EQ(t.reset_min_ns, MaxOf(v, &Document::reset_min_ns));
    // The in-frame limit is the smallest latch time of any document, not the largest reset.
    CHECK_EQ(t.max_low_ns, (MinLatch(v) < t.reset_min_ns) ? MinLatch(v) : 0U);
    CHECK(detail::TimingValid(t));
  }
}

TEST_CASE("The in-frame gap limit stays below the smallest latch time of every document of the "
          "variant, with a 10 percent margin, at every rate") {
  const std::array<uint32_t, 4> rates = {16'000'000, 20'000'000, 40'000'000, 80'000'000};
  for (const Variant v : kNamed) {
    for (const uint32_t hz : rates) {
      const auto w = ResolveWaveform(ProfileTiming(v), hz, 0);
      REQUIRE(w.ok());
      const uint64_t gap_ns = TicksToNs(w.value.max_gap_ticks, hz);
      const uint64_t latch_ns = MinLatch(v);
      CHECK(gap_ns < latch_ns);
      CHECK((gap_ns * 10U) <= (latch_ns * 9U));
      // and it still allows a low well above the bit lows
      CHECK(w.value.max_gap_ticks > (w.value.t0l_ticks + w.value.t1l_ticks));
    }
  }
}

TEST_CASE("Every variant has at least one document and every document belongs "
          "to a named variant") {
  for (const Variant v : kNamed) {
    int32_t n = 0;
    for (const Document& d : kDocuments) {
      n += (d.variant == v) ? 1 : 0;
    }
    CHECK(n > 0);
  }
  for (const Document& d : kDocuments) {
    CHECK(d.variant != Variant::Custom);
  }
}

TEST_CASE("The nominals of each profile lie inside every document of the "
          "variant at 1 ns resolution") {
  for (const Variant v : kNamed) {
    const auto w = ResolveWaveform(ProfileTiming(v), kMaxResolutionHz, 0);
    REQUIRE(w.ok());
    for (const Document& d : kDocuments) {
      if (d.variant == v) {
        CHECK(ws2812_rmt_test::WaveInDocument(d, w.value));
      }
    }
  }
}

TEST_CASE("Two WS2812B generations have no common waveform") {
  const Timing a = ProfileTiming(Variant::Ws2812bPreV5);
  const Timing b = ProfileTiming(Variant::Ws2812bV5);
  CHECK(a.t1l.max_ns < b.t1l.min_ns); // 420 against 580
  CHECK(a.reset_min_ns > b.reset_min_ns);
}

TEST_CASE("DefaultOrder and OrderOf follow the documents") {
  for (const Document& d : kDocuments) {
    const ColourOrder o = DefaultOrder(d.variant);
    const std::string_view order = d.order;
    if ((std::string_view(d.id) == "skw-01") || (std::string_view(d.id) == "skw-03")) {
      CHECK(!(o == kOrderRgbw)); // the default is the later revision; the order
                                 // is a Config field
      continue;
    }
    CHECK_EQ(static_cast<size_t>(o.count), order.size());
    for (size_t i = 0; i < order.size(); ++i) {
      CHECK(o.ch[i] == ws2812_rmt_test::ChannelOfLetter(order[i]));
    }
  }
  Config c;
  c.variant = Variant::Sk6812Rgbw;
  c.use_default_order = false;
  c.order = kOrderRgbw;
  CHECK(OrderOf(c) == kOrderRgbw);
  CHECK(DefaultOrder(Variant::Custom).count == 0U);
}
