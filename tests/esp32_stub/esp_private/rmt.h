/**
 * @file rmt.h
 * @brief Host stand-in for ESP-IDF's internal RMT query: the rate the channel got
 * (`esp32_host_hooks.hpp`, `channel_real_hz`).
 */
#ifndef WS2812_RMT_STUB_ESP_PRIVATE_RMT_H
#define WS2812_RMT_STUB_ESP_PRIVATE_RMT_H

#include <cstdint>

#include "driver/rmt_tx.h"
#include "esp32_host_hooks.hpp"
#include "esp_err.h"

inline esp_err_t rmt_get_channel_resolution(rmt_channel_handle_t channel,
                                            uint32_t* ret_resolution_hz) {
  if ((channel == nullptr) || (ret_resolution_hz == nullptr)) {
    return ESP_FAIL;
  }
  *ret_resolution_hz = channel->resolution_hz;
  esp_stub::Hooks().calls.emplace_back("rmt_get_channel_resolution");
  return ESP_OK;
}

#endif // WS2812_RMT_STUB_ESP_PRIVATE_RMT_H
