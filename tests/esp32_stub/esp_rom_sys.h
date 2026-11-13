/**
 * @file esp_rom_sys.h
 * @brief Host stand-in for the ROM busy wait: every wait is recorded.
 */
#ifndef WS2812_RMT_STUB_ESP_ROM_SYS_H
#define WS2812_RMT_STUB_ESP_ROM_SYS_H

#include <cstdint>

#include "esp32_host_hooks.hpp"

inline void esp_rom_delay_us(uint32_t us) {
  esp_stub::Hooks().delays_us.push_back(us);
  esp_stub::Hooks().calls.emplace_back("esp_rom_delay_us");
}

#endif // WS2812_RMT_STUB_ESP_ROM_SYS_H
