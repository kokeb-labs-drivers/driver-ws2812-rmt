/**
 * @file quickstart.cpp
 * @brief The Quickstart page's example, compiled by the host build (never run).
 */
#include "my_port.hpp"

// --8<-- [start:quickstart]
#include "ws2812_rmt.hpp"

// 60 pixels of a current WS2812B (V5 or later): the default variant, GRB order.
int32_t light_first_pixel_red() {
  MyPort port;
  ws2812_rmt::Ws2812Rmt<MyPort, 60> strip(port); // owns the pixel and wire buffers; no I/O yet
  if (auto r = strip.EnsureInitialized(); !r) { // 1. channel up, waveform resolved at the real rate
    return static_cast<int32_t>(r.error); //    Bus, or TimingUnreachable for an unsuitable rate
  }
  strip.Clear();                                                          // 2. all pixels black
  static_cast<void>(strip.SetPixel(0, ws2812_rmt::Colour{255, 0, 0, 0})); //    pixel 0 is the first
  if (auto r = strip.Show(); !r) {        // 3. encode, send, wait for the reset
    return static_cast<int32_t>(r.error); //    Bus: the frame was not sent
  }
  return 0;
}
// --8<-- [end:quickstart]
