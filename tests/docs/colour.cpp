/**
 * @file colour.cpp
 * @brief The Colour page's examples, compiled by the host build (never run).
 */
#include <array>
#include <cstdint>

#include "ws2812_rmt.hpp"

// --8<-- [start:encode]
// The pure encoder, without a strip: what the wire carries for one pixel.
bool EncodeOne() {
  std::array<uint8_t, 3> wire{};
  const ws2812_rmt::Colour c{0x12, 0x34, 0x56, 0};
  const size_t n =
      ws2812_rmt::EncodePixel(c, ws2812_rmt::kOrderGrb, 255, ws2812_rmt::Gamma::None, wire);
  return (n == 3U) && (wire[0] == 0x34U) && (wire[1] == 0x12U) && (wire[2] == 0x56U); // green first
}
// --8<-- [end:encode]

// --8<-- [start:white]
// RGB colour on an RGBW strip with the white die: the common part of R, G and B moves to W.
ws2812_rmt::Colour Warm() {
  return ws2812_rmt::ExtractWhite(ws2812_rmt::Colour{200, 100, 50, 0}); // {150, 50, 0, 50}
}
// --8<-- [end:white]

// --8<-- [start:wheel]
// An animation is ordinary code: the driver has Wheel() and SetPixel(), not effects.
template <typename Strip>
void Rainbow(Strip& strip, uint8_t phase) {
  for (size_t i = 0; i < strip.Length(); ++i) {
    const auto position = static_cast<uint8_t>(phase + ((i * 256U) / strip.Length()));
    static_cast<void>(strip.SetPixel(i, ws2812_rmt::Wheel(position)));
  }
}
// --8<-- [end:wheel]
