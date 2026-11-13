/**
 * @file ws2812_rmt_basic_example.cpp
 * @brief Start the strip from the menuconfig values, show red, then a rainbow that moves one step
 * every 20 ms (a full turn in about 5 s).
 * @copyright Copyright (c) 2026 The driver-ws2812-rmt authors. SPDX-License-Identifier: MIT
 */
#include <cstdint>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp32_ws2812_rmt_port.hpp"
#include "ws2812_rmt.hpp"
#include "ws2812_rmt_examples.hpp"
#include "ws2812_rmt_kconfig.hpp"

namespace {
constexpr const char* kTag = "ws2812_rmt_example";
} // namespace

namespace ws2812_rmt_basic_example {

ws2812_rmt::Config MakeConfig() noexcept {
  return ws2812_rmt::KconfigDefaults();
}

bool Start(Strip& strip) noexcept {
  ESP_LOGI(kTag, "WS2812_RMT driver %s", ws2812_rmt::GetDriverVersion());
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
  strip.Fill(ws2812_rmt::Colour{255, 0, 0, 0});
  if (const auto shown = strip.Show(); !shown) {
    ESP_LOGE(kTag, "Show failed: %s", ws2812_rmt::ToString(shown.error));
    return false;
  }
  return true;
}

} // namespace ws2812_rmt_basic_example

extern "C" void app_main(void) {
  using ws2812_rmt_basic_example::kPixels;
  static Esp32Ws2812RmtPort port;
  static ws2812_rmt_basic_example::Strip strip(port, ws2812_rmt_basic_example::MakeConfig());

  if (!ws2812_rmt_basic_example::Start(strip)) {
    return;
  }
  vTaskDelay(pdMS_TO_TICKS(1000));
  for (uint32_t step = 0;; ++step) {
    for (size_t i = 0; i < kPixels; ++i) {
      const auto position = static_cast<uint8_t>(((i * 256U) / kPixels) + step);
      if (const auto set = strip.SetPixel(i, ws2812_rmt::Wheel(position)); !set) {
        ESP_LOGE(kTag, "SetPixel(%u) failed: %s", static_cast<unsigned>(i),
                 ws2812_rmt::ToString(set.error));
      }
    }
    if (const auto shown = strip.Show(); !shown) {
      ESP_LOGW(kTag, "Show failed: %s", ws2812_rmt::ToString(shown.error));
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}
