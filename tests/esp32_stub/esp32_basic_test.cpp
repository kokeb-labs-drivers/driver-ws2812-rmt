/**
 * @file esp32_basic_test.cpp
 * @brief The ESP-IDF port and the basic example on the host, over stand-ins of the ESP-IDF headers:
 * the example's own start-up reaches a shown frame with the default pins (RD-21), and the port
 * configures the channel, waits and recovers as its comments say.
 */
#include <array>
#include <cstdint>

#include "esp32_host_hooks.hpp"
#include "esp32_rig.hpp"
#include "esp32_ws2812_rmt_port.hpp"
#include "test_harness.hpp"
#include "ws2812_rmt_examples.hpp"

namespace {

using ws2812_rmt_basic_example::Strip;

/// The timeout the port is handed for a frame of `bytes` bytes, plus the two RTOS ticks it adds.
int ExpectedWaitMs(const ws2812_rmt::Waveform& wave, size_t bytes, uint32_t slack_ms) {
  const uint64_t ns = ws2812_rmt::FrameTimeNs(bytes, uint8_t{1}, wave);
  return static_cast<int>(((ns + 999'999U) / 1'000'000U) + slack_ms +
                          (2U * static_cast<uint64_t>(portTICK_PERIOD_MS)));
}

} // namespace

TEST_CASE("basic example: with the default pins its start-up powers the strip and shows a frame") {
  const esp_stub::Rig rig;
  Esp32Ws2812RmtPort port; // power = -1, data = 8
  Strip strip(port, ws2812_rmt_basic_example::MakeConfig());
  CHECK(ws2812_rmt_basic_example::Start(strip));
  const esp_stub::State& s = esp_stub::Hooks();
  CHECK(s.errors.empty());
  REQUIRE(s.frames.size() == 1U);
  REQUIRE(s.frames[0].size() == ws2812_rmt_basic_example::kPixels * 3U);
  CHECK_EQ(s.frames[0][0], 0U); // G R B: red
  CHECK_EQ(s.frames[0][1], 255U);
  CHECK_EQ(s.frames[0][2], 0U);
  CHECK(s.gpio_levels.empty()); // no supply pin was driven
  CHECK_EQ(s.channel_gpio, 8);
  CHECK_EQ(s.channel_resolution, 20'000'000U);
  CHECK_EQ(s.channel_init_level, 0U); // the line idles low
  CHECK_EQ(s.eot_level, 0U);
  CHECK_EQ(s.channel_symbols, 96U); // two memory blocks of 48 words, no DMA
}

TEST_CASE("esp32 port: the encoder carries the waveform and the reset follows the frame") {
  const esp_stub::Rig rig;
  Esp32Ws2812RmtPort port;
  Strip strip(port, ws2812_rmt_basic_example::MakeConfig());
  REQUIRE(ws2812_rmt_basic_example::Start(strip));
  const auto wave = strip.CurrentWaveform();
  REQUIRE(wave.ok());
  const esp_stub::State& s = esp_stub::Hooks();
  REQUIRE(s.encoders.size() == 1U);
  CHECK_EQ(s.encoders[0].t0h, wave.value.t0h_ticks);
  CHECK_EQ(s.encoders[0].t0l, wave.value.t0l_ticks);
  CHECK_EQ(s.encoders[0].t1h, wave.value.t1h_ticks);
  CHECK_EQ(s.encoders[0].t1l, wave.value.t1l_ticks);
  CHECK_EQ(s.encoders[0].msb_first, 1U); // the most significant bit of each byte goes first
  // The reset: a busy wait strictly longer than reset_ticks (6000 ticks, 300 us at 20 MHz).
  REQUIRE(!s.delays_us.empty());
  CHECK_EQ(s.delays_us.back(), 301U);
  // The wait for the end of the frame is the driver's timeout and two RTOS ticks more.
  REQUIRE(s.wait_ms.size() == 1U);
  CHECK_EQ(s.wait_ms[0], ExpectedWaitMs(wave.value, s.frames[0].size(),
                                        ws2812_rmt_basic_example::MakeConfig().transmit_slack_ms));
  // The same waveform again reuses the encoder.
  REQUIRE(strip.Show().ok());
  CHECK_EQ(s.encoders.size(), 1U);
}

TEST_CASE("esp32 port: a failed transmission stops the channel; the next frame enables it again") {
  const esp_stub::Rig rig;
  Esp32Ws2812RmtPort port;
  Strip strip(port, ws2812_rmt_basic_example::MakeConfig());
  REQUIRE(ws2812_rmt_basic_example::Start(strip));
  esp_stub::State& s = esp_stub::Hooks();
  s.fail_transmit = true;
  CHECK_EQ(strip.Show().error, ws2812_rmt::Error::Bus);
  CHECK_EQ(s.disables, 1U);
  s.fail_transmit = false;
  const size_t waits = s.delays_us.size();
  REQUIRE(strip.Show().ok());
  CHECK_EQ(s.enables, 2U);
  // The driver's reset before the next frame, then the port's reset after it.
  REQUIRE(s.delays_us.size() == (waits + 2U));
  CHECK(s.delays_us[waits] > 300U);
}

TEST_CASE("esp32 port: a configured supply pin is set low before it is an output, then on") {
  const esp_stub::Rig rig;
  Esp32Ws2812RmtPins pins;
  pins.power = 5;
  Esp32Ws2812RmtPort port(pins);
  Strip strip(port, ws2812_rmt_basic_example::MakeConfig());
  REQUIRE(ws2812_rmt_basic_example::Start(strip));
  const esp_stub::State& s = esp_stub::Hooks();
  // The order: the level first, then the pin becomes an output, then the channel.
  REQUIRE(s.calls.size() >= 3U);
  CHECK(s.calls[0] == "gpio_set_level");
  CHECK(s.calls[1] == "gpio_config");
  CHECK(s.calls[2] == "rmt_new_tx_channel");
  REQUIRE(s.gpio_levels.size() == 2U);
  CHECK_EQ(s.gpio_levels[0].first, 5);
  CHECK_EQ(s.gpio_levels[0].second, 0U); // inactive before the output is enabled
  CHECK_EQ(s.gpio_levels[1].second, 1U);
  CHECK_EQ(s.gpio_configured_mask, uint64_t{1} << 5U);
  CHECK(strip.PowerOff().ok());
  CHECK_EQ(s.gpio_levels.back().second, 0U);
}

TEST_CASE("esp32 port: GpioSet with no supply pin succeeds and drives nothing") {
  const esp_stub::Rig rig;
  Esp32Ws2812RmtPort port;
  CHECK(port.GpioSet(ws2812_rmt::CtrlPin::Power, ws2812_rmt::GpioSignal::ACTIVE));
  CHECK(port.GpioSet(ws2812_rmt::CtrlPin::Power, ws2812_rmt::GpioSignal::INACTIVE));
  CHECK(esp_stub::Hooks().gpio_levels.empty());
}

TEST_CASE(
    "esp32 port: the reported rate is the rounded divider's, and a far one refuses to start") {
  const esp_stub::Rig rig;
  esp_stub::Hooks().source_hz = 79'999'000U; // divider 4: 19'999'750 Hz, 12.5 ppm low
  Esp32Ws2812RmtPort port;
  REQUIRE(port.EnsureInitialized());
  uint32_t hz = 0U;
  REQUIRE(port.ResolutionHz(hz));
  CHECK_EQ(hz, 19'999'750U);
  esp_stub::Hooks().source_hz = 32'000'000U; // an ESP32-H2 crystal with the 20 MHz default
  Esp32Ws2812RmtPort far;
  CHECK(!far.EnsureInitialized());
  Strip strip(far, ws2812_rmt_basic_example::MakeConfig());
  CHECK_EQ(strip.EnsureInitialized().error, ws2812_rmt::Error::Bus);
}

TEST_CASE("esp32 port: a channel at another rate than computed (shared group clock) refuses to "
          "start") {
  const esp_stub::Rig rig;
  // Another channel of the group fixed the group clock first: this channel gets 16 MHz, not 20.
  esp_stub::Hooks().channel_real_hz = 16'000'000U;
  Esp32Ws2812RmtPort port;
  Strip strip(port, ws2812_rmt_basic_example::MakeConfig());
  CHECK_EQ(strip.EnsureInitialized().error, ws2812_rmt::Error::Bus);
  const esp_stub::State& s = esp_stub::Hooks();
  CHECK_EQ(s.channels_deleted, 1U);
  CHECK(!s.errors.empty());
  CHECK(s.frames.empty());
  // The rate the channel really got is what the port reports when it matches.
  esp_stub::Hooks().channel_real_hz = 20'000'000U;
  Esp32Ws2812RmtPort same;
  REQUIRE(same.EnsureInitialized());
  uint32_t hz = 0U;
  REQUIRE(same.ResolutionHz(hz));
  CHECK_EQ(hz, 20'000'000U);
}

TEST_CASE("esp32 port: a phase of 32767 ticks fits an RMT half-symbol, 32768 does not") {
  const esp_stub::Rig rig;
  Esp32Ws2812RmtPort port;
  REQUIRE(port.EnsureInitialized());
  const std::array<uint8_t, 3> bytes = {0x12U, 0x34U, 0x56U};
  ws2812_rmt::Waveform wave{};
  wave.resolution_hz = 20'000'000U;
  wave.t0h_ticks = ws2812_rmt::kRmtHalfSymbolMaxTicks;
  wave.t0l_ticks = 19U;
  wave.t1h_ticks = 13U;
  wave.t1l_ticks = 13U;
  wave.reset_ticks = 6000U;
  CHECK(port.Transmit(bytes, wave, 10U));
  CHECK_EQ(esp_stub::Hooks().frames.size(), 1U);
  wave.t0h_ticks = ws2812_rmt::kRmtHalfSymbolMaxTicks + 1U;
  CHECK(!port.Transmit(bytes, wave, 10U));
  CHECK_EQ(esp_stub::Hooks().frames.size(), 1U);
}
