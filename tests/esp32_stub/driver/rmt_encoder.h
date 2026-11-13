/**
 * @file rmt_encoder.h
 * @brief Host stand-in for the RMT bytes encoder: the durations of each encoder are recorded.
 */
#ifndef WS2812_RMT_STUB_DRIVER_RMT_ENCODER_H
#define WS2812_RMT_STUB_DRIVER_RMT_ENCODER_H

#include <array>
#include <cstdint>

#include "esp32_host_hooks.hpp"
#include "esp_err.h"

/// The stand-in encoder object.
struct rmt_encoder_t {
  esp_stub::EncoderDurations durations;
};
using rmt_encoder_handle_t = rmt_encoder_t*;

/// One RMT symbol: two levels with their durations (15-bit fields in ESP-IDF; plain fields here,
/// the port checks the 15-bit limit itself).
struct rmt_symbol_word_t {
  uint32_t duration0;
  uint32_t level0;
  uint32_t duration1;
  uint32_t level1;
};

struct rmt_bytes_encoder_config_t {
  rmt_symbol_word_t bit0;
  rmt_symbol_word_t bit1;
  struct {
    uint32_t msb_first;
  } flags;
};

inline esp_err_t rmt_new_bytes_encoder(const rmt_bytes_encoder_config_t* config,
                                       rmt_encoder_handle_t* ret_encoder) {
  const esp_stub::EncoderDurations d{config->bit0.duration0, config->bit0.duration1,
                                     config->bit1.duration0, config->bit1.duration1,
                                     config->flags.msb_first};
  esp_stub::State& s = esp_stub::Hooks();
  s.encoders.push_back(d);
  s.calls.emplace_back("rmt_new_bytes_encoder");
  // No heap: the port holds at most one encoder, so a fixed pool used in turn is enough.
  static std::array<rmt_encoder_t, 4> pool{};
  rmt_encoder_t& slot = pool[s.encoders.size() % pool.size()];
  slot.durations = d;
  *ret_encoder = &slot;
  return ESP_OK;
}

inline esp_err_t rmt_del_encoder(rmt_encoder_handle_t encoder) {
  return (encoder != nullptr) ? ESP_OK : ESP_FAIL;
}

#endif // WS2812_RMT_STUB_DRIVER_RMT_ENCODER_H
