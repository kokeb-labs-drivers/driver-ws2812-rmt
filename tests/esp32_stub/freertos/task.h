/**
 * @file task.h
 * @brief Host stand-in for `vTaskDelay()`: the calls are counted.
 */
#ifndef WS2812_RMT_STUB_FREERTOS_TASK_H
#define WS2812_RMT_STUB_FREERTOS_TASK_H

#include "esp32_host_hooks.hpp"
#include "freertos/FreeRTOS.h"

inline void vTaskDelay(TickType_t ticks) {
  static_cast<void>(ticks);
  esp_stub::Hooks().task_delays += 1U;
}

#endif // WS2812_RMT_STUB_FREERTOS_TASK_H
