/**
 * @file ws2812_rmt_custom_example.cpp
 * @brief A part with its own timing: `Variant::Custom`, a declared window set and an RGBW colour
 * order.
 * @copyright Copyright (c) 2026 The driver-ws2812-rmt authors. SPDX-License-Identifier: MIT
 *
 * The windows below are placeholders: copy the numbers of your part's datasheet, with the section.
 * `EnsureInitialized()` refuses a set the port's tick rate cannot meet (`TimingUnreachable`).
 */
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp32_ws2812_rmt_port.hpp"
#include "ws2812_rmt.hpp"
#include "ws2812_rmt_examples.hpp"

namespace {
constexpr const char* kTag = "ws2812_rmt_custom";
} // namespace

namespace ws2812_rmt_custom_example {

ws2812_rmt::Config MakeConfig() noexcept {
  ws2812_rmt::Config config = ws2812_rmt::Config::Default();
  config.variant = ws2812_rmt::Variant::Custom;
  config.custom =
      ws2812_rmt::ProfileTiming(ws2812_rmt::Variant::Sk6812Rgbw); // replace with your datasheet's
  config.use_default_order = false;
  config.order = ws2812_rmt::kOrderGrbw;
  config.pixel_count = kPixels;
  return config;
}

bool Start(Strip& strip) noexcept {
  if (const auto init = strip.EnsureInitialized(); !init) {
    ESP_LOGE(kTag, "EnsureInitialized failed: %s", ws2812_rmt::ToString(init.error));
    return false;
  }
  // The port has a GpioSet() hook, so the strip starts unpowered and Show() needs PowerOn() first.
  // With no power pin (pins.power = -1) the port returns true: the strip is always on.
  if (const auto power = strip.PowerOn(); !power) {
    ESP_LOGE(kTag, "PowerOn failed: %s", ws2812_rmt::ToString(power.error));
    return false;
  }
  const auto wave = strip.CurrentWaveform();
  if (wave) {
    ESP_LOGI(
        kTag, "t0h %u, t0l %u, t1h %u, t1l %u ticks at %u Hz",
        static_cast<unsigned>(wave.value.t0h_ticks), static_cast<unsigned>(wave.value.t0l_ticks),
        static_cast<unsigned>(wave.value.t1h_ticks), static_cast<unsigned>(wave.value.t1l_ticks),
        static_cast<unsigned>(wave.value.resolution_hz));
  }
  strip.Fill(ws2812_rmt::ExtractWhite(ws2812_rmt::Colour{200, 100, 50, 0}));
  if (const auto shown = strip.Show(); !shown) {
    ESP_LOGE(kTag, "Show failed: %s", ws2812_rmt::ToString(shown.error));
    return false;
  }
  return true;
}

} // namespace ws2812_rmt_custom_example

extern "C" void app_main(void) {
  static Esp32Ws2812RmtPort port;
  static ws2812_rmt_custom_example::Strip strip(port, ws2812_rmt_custom_example::MakeConfig());

  if (!ws2812_rmt_custom_example::Start(strip)) {
    return;
  }
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    if (const auto shown = strip.Show(); !shown) {
      ESP_LOGW(kTag, "Show failed: %s", ws2812_rmt::ToString(shown.error));
    }
  }
}
