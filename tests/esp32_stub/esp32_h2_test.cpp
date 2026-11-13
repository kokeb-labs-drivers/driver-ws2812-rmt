/**
 * @file esp32_h2_test.cpp
 * @brief The ESP-IDF port built for the ESP32-H2 (`CONFIG_IDF_TARGET_ESP32H2`), whose RMT runs from
 * the 32 MHz crystal: the default rate is 32 MHz and the basic example starts.
 */
#include <cstdint>

#include "esp32_host_hooks.hpp"
#include "esp32_rig.hpp"
#include "esp32_ws2812_rmt_port.hpp"
#include "test_harness.hpp"
#include "ws2812_rmt_examples.hpp"

TEST_CASE("esp32-h2: the default rate is the crystal's 32 MHz and the basic example starts") {
  const esp_stub::Rig rig;
  esp_stub::Hooks().source_hz = 32'000'000U;
  CHECK_EQ(Esp32Ws2812RmtPins{}.resolution_hz, 32'000'000U);
  Esp32Ws2812RmtPort port;
  ws2812_rmt_basic_example::Strip strip(port, ws2812_rmt_basic_example::MakeConfig());
  CHECK(ws2812_rmt_basic_example::Start(strip));
  CHECK(esp_stub::Hooks().errors.empty());
  CHECK_EQ(esp_stub::Hooks().frames.size(), 1U);
  const auto wave = strip.CurrentWaveform();
  REQUIRE(wave.ok());
  CHECK_EQ(wave.value.resolution_hz, 32'000'000U);
  CHECK(ws2812_rmt::WaveformInWindow(ws2812_rmt::Variant::Ws2812bV5, wave.value));
}
