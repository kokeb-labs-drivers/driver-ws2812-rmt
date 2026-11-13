/**
 * @file FreeRTOS.h
 * @brief Host stand-in for the FreeRTOS types the examples use: a tick of 10 ms (100 Hz, the
 * ESP-IDF default).
 */
#ifndef WS2812_RMT_STUB_FREERTOS_H
#define WS2812_RMT_STUB_FREERTOS_H

#include <cstdint>

using TickType_t = uint32_t;
inline constexpr TickType_t portTICK_PERIOD_MS = 10U;

/// Milliseconds to ticks, rounded down as FreeRTOS does.
inline constexpr TickType_t pdMS_TO_TICKS(uint32_t ms) {
  return ms / portTICK_PERIOD_MS;
}

#endif // WS2812_RMT_STUB_FREERTOS_H
