/**
 * @file ws2812_rmt_kconfig.hpp
 * @brief Optional ESP-IDF layer: a `Config` built from the `CONFIG_WS2812_RMT_*` Kconfig options.
 * @copyright Copyright (c) 2026 The driver-ws2812-rmt authors. SPDX-License-Identifier: MIT
 *
 * Include it only where menuconfig should choose the defaults; without Kconfig `KconfigDefaults()`
 * equals `Config::Default()`. One configuration per program: every translation unit of one program
 * must see the same `CONFIG_` values, and no driver header includes this one (handbook
 * KB-CPP20-018).
 */
#ifndef WS2812_RMT_KCONFIG_HPP
#define WS2812_RMT_KCONFIG_HPP

#ifdef __has_include
#if __has_include("sdkconfig.h")
#include "sdkconfig.h"
#endif
#endif

#include "ws2812_rmt_types.hpp"

#ifndef CONFIG_WS2812_RMT_DEFAULT_BRIGHTNESS
/** Brightness at start when Kconfig does not set it: `Config::Default().brightness`. */
#define CONFIG_WS2812_RMT_DEFAULT_BRIGHTNESS 255
#endif
#ifndef CONFIG_WS2812_RMT_RESET_US
/** Reset low time when Kconfig does not set it: 0, the datasheet minimum plus the margin. */
#define CONFIG_WS2812_RMT_RESET_US 0
#endif
#ifndef CONFIG_WS2812_RMT_POWER_ON_SETTLE_US
/** Wait after `PowerOn()` when Kconfig does not set it: `Config::Default().power_on_settle_us`. */
#define CONFIG_WS2812_RMT_POWER_ON_SETTLE_US 1000
#endif

namespace ws2812_rmt {

/**
 * @brief The variant menuconfig selected; `Variant::Ws2812bV5` without a selection.
 * @return The variant.
 */
constexpr Variant KconfigVariant() noexcept {
#if defined(CONFIG_WS2812_RMT_VARIANT_WS2812)
  return Variant::Ws2812;
#elif defined(CONFIG_WS2812_RMT_VARIANT_WS2812B_PRE_V5)
  return Variant::Ws2812bPreV5;
#elif defined(CONFIG_WS2812_RMT_VARIANT_WS2812C)
  return Variant::Ws2812c;
#elif defined(CONFIG_WS2812_RMT_VARIANT_WS2812D)
  return Variant::Ws2812d;
#elif defined(CONFIG_WS2812_RMT_VARIANT_SK6812_RGBW)
  return Variant::Sk6812Rgbw;
#elif defined(CONFIG_WS2812_RMT_VARIANT_SK6812_MINI)
  return Variant::Sk6812Mini;
#elif defined(CONFIG_WS2812_RMT_VARIANT_SK6812)
  return Variant::Sk6812;
#else
  return Variant::Ws2812bV5;
#endif
}

/**
 * @brief The configuration menuconfig selected, on top of the datasheet defaults.
 * @return `Config::Default()` with the variant, brightness, reset and settle from Kconfig.
 */
constexpr Config KconfigDefaults() noexcept {
  Config c = Config::Default();
  c.variant = KconfigVariant();
  c.brightness = static_cast<uint8_t>(CONFIG_WS2812_RMT_DEFAULT_BRIGHTNESS);
  c.reset_us = static_cast<uint32_t>(CONFIG_WS2812_RMT_RESET_US);
  c.power_on_settle_us = static_cast<uint32_t>(CONFIG_WS2812_RMT_POWER_ON_SETTLE_US);
  return c;
}

} // namespace ws2812_rmt

#endif // WS2812_RMT_KCONFIG_HPP
