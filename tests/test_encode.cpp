/**
 * @file test_encode.cpp
 * @brief Bit-exact golden vectors of the pixel encoding (datasheet "composition
 * of 24bit data": GRB, high bit first; SK6812RGBW "data structure of 32bit"),
 * every colour order, the inverse.
 */
#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

#include "mock_ws2812_rmt.hpp"
#include "test_harness.hpp"
#include "ws2812_rmt_functions.hpp"

using namespace ws2812_rmt;

TEST_CASE("One pixel (0x12, 0x34, 0x56) in GRB order is 34 12 56") {
  std::array<uint8_t, 4> out{0xEE, 0xEE, 0xEE, 0xEE};
  const Colour c{0x12, 0x34, 0x56, 0x78};
  CHECK_EQ(EncodePixel(c, kOrderGrb, 255, Gamma::None, out), 3U);
  CHECK_EQ(out[0], 0x34U);
  CHECK_EQ(out[1], 0x12U);
  CHECK_EQ(out[2], 0x56U);
  CHECK_EQ(out[3],
           0xEEU); // the white byte is not written for a three-channel order
}

TEST_CASE("RGBW: the white byte goes where the order says") {
  std::array<uint8_t, 4> out{};
  const Colour c{1, 2, 3, 4};
  CHECK_EQ(EncodePixel(c, kOrderGrbw, 255, Gamma::None, out), 4U);
  CHECK(out == (std::array<uint8_t, 4>{2, 1, 3, 4}));
  CHECK_EQ(EncodePixel(c, kOrderRgbw, 255, Gamma::None, out), 4U);
  CHECK(out == (std::array<uint8_t, 4>{1, 2, 3, 4}));
}

TEST_CASE("A frame is the pixels back to back, pixel 0 first") {
  const std::array<Colour, 3> px = {Colour{1, 2, 3, 0}, Colour{4, 5, 6, 0}, Colour{7, 8, 9, 0}};
  std::array<uint8_t, 9> wire{};
  CHECK_EQ(EncodeFrame(px, kOrderRgb, 255, Gamma::None, wire), 9U);
  CHECK(wire == (std::array<uint8_t, 9>{1, 2, 3, 4, 5, 6, 7, 8, 9}));
  CHECK_EQ(EncodeFrame(px, kOrderGrb, 255, Gamma::None, wire), 9U);
  CHECK(wire == (std::array<uint8_t, 9>{2, 1, 3, 5, 4, 6, 8, 7, 9}));
}

TEST_CASE("Brightness 0 and 255, and the gamma-then-brightness order") {
  std::array<uint8_t, 3> out{};
  const Colour c{255, 128, 1, 0};
  CHECK_EQ(EncodePixel(c, kOrderRgb, 0, Gamma::None, out), 3U);
  CHECK(out == (std::array<uint8_t, 3>{0, 0, 0}));
  CHECK_EQ(EncodePixel(c, kOrderRgb, 255, Gamma::None, out), 3U);
  CHECK(out == (std::array<uint8_t, 3>{255, 128, 1}));
  CHECK_EQ(EncodePixel(c, kOrderRgb, 255, Gamma::Gamma22, out), 3U);
  CHECK_EQ(out[0], 255U);
  CHECK_EQ(out[1], kGamma22[128]);
  CHECK_EQ(out[2], 0U);
  CHECK_EQ(EncodePixel(c, kOrderRgb, 128, Gamma::None, out), 3U);
  CHECK_EQ(out[0], 128U);
  CHECK_EQ(out[1], 64U);
}

TEST_CASE("Too small an output or an invalid order writes nothing and returns 0") {
  std::array<uint8_t, 2> small{0xAA, 0xAA};
  CHECK_EQ(EncodePixel(Colour{1, 2, 3, 4}, kOrderGrb, 255, Gamma::None, small), 0U);
  CHECK_EQ(small[0], 0xAAU);
  ColourOrder bad = kOrderGrb;
  bad.ch[1] = Channel::G; // repeated channel
  std::array<uint8_t, 4> out{0xAA, 0xAA, 0xAA, 0xAA};
  CHECK_EQ(EncodePixel(Colour{1, 2, 3, 4}, bad, 255, Gamma::None, out), 0U);
  CHECK_EQ(out[0], 0xAAU);
  const std::array<Colour, 2> px{};
  std::array<uint8_t, 5> five{};
  CHECK_EQ(EncodeFrame(px, kOrderGrb, 255, Gamma::None, five),
           0U); // 6 bytes needed
  CHECK_EQ(EncodeFrame(px, bad, 255, Gamma::None, out), 0U);
  CHECK_EQ(DecodeWire(five, bad, std::span<Colour>{}), 0U);
}

TEST_CASE("IsValidOrder: counts, repeats, white in a three-channel order, "
          "unknown channels") {
  CHECK(IsValidOrder(kOrderGrb) && IsValidOrder(kOrderRgb) && IsValidOrder(kOrderGrbw) &&
        IsValidOrder(kOrderRgbw));
  ColourOrder o = kOrderGrb;
  o.count = 2;
  CHECK(!IsValidOrder(o));
  o.count = 5;
  CHECK(!IsValidOrder(o));
  o = kOrderGrb;
  o.ch[2] = Channel::W; // G, R, W: white in a three-channel order
  CHECK(!IsValidOrder(o));
  o = kOrderGrbw;
  o.ch[3] = Channel::R; // repeated
  CHECK(!IsValidOrder(o));
  o = kOrderGrb;
  o.ch[0] = ws2812_rmt_test::RawEnum<Channel>(9);
  CHECK(!IsValidOrder(o));
}

TEST_CASE("All six RGB orders and all twenty-four RGBW orders encode and "
          "decode back") {
  const std::array<Colour, 4> px = {Colour{10, 20, 30, 40}, Colour{255, 0, 128, 7},
                                    Colour{0, 0, 0, 0}, Colour{1, 254, 3, 252}};
  std::array<Channel, 4> p{Channel::R, Channel::G, Channel::B, Channel::W};
  int32_t rgb_orders = 0;
  int32_t rgbw_orders = 0;
  std::array<Channel, 4> perm = p;
  do {
    ColourOrder four{perm, 4};
    std::vector<uint8_t> wire(16);
    CHECK_EQ(EncodeFrame(px, four, 255, Gamma::None, wire), 16U);
    std::array<Colour, 4> back{};
    CHECK_EQ(DecodeWire(wire, four, back), 4U);
    CHECK(back == px);
    ++rgbw_orders;
    if (perm[3] == Channel::W) { // the white channel last: the first three are
                                 // a permutation of R, G, B
      ColourOrder three{perm, 3};
      std::vector<uint8_t> w3(12);
      CHECK_EQ(EncodeFrame(px, three, 255, Gamma::None, w3), 12U);
      std::array<Colour, 4> b3{};
      CHECK_EQ(DecodeWire(w3, three, b3), 4U);
      for (size_t i = 0; i < 4; ++i) {
        CHECK(b3[i] == (Colour{px[i].r, px[i].g, px[i].b, 0}));
      }
      ++rgb_orders;
    }
  } while (std::next_permutation(perm.begin(), perm.end(), [](Channel a, Channel b) {
    return static_cast<uint8_t>(a) < static_cast<uint8_t>(b);
  }));
  CHECK_EQ(rgb_orders, 6);
  CHECK_EQ(rgbw_orders, 24);
}

TEST_CASE("Wire length is pixels times channels; DecodeWire stops at the "
          "shorter side") {
  const std::array<Colour, 3> px = {Colour{1, 2, 3, 4}, Colour{5, 6, 7, 8}, Colour{9, 10, 11, 12}};
  std::array<uint8_t, 12> wire{};
  CHECK_EQ(EncodeFrame(px, kOrderGrbw, 255, Gamma::None, wire), 12U);
  std::array<Colour, 2> two{};
  CHECK_EQ(DecodeWire(wire, kOrderGrbw, two), 2U);
  CHECK(two[1] == px[1]);
  std::array<Colour, 3> three{};
  CHECK_EQ(DecodeWire(std::span<const uint8_t>(wire).first(7), kOrderGrbw, three),
           1U); // one complete pixel
}

TEST_CASE("Colour helpers pack and unpack") {
  const Colour c = Colour::FromPacked(0xAB'CD'EF);
  CHECK(c == (Colour{0xAB, 0xCD, 0xEF, 0}));
  CHECK_EQ(c.Packed(), 0xABCDEFU);
  const Colour w = Colour::FromPackedW(0x12'34'56'78);
  CHECK(w == (Colour{0x34, 0x56, 0x78, 0x12}));
  CHECK_EQ(Colour::FromPacked(0xFF'00'00'01).Packed(), 1U);
}
