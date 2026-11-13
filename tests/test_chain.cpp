/**
 * @file test_chain.cpp
 * @brief End to end: pixel buffer -> wire -> RMT half-symbols -> a chain of
 * pixels reading one datasheet's windows -> latched colours. The oracle is the
 * model of tests/mock_ws2812_rmt.hpp, never the driver.
 */
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string_view>
#include <vector>

#include "mock_ws2812_rmt.hpp"
#include "test_harness.hpp"
#include "ws2812_rmt.hpp"

using namespace ws2812_rmt;
using ws2812_rmt_test::ChannelOfLetter;
using ws2812_rmt_test::Document;
using ws2812_rmt_test::FullPort;
using ws2812_rmt_test::kDocuments;
using ws2812_rmt_test::PixelChainModel;

namespace {

/// Independent reference of the wire value: round(v * b / 255) with the gamma
/// from std::pow.
uint8_t Reference(uint8_t v, uint8_t brightness, bool gamma22) {
  uint32_t g = v;
  if (gamma22) {
    g = static_cast<uint32_t>(
        std::floor((255.0 * std::pow(static_cast<double>(v) / 255.0, 2.2)) + 0.5));
  }
  return static_cast<uint8_t>(((2U * g * brightness) + 255U) / 510U);
}

uint8_t Pick(const Colour& c, Channel ch) {
  return ChannelOf(c, ch);
}

/// Colours: a ramp, a pseudo-random run and the edges, by index.
Colour ColourAt(size_t i) {
  const auto x = static_cast<uint32_t>(i);
  switch (i % 4U) {
  case 0:
    return Colour{static_cast<uint8_t>(x * 7U), static_cast<uint8_t>(x * 13U),
                  static_cast<uint8_t>(x * 29U), static_cast<uint8_t>(x * 3U)};
  case 1:
    return Colour{
        static_cast<uint8_t>((x * 1103515245U) >> 7U), static_cast<uint8_t>((x * 22695477U) >> 5U),
        static_cast<uint8_t>((x * 69069U) >> 9U), static_cast<uint8_t>((x * 214013U) >> 11U)};
  case 2:
    return Colour{255, 0, 255, 0};
  default:
    return Colour{0, 255, 0, 255};
  }
}

/// Run one strip of N pixels through the fake port into a model of `doc`; true
/// when everything matched.
template <size_t N>
bool RunChain(const Document& doc, Variant variant, const std::array<Channel, 4>& perm,
              uint8_t brightness, bool gamma22, int32_t skew_ppm, int32_t* violations) {
  FullPort port;
  port.skew_ppm = skew_ppm;
  Config cfg = Config::Default();
  cfg.variant = variant;
  const size_t width = std::string_view(doc.order).size();
  cfg.use_default_order = false;
  cfg.order = ColourOrder{perm, static_cast<uint8_t>(width)};
  cfg.brightness = brightness;
  cfg.gamma = gamma22 ? Gamma::Gamma22 : Gamma::None;
  auto strip = std::make_unique<Ws2812Rmt<FullPort, N>>(port, cfg);
  if (!ws2812_rmt_test::Start(*strip)) {
    return false;
  }
  for (size_t i = 0; i < N; ++i) {
    if (!strip->SetPixel(i, ColourAt(i)).ok()) {
      return false;
    }
  }
  if (!strip->Show().ok()) {
    return false;
  }
  PixelChainModel model(doc, N);
  model.Consume(port.symbols, port.EffectiveHz());
  *violations += model.violations + model.gaps_too_long + model.idle_violations;
  if ((model.latches != 1) || (model.unlatched_bits != 0)) {
    return false;
  }
  for (size_t i = 0; i < N; ++i) {
    const Colour in = ColourAt(i);
    // The model assigns wire byte k to its document's channel k; the driver
    // sent channel perm[k] there.
    Colour expected{};
    for (size_t k = 0; k < width; ++k) {
      const uint8_t v = Reference(Pick(in, perm[k]), brightness, gamma22);
      switch (ChannelOfLetter(doc.order[k])) {
      case Channel::R:
        expected.r = v;
        break;
      case Channel::G:
        expected.g = v;
        break;
      case Channel::B:
        expected.b = v;
        break;
      case Channel::W:
      default:
        expected.w = v;
        break;
      }
    }
    if (!(model.Latched()[i] == expected)) {
      return false;
    }
  }
  return true;
}

template <size_t N>
void AllOrders(const Document& doc, Variant variant, int32_t skew_ppm, int32_t* violations,
               int32_t* failures) {
  const size_t width = std::string_view(doc.order).size();
  std::array<Channel, 4> perm = {Channel::R, Channel::G, Channel::B, Channel::W};
  do {
    if ((width == 4U) || (perm[3] == Channel::W)) {
      // 3 channels: the first three are a permutation of R, G, B (white is last
      // in the array)
      if (!RunChain<N>(doc, variant, perm, 255, false, skew_ppm, violations)) {
        ++*failures;
      }
    }
  } while (std::next_permutation(perm.begin(), perm.end(), [](Channel a, Channel b) {
    return static_cast<uint8_t>(a) < static_cast<uint8_t>(b);
  }));
}

} // namespace

TEST_CASE("Every variant x every document x every colour order: the latched "
          "colours are the sent "
          "colours") {
  int32_t violations = 0;
  int32_t failures = 0;
  int32_t runs = 0;
  for (const Document& doc : kDocuments) {
    AllOrders<1>(doc, doc.variant, 0, &violations, &failures);
    AllOrders<2>(doc, doc.variant, 0, &violations, &failures);
    AllOrders<3>(doc, doc.variant, 0, &violations, &failures);
    runs += (std::string_view(doc.order).size() == 4U) ? 72 : 18;
  }
  CHECK_EQ(failures, 0);
  CHECK_EQ(violations, 0);
  CHECK(runs > 300);
}

TEST_CASE("Strip lengths 59, 60, 144 and 1024 latch exactly (default orders)") {
  int32_t violations = 0;
  int32_t failures = 0;
  for (const Document& doc : kDocuments) {
    const std::array<Channel, 4> perm = {
        ChannelOfLetter(doc.order[0]), ChannelOfLetter(doc.order[1]), ChannelOfLetter(doc.order[2]),
        (std::string_view(doc.order).size() == 4U) ? ChannelOfLetter(doc.order[3]) : Channel::W};
    failures += RunChain<59>(doc, doc.variant, perm, 255, false, 0, &violations) ? 0 : 1;
    failures += RunChain<60>(doc, doc.variant, perm, 255, false, 0, &violations) ? 0 : 1;
    failures += RunChain<144>(doc, doc.variant, perm, 255, false, 0, &violations) ? 0 : 1;
    failures += RunChain<1024>(doc, doc.variant, perm, 255, false, 0, &violations) ? 0 : 1;
  }
  CHECK_EQ(failures, 0);
  CHECK_EQ(violations, 0);
}

TEST_CASE("Brightness and gamma: the latched colours are the post-brightness "
          "post-gamma colours") {
  int32_t violations = 0;
  int32_t failures = 0;
  const std::array<uint8_t, 5> levels = {0, 1, 100, 200, 255};
  for (const Document& doc : kDocuments) {
    const std::array<Channel, 4> perm = {
        ChannelOfLetter(doc.order[0]), ChannelOfLetter(doc.order[1]), ChannelOfLetter(doc.order[2]),
        (std::string_view(doc.order).size() == 4U) ? ChannelOfLetter(doc.order[3]) : Channel::W};
    for (const uint8_t b : levels) {
      failures += RunChain<7>(doc, doc.variant, perm, b, false, 0, &violations) ? 0 : 1;
      failures += RunChain<7>(doc, doc.variant, perm, b, true, 0, &violations) ? 0 : 1;
    }
  }
  CHECK_EQ(failures, 0);
  CHECK_EQ(violations, 0);
}

TEST_CASE("A tick-rate error of +-1000 ppm stays inside every window") {
  int32_t violations = 0;
  int32_t failures = 0;
  for (const Document& doc : kDocuments) {
    const std::array<Channel, 4> perm = {
        ChannelOfLetter(doc.order[0]), ChannelOfLetter(doc.order[1]), ChannelOfLetter(doc.order[2]),
        (std::string_view(doc.order).size() == 4U) ? ChannelOfLetter(doc.order[3]) : Channel::W};
    for (const int32_t ppm : {-1000, 1000}) {
      failures += RunChain<30>(doc, doc.variant, perm, 255, false, ppm, &violations) ? 0 : 1;
    }
  }
  CHECK_EQ(failures, 0);
  CHECK_EQ(violations, 0);
}

TEST_CASE("A waveform of the wrong generation is flagged by the model, not "
          "accepted") {
  int32_t violations = 0;
  for (const Document& doc : kDocuments) {
    if (std::string_view(doc.id) == "b-mar17") {
      const std::array<Channel, 4> perm = {Channel::G, Channel::R, Channel::B, Channel::W};
      // T1L 600 against 220..420: the chain may still latch, but the model counts the violation.
      static_cast<void>(RunChain<5>(doc, Variant::Ws2812bV5, perm, 255, false, 0, &violations));
    }
  }
  CHECK(violations > 0);
}

TEST_CASE("The reset latches only above its minimum, and a frame needs one to show") {
  const ws2812_rmt_test::Document& d = kDocuments[5]; // b-v5: reset above 280 us
  CHECK(std::string_view(d.id) == std::string_view("b-v5"));
  const auto build = [](uint64_t reset_ticks) {
    std::vector<ws2812_rmt_test::HalfSymbol> s;
    for (int32_t i = 0; i < 24; ++i) { // 24 one-bits: 700 high, 600 low at 1 ns ticks
      s.push_back({true, 700});
      s.push_back({false, 600});
    }
    s.push_back({false, reset_ticks});
    return s;
  };
  PixelChainModel at_min(d, 1);
  at_min.Consume(build(280'000U - 600U),
                 1'000'000'000ULL); // total low exactly 280 us
  CHECK_EQ(at_min.latches, 0);
  CHECK_EQ(at_min.unlatched_bits, 24);
  PixelChainModel above(d, 1);
  above.Consume(build(280'001U - 600U), 1'000'000'000ULL);
  CHECK_EQ(above.latches, 1);
  CHECK(above.Latched()[0] == (Colour{255, 255, 255, 0}));
  PixelChainModel no_reset(d, 1);
  std::vector<ws2812_rmt_test::HalfSymbol> s = build(1);
  s.pop_back();
  no_reset.Consume(s, 1'000'000'000ULL);
  CHECK_EQ(no_reset.latches, 0);
  CHECK(no_reset.Latched()[0] == (Colour{}));
}

TEST_CASE("The model rejects a frame that starts low (an idle level that is "
          "not low)") {
  PixelChainModel m(kDocuments[5], 1);
  m.Consume({{false, 100}, {true, 300}, {false, 700}}, 1'000'000'000ULL);
  CHECK_EQ(m.idle_violations, 1);
}

TEST_CASE("Pixels beyond the strip are ignored by the chain, fewer than the "
          "frame carries fall off") {
  const Document& d = kDocuments[5];
  FullPort port;
  auto strip = std::make_unique<Ws2812Rmt<FullPort, 4>>(port);
  REQUIRE(ws2812_rmt_test::Start(*strip));
  strip->Fill(Colour{9, 8, 7, 0});
  REQUIRE(strip->Show().ok());
  PixelChainModel longer(d, 6); // two pixels more than were sent
  longer.Consume(port.symbols, port.EffectiveHz());
  CHECK(longer.Latched()[3] == (Colour{9, 8, 7, 0}));
  CHECK(longer.Latched()[4] == (Colour{}));
  PixelChainModel shorter(d, 2); // two pixels fewer
  shorter.Consume(port.symbols, port.EffectiveHz());
  CHECK(shorter.Latched()[1] == (Colour{9, 8, 7, 0}));
}
