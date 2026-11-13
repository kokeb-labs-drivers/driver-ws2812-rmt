/**
 * @file rmt_tx.h
 * @brief Host stand-in for the RMT transmit channel: the configuration, every payload and every
 * wait are recorded; a transmission fails when the test says so.
 */
#ifndef WS2812_RMT_STUB_DRIVER_RMT_TX_H
#define WS2812_RMT_STUB_DRIVER_RMT_TX_H

#include <cstddef>
#include <cstdint>
#include <span>

#include "driver/gpio.h"
#include "driver/rmt_encoder.h"
#include "esp32_host_hooks.hpp"
#include "esp_clk_tree.h"
#include "esp_err.h"

enum rmt_clock_source_t : int { RMT_CLK_SRC_DEFAULT = SOC_MOD_CLK_APB };

/// The stand-in channel object; the test creates at most one.
struct rmt_channel_t {
  bool enabled;
  uint32_t resolution_hz; ///< The rate the channel got.
};
using rmt_channel_handle_t = rmt_channel_t*;

struct rmt_tx_channel_config_t {
  gpio_num_t gpio_num;
  rmt_clock_source_t clk_src;
  uint32_t resolution_hz;
  size_t mem_block_symbols;
  size_t trans_queue_depth;
  int intr_priority;
  struct {
    uint32_t invert_out;
    uint32_t with_dma;
    uint32_t io_loop_back;
    uint32_t io_od_mode;
    uint32_t allow_pd;
    uint32_t init_level;
  } flags;
};

struct rmt_transmit_config_t {
  int loop_count;
  struct {
    uint32_t eot_level;
    uint32_t queue_nonblocking;
  } flags;
};

inline esp_err_t rmt_new_tx_channel(const rmt_tx_channel_config_t* config,
                                    rmt_channel_handle_t* ret_chan) {
  esp_stub::State& s = esp_stub::Hooks();
  s.channel_gpio = static_cast<int>(config->gpio_num);
  s.channel_resolution = config->resolution_hz;
  s.channel_symbols = config->mem_block_symbols;
  s.channel_init_level = config->flags.init_level;
  s.calls.emplace_back("rmt_new_tx_channel");
  static rmt_channel_t channel{};
  uint32_t real_hz = s.channel_real_hz;
  if ((real_hz == 0U) && (config->resolution_hz != 0U)) {
    const uint64_t divider =
        (static_cast<uint64_t>(s.source_hz) + (config->resolution_hz / 2U)) / config->resolution_hz;
    real_hz = (divider == 0U) ? 0U : static_cast<uint32_t>(s.source_hz / divider);
  }
  channel = rmt_channel_t{false, real_hz};
  *ret_chan = &channel;
  return ESP_OK;
}

inline esp_err_t rmt_del_channel(rmt_channel_handle_t channel) {
  static_cast<void>(channel);
  esp_stub::Hooks().channels_deleted += 1U;
  esp_stub::Hooks().calls.emplace_back("rmt_del_channel");
  return ESP_OK;
}

inline esp_err_t rmt_enable(rmt_channel_handle_t channel) {
  channel->enabled = true;
  esp_stub::Hooks().enables += 1U;
  return ESP_OK;
}

inline esp_err_t rmt_disable(rmt_channel_handle_t channel) {
  channel->enabled = false;
  esp_stub::Hooks().disables += 1U;
  return ESP_OK;
}

inline esp_err_t rmt_transmit(rmt_channel_handle_t channel, rmt_encoder_handle_t encoder,
                              const void* payload, size_t payload_bytes,
                              const rmt_transmit_config_t* config) {
  esp_stub::State& s = esp_stub::Hooks();
  if (!channel->enabled || (encoder == nullptr) || (payload == nullptr) || (payload_bytes == 0U) ||
      s.fail_transmit) {
    return ESP_FAIL;
  }
  s.eot_level = config->flags.eot_level;
  const std::span<const uint8_t> bytes(static_cast<const uint8_t*>(payload), payload_bytes);
  s.frames.emplace_back(bytes.begin(), bytes.end());
  s.calls.emplace_back("rmt_transmit");
  return ESP_OK;
}

inline esp_err_t rmt_tx_wait_all_done(rmt_channel_handle_t channel, int timeout_ms) {
  static_cast<void>(channel);
  esp_stub::Hooks().wait_ms.push_back(timeout_ms);
  esp_stub::Hooks().calls.emplace_back("rmt_tx_wait_all_done");
  return ESP_OK;
}

#endif // WS2812_RMT_STUB_DRIVER_RMT_TX_H
