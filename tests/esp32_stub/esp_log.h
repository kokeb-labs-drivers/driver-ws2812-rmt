/**
 * @file esp_log.h
 * @brief Host stand-in for the ESP log: the format strings are kept for the test, the arguments are
 * not formatted.
 */
#ifndef WS2812_RMT_STUB_ESP_LOG_H
#define WS2812_RMT_STUB_ESP_LOG_H

#include "esp32_host_hooks.hpp"

enum esp_log_level_t : int { ESP_LOG_ERROR, ESP_LOG_WARN, ESP_LOG_INFO, ESP_LOG_DEBUG };

namespace esp_stub {
/// Keeps a message: error and warning lines in `errors`, the rest in `infos`.
inline void Log(esp_log_level_t level, const char* tag, const char* text) {
  static_cast<void>(tag);
  if ((level == ESP_LOG_ERROR) || (level == ESP_LOG_WARN)) {
    Hooks().errors.emplace_back(text);
  } else {
    Hooks().infos.emplace_back(text);
  }
}
} // namespace esp_stub

/// The port's plain-text log line (`esp_log_write(level, tag, "%s\n", message)`).
inline void esp_log_write(esp_log_level_t level, const char* tag, const char* format,
                          const char* message) {
  static_cast<void>(format);
  esp_stub::Log(level, tag, message);
}

#define ESP_LOGE(tag, format, ...) esp_stub::Log(ESP_LOG_ERROR, (tag), (format))
#define ESP_LOGW(tag, format, ...) esp_stub::Log(ESP_LOG_WARN, (tag), (format))
#define ESP_LOGI(tag, format, ...) esp_stub::Log(ESP_LOG_INFO, (tag), (format))

#endif // WS2812_RMT_STUB_ESP_LOG_H
