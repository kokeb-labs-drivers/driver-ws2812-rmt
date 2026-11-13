/**
 * @file test_kconfig.cpp
 * @brief KconfigDefaults(): Config::Default() without Kconfig, the menuconfig
 * values with it (KCONFIG_SET).
 */
#include "test_harness.hpp"
#include "ws2812_rmt_functions.hpp"
#include "ws2812_rmt_kconfig.hpp"

#if defined(KCONFIG_SET)
TEST_CASE("Kconfig values reach the Config") {
  constexpr ws2812_rmt::Config kCfg = ws2812_rmt::KconfigDefaults();
  CHECK(kCfg.variant == ws2812_rmt::Variant::Sk6812Rgbw);
  CHECK_EQ(kCfg.brightness, 100U);
  CHECK_EQ(kCfg.reset_us, 350U);
  CHECK_EQ(kCfg.power_on_settle_us, 2000U);
  static_assert(ws2812_rmt::Validate(kCfg, 8).ok(),
                "the menuconfig values form a valid configuration");
}
#else
TEST_CASE("Without Kconfig, KconfigDefaults() equals Config::Default()") {
  constexpr ws2812_rmt::Config kCfg = ws2812_rmt::KconfigDefaults();
  constexpr ws2812_rmt::Config kDef = ws2812_rmt::Config::Default();
  CHECK(kCfg.variant == kDef.variant);
  CHECK_EQ(kCfg.brightness, kDef.brightness);
  CHECK_EQ(kCfg.reset_us, kDef.reset_us);
  CHECK_EQ(kCfg.power_on_settle_us, kDef.power_on_settle_us);
  CHECK(ws2812_rmt::KconfigVariant() == ws2812_rmt::Variant::Ws2812bV5);
}
#endif
