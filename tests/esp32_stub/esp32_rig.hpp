/**
 * @file esp32_rig.hpp
 * @brief Fresh stand-in state for one test case of the ESP32 port and examples.
 */
#ifndef WS2812_RMT_ESP32_RIG_HPP
#define WS2812_RMT_ESP32_RIG_HPP

#include "esp32_host_hooks.hpp"

namespace esp_stub {

/// Clears the recorded calls and the knobs when a test case starts and when it ends.
struct Rig {
  Rig() {
    Hooks() = State{};
  }
  Rig(const Rig&) = delete;
  Rig& operator=(const Rig&) = delete;
  Rig(Rig&&) = delete;
  Rig& operator=(Rig&&) = delete;
  ~Rig() {
    Hooks() = State{};
  }
};

} // namespace esp_stub

#endif // WS2812_RMT_ESP32_RIG_HPP
