/**
 * @file configuration.cpp
 * @brief The Configuration page's examples, compiled by the host build (never run).
 */
#include "my_port.hpp"
#include "ws2812_rmt.hpp"
#include "ws2812_rmt_kconfig.hpp"

// --8<-- [start:kconfig]
ws2812_rmt::Error StartFromMenuconfig(MyPort& port) {
  static ws2812_rmt::Ws2812Rmt<MyPort, 60> strip(port, ws2812_rmt::KconfigDefaults());
  return strip.EnsureInitialized().error;
}
// --8<-- [end:kconfig]

// --8<-- [start:custom]
// A part whose datasheet is not among the named variants: state its timing and its order yourself.
constexpr ws2812_rmt::Config MyLedConfig() {
  ws2812_rmt::Config c = ws2812_rmt::Config::Default();
  c.variant = ws2812_rmt::Variant::Custom;
  c.custom.t0h = {250, 450, 350}; // min, max, nominal in nanoseconds
  c.custom.t0l = {700, 1200, 900};
  c.custom.t1h = {700, 1100, 850};
  c.custom.t1l = {300, 700, 450};
  c.custom.reset_min_ns = 60'000; // the low time that must be exceeded to latch
  c.use_default_order = false;    // a Custom part always states its order
  c.order = ws2812_rmt::kOrderGrb;
  return c;
}

static_assert(ws2812_rmt::Validate(MyLedConfig(), 30).ok()); // proved at build time
// --8<-- [end:custom]
