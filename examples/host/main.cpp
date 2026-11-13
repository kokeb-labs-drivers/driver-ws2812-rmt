/**
 * @file main.cpp
 * @brief The WS2812_RMT driver on your PC: drive a simulated strip and read back what its pixels
 * latch.
 *
 * No hardware and no SDK: the host port (host_ws2812_rmt_port.hpp) connects the real driver to the
 * model the host tests use, and a chain of simulated pixels decodes what the port sent. The same
 * calls run on an MCU with that MCU's port (examples/esp32/ for ESP-IDF). The animation is ordinary
 * application code over `Wheel()` and `SetPixel()`; the driver has no effects. Exit code 0 when
 * every call succeeded.
 */
#include <array>
#include <cinttypes>
#include <cstdint>
#include <cstdio>

#include "host_ws2812_rmt_port.hpp"
#include "ws2812_rmt.hpp"

namespace {

using Strip = ws2812_rmt::Ws2812Rmt<HostWs2812RmtPort, 8>;

void PrintLatched(const char* what, const ws2812_rmt_test::PixelChainModel& chain, size_t count) {
  static_cast<void>(std::printf("%s:", what));
  for (size_t i = 0; i < count; ++i) {
    const auto& c = chain.Latched()[i];
    static_cast<void>(std::printf(" %02X%02X%02X", c.r, c.g, c.b));
  }
  static_cast<void>(std::printf("\n"));
}

} // namespace

int main() {
  HostWs2812RmtPort port;
  Strip strip(port); // WS2812B V5 or later, 8 pixels, GRB: no I/O yet

  static_cast<void>(
      std::printf("WS2812_RMT driver %s on the host\n", ws2812_rmt::GetDriverVersion()));
  if (const auto r = strip.EnsureInitialized(); !r) {
    static_cast<void>(std::printf("EnsureInitialized failed: %s\n", ws2812_rmt::ToString(r.error)));
    return 1;
  }
  if (const auto r = strip.PowerOn(); !r) { // this port has a supply pin, which starts off
    static_cast<void>(std::printf("PowerOn failed: %s\n", ws2812_rmt::ToString(r.error)));
    return 1;
  }
  const auto frame = strip.FrameTimeUs();
  static_cast<void>(std::printf("frame time %u us for %u pixels\n",
                                static_cast<unsigned>(frame.value),
                                static_cast<unsigned>(strip.Length())));

  // 1. One pixel, then the frame.
  if (!strip.SetPixel(0, ws2812_rmt::Colour::FromPacked(0x123456)).ok() || !strip.Show().ok()) {
    return 1;
  }
  ws2812_rmt_test::PixelChainModel first(ws2812_rmt_test::DocumentById("b-v5"),
                                         8); // the V5 datasheet's windows
  first.Consume(port.symbols, port.EffectiveHz());
  PrintLatched("one pixel", first, 2);
  static_cast<void>(std::printf("wire bytes: %02X %02X %02X\n", port.last_wire[0],
                                port.last_wire[1], port.last_wire[2]));

  // 2. A rainbow, written in user code.
  port.ClearSymbols();
  for (size_t i = 0; i < strip.Length(); ++i) {
    const auto position = static_cast<uint8_t>((i * 256U) / strip.Length());
    if (!strip.SetPixel(i, ws2812_rmt::Wheel(position)).ok()) {
      return 1;
    }
  }
  strip.SetBrightness(64);
  if (!strip.Show().ok()) {
    return 1;
  }
  ws2812_rmt_test::PixelChainModel rainbow(ws2812_rmt_test::DocumentById("b-v5"), 8);
  rainbow.Consume(port.symbols, port.EffectiveHz());
  PrintLatched("rainbow at brightness 64", rainbow, 8);
  static_cast<void>(std::printf("violations %" PRId32 ", gaps %" PRId32 "\n", rainbow.violations,
                                rainbow.gaps_too_long));

  // 3. An RGBW strip shows an RGB colour with the white die.
  HostWs2812RmtPort rgbw_port;
  ws2812_rmt::Config config = ws2812_rmt::Config::Default();
  config.variant = ws2812_rmt::Variant::Sk6812Rgbw;
  config.pixel_count = 2;
  Strip rgbw(rgbw_port, config);
  if (!rgbw.EnsureInitialized().ok() || !rgbw.PowerOn().ok()) {
    return 1;
  }
  rgbw.Fill(ws2812_rmt::ExtractWhite(ws2812_rmt::Colour{.r = 200, .g = 100, .b = 50, .w = 0}));
  if (!rgbw.Show().ok()) {
    return 1;
  }
  static_cast<void>(std::printf("RGBW wire (G R B W): %02X %02X %02X %02X\n",
                                rgbw_port.last_wire[0], rgbw_port.last_wire[1],
                                rgbw_port.last_wire[2], rgbw_port.last_wire[3]));
  return 0;
}
