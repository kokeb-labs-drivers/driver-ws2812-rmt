/**
 * @file esp_err.h
 * @brief Host stand-in for the ESP-IDF error type.
 */
#ifndef WS2812_RMT_STUB_ESP_ERR_H
#define WS2812_RMT_STUB_ESP_ERR_H

using esp_err_t = int;
inline constexpr esp_err_t ESP_OK = 0;
inline constexpr esp_err_t ESP_FAIL = -1;

#endif // WS2812_RMT_STUB_ESP_ERR_H
