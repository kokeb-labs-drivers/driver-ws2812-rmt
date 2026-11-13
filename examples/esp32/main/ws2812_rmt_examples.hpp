/**
 * @file ws2812_rmt_examples.hpp
 * @brief The strips of the ESP32 examples and their start-up sequences, declared once so that the
 * host tests run the same start-up as `app_main()` (tests/esp32_stub/esp32_basic_test.cpp,
 * tests/esp32_stub/esp32_custom_test.cpp, tests/esp32_stub/esp32_h2_test.cpp).
 * @copyright Copyright (c) 2026 The driver-ws2812-rmt authors. SPDX-License-Identifier: MIT
 */
#ifndef WS2812_RMT_ESP32_EXAMPLES_HPP
#define WS2812_RMT_ESP32_EXAMPLES_HPP

#include <cstddef>

#include "esp32_ws2812_rmt_port.hpp"
#include "ws2812_rmt.hpp"

/// The basic example: the menuconfig values, red, then a rainbow.
namespace ws2812_rmt_basic_example {
inline constexpr size_t kPixels = 8;
using Strip = ws2812_rmt::Ws2812Rmt<Esp32Ws2812RmtPort, kPixels>;
/// The configuration: the menuconfig values (`ws2812_rmt::KconfigDefaults()`).
ws2812_rmt::Config MakeConfig() noexcept;
/// Starts the strip (`EnsureInitialized()`, `PowerOn()`) and shows the first frame, all red.
/// @return false, with the reason logged, when a step fails.
bool Start(Strip& strip) noexcept;
} // namespace ws2812_rmt_basic_example

/// The custom example: `Variant::Custom` with an RGBW order.
namespace ws2812_rmt_custom_example {
inline constexpr size_t kPixels = 4;
using Strip = ws2812_rmt::Ws2812Rmt<Esp32Ws2812RmtPort, kPixels>;
/// The configuration: a declared window set and the GRBW order.
ws2812_rmt::Config MakeConfig() noexcept;
/// Starts the strip (`EnsureInitialized()`, `PowerOn()`) and shows the first frame.
/// @return false, with the reason logged, when a step fails.
bool Start(Strip& strip) noexcept;
} // namespace ws2812_rmt_custom_example

#endif // WS2812_RMT_ESP32_EXAMPLES_HPP
