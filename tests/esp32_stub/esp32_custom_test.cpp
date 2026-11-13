/**
 * @file esp32_custom_test.cpp
 * @brief The custom example's own start-up on the host, over stand-ins of the ESP-IDF headers: with
 * the default pins it powers the strip and shows its first frame (RD-21).
 */
#include <cstdint>

#include "esp32_host_hooks.hpp"
#include "esp32_rig.hpp"
#include "esp32_ws2812_rmt_port.hpp"
#include "test_harness.hpp"
#include "ws2812_rmt_examples.hpp"

TEST_CASE("custom example: with the default pins its start-up powers the strip and shows a frame") {
  const esp_stub::Rig rig;
  Esp32Ws2812RmtPort port;
  ws2812_rmt_custom_example::Strip strip(port, ws2812_rmt_custom_example::MakeConfig());
  CHECK(ws2812_rmt_custom_example::Start(strip));
  const esp_stub::State& s = esp_stub::Hooks();
  CHECK(s.errors.empty());
  REQUIRE(s.frames.size() == 1U);
  CHECK_EQ(s.frames[0].size(), ws2812_rmt_custom_example::kPixels * 4U); // G R B W
  const auto wave = strip.CurrentWaveform();
  REQUIRE(wave.ok());
  CHECK(ws2812_rmt::WaveformInWindow(ws2812_rmt::Variant::Sk6812Rgbw, wave.value));
  REQUIRE(s.encoders.size() == 1U);
  CHECK_EQ(s.encoders[0].t0h, wave.value.t0h_ticks);
}

TEST_CASE("custom example: without PowerOn() a strip of this port would refuse every frame") {
  const esp_stub::Rig rig;
  Esp32Ws2812RmtPort port;
  ws2812_rmt_custom_example::Strip strip(port, ws2812_rmt_custom_example::MakeConfig());
  REQUIRE(strip.EnsureInitialized().ok());
  CHECK_EQ(strip.Show().error, ws2812_rmt::Error::NotPowered); // why Start() powers it first
  CHECK(esp_stub::Hooks().frames.empty());
}
