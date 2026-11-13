/**
 * @file esp32_host_hooks.hpp
 * @brief What the host stand-ins of the ESP-IDF headers share: the knobs a test sets and the calls
 * they record.
 */
#ifndef WS2812_RMT_ESP32_HOST_HOOKS_HPP
#define WS2812_RMT_ESP32_HOST_HOOKS_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace esp_stub {

/// One `rmt_new_bytes_encoder()` call: the four durations of the two bit codes.
struct EncoderDurations {
  uint32_t t0h;
  uint32_t t0l;
  uint32_t t1h;
  uint32_t t1l;
  uint32_t msb_first; ///< `flags.msb_first`: 1 when the most significant bit goes first.
};

/// The shared state of the stand-ins.
struct State {
  // Knobs
  uint32_t source_hz{80'000'000}; ///< What `esp_clk_tree_src_get_freq_hz()` reports.
  bool fail_transmit{false};      ///< `rmt_transmit()` returns an error.
  /// What `rmt_get_channel_resolution()` reports; 0: the source divided by the rounded divider,
  /// as for a channel alone in its group. Another value models a group clock that a channel
  /// created earlier fixed.
  uint32_t channel_real_hz{0};
  // Records
  std::vector<std::pair<int, uint32_t>> gpio_levels; ///< `gpio_set_level(pin, level)` calls.
  uint64_t gpio_configured_mask{0};
  int channel_gpio{-2};           ///< The data pin of `rmt_new_tx_channel()`; -2 none created.
  uint32_t channel_resolution{0}; ///< The requested rate of `rmt_new_tx_channel()`.
  size_t channel_symbols{0};      ///< `mem_block_symbols` of `rmt_new_tx_channel()`.
  uint32_t channel_init_level{1}; ///< `flags.init_level` of `rmt_new_tx_channel()`.
  std::vector<EncoderDurations> encoders;   ///< Every `rmt_new_bytes_encoder()`.
  std::vector<std::vector<uint8_t>> frames; ///< Every payload `rmt_transmit()` accepted.
  std::vector<int> wait_ms;                 ///< Every `rmt_tx_wait_all_done()` timeout.
  std::vector<uint32_t> delays_us;          ///< Every `esp_rom_delay_us()`.
  uint32_t eot_level{1};                    ///< `flags.eot_level` of the last `rmt_transmit()`.
  uint32_t enables{0};                      ///< `rmt_enable()` calls.
  uint32_t disables{0};                     ///< `rmt_disable()` calls.
  std::vector<std::string> errors;          ///< Messages logged at error or warning level.
  std::vector<std::string> infos;           ///< Messages logged at info level.
  uint32_t task_delays{0};                  ///< `vTaskDelay()` calls.
  uint32_t channels_deleted{0};             ///< `rmt_del_channel()` calls.
  std::vector<std::string> calls; ///< Every pin, channel, encoder and wait call, in order.
};

/// The one instance.
inline State& Hooks() {
  static State state;
  return state;
}

} // namespace esp_stub

#endif // WS2812_RMT_ESP32_HOST_HOOKS_HPP
