/**
 * @file esp_clk_tree.h
 * @brief Host stand-in for the clock tree query: the source clock is a knob of the test.
 */
#ifndef WS2812_RMT_STUB_ESP_CLK_TREE_H
#define WS2812_RMT_STUB_ESP_CLK_TREE_H

#include <cstdint>

#include "esp32_host_hooks.hpp"
#include "esp_err.h"

enum soc_module_clk_t : int { SOC_MOD_CLK_APB = 1, SOC_MOD_CLK_XTAL = 2 };
enum esp_clk_tree_src_freq_precision_t : int { ESP_CLK_TREE_SRC_FREQ_PRECISION_CACHED = 0 };

inline esp_err_t esp_clk_tree_src_get_freq_hz(soc_module_clk_t clk,
                                              esp_clk_tree_src_freq_precision_t precision,
                                              uint32_t* freq_hz) {
  static_cast<void>(clk);
  static_cast<void>(precision);
  *freq_hz = esp_stub::Hooks().source_hz;
  return ESP_OK;
}

#endif // WS2812_RMT_STUB_ESP_CLK_TREE_H
