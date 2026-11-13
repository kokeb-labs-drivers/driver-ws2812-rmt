/**
 * @file api_tasks.cpp
 * @brief The API guide's examples, compiled by the host build (never run).
 */
#include "my_port.hpp"
#include "ws2812_rmt.hpp"

// --8<-- [start:rgbw]
// An SK6812RGBW strip: four channels, white last. Rev 01 and Rev 03 datasheets send red first.
ws2812_rmt::Config RgbwConfig(bool old_revision) {
  ws2812_rmt::Config c = ws2812_rmt::Config::Default();
  c.variant = ws2812_rmt::Variant::Sk6812Rgbw;
  if (old_revision) {
    c.use_default_order = false;
    c.order = ws2812_rmt::kOrderRgbw; // red and green swapped on the strip means: use this
  }
  return c;
}
// --8<-- [end:rgbw]

// --8<-- [start:brightness]
using Strip = ws2812_rmt::Ws2812Rmt<MyPort, 30>;

void Dim(Strip& strip, bool night) {
  strip.SetBrightness(night ? 20 : 255); // linear; the stored colours are untouched
  strip.SetGamma(ws2812_rmt::Gamma::Gamma22);
}
// --8<-- [end:brightness]

// --8<-- [start:power]
// The strip supply on a switch: the port implements GpioSet(), the driver sequences it.
ws2812_rmt::Error ShowAfterPowerUp(Strip& strip) {
  if (auto r = strip.PowerOn(); !r) {
    return r.error; // Unsupported (no GpioSet hook), NotInitialized or Bus
  }
  return strip.Show().error;
}
// --8<-- [end:power]

// --8<-- [start:errors]
const char* Describe(ws2812_rmt::Error e) {
  switch (e) {
  case ws2812_rmt::Error::TimingUnreachable:
    return "the port's tick rate cannot place a window: use 20 MHz or more";
  case ws2812_rmt::Error::Bus:
    return "the port failed: channel, rate, send, timeout or underrun";
  default:
    return ws2812_rmt::ToString(e);
  }
}
// --8<-- [end:errors]
