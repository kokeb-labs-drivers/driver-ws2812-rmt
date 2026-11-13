/**
 * @file esp32_ws2812_rmt_port.hpp
 * @brief ESP-IDF port for the WS2812_RMT driver: the RMT transmitter with a bytes encoder, an
 * optional GPIO for the strip supply, `esp_rom_delay_us()` and `ESP_LOG`.
 * @copyright Copyright (c) 2026 The driver-ws2812-rmt authors. SPDX-License-Identifier: MIT
 *
 * Not verified on hardware. CI builds it with ESP-IDF, and the host tests build it and the
 * examples' start-up against stand-ins of the ESP-IDF headers (tests/esp32_stub/); the bench steps
 * are in docs/porting.md. The reset is a timed wait after the last bit, not an RMT symbol: RMT
 * idles low after a transmission (`eot_level`), so waiting after `rmt_tx_wait_all_done()` keeps the
 * line low for the reset, on success and after a failure (the driver then adds a full reset through
 * `DelayUs()` before the next frame). The wait is a busy loop of the CPU: it is 300 us per frame at
 * the V5 reset.
 *
 * Tick rate: the RMT divides its source clock by an integer that ESP-IDF rounds to the nearest
 * (`rmt_common.c`), so the rate it runs at can be above or below the requested one.
 * `EnsureInitialized()` computes the rate the same way (`ws2812_rmt::ResolveRmtRate()`) and refuses
 * one more than 1000 ppm from the request. After `rmt_new_tx_channel()` it reads the rate the
 * channel really got (`rmt_get_channel_resolution()`) and refuses to start when it differs from the
 * computed one: on the targets whose channels share one group prescaler (C3, C5, C6, H2, S3, P4),
 * a channel created earlier at another rate fixes the group clock, and this channel then runs at a
 * rate the waveform was not computed for (wrong colours, no error). `ResolutionHz()` reports the
 * rate read back. `rmt_get_channel_resolution()` is declared in ESP-IDF's `esp_private/rmt.h`
 * (internal, present in v5.5): there is no public query of the real rate. The default request is 20
 * MHz, and 32 MHz on the ESP32-H2, whose RMT runs from the 32 MHz crystal (20 MHz is not reachable
 * there: the divider 2 gives 16 MHz).
 *
 * Memory: with `CONFIG_RMT_TX_ISR_CACHE_SAFE`, `rmt_transmit()` refuses a payload outside internal
 * RAM, so every `Show()` and `TraceWire()` returns `Bus` when the driver object (it holds the wire
 * buffer) is in PSRAM or a `TraceWire()` span is in flash. Keep both in internal RAM. An underrun
 * of the refill (a stall of the interrupt longer than the symbols in the RMT memory) is not visible
 * to this port; use DMA where the chip has it (default below).
 */
#ifndef WS2812_RMT_ESP32_WS2812_RMT_PORT_HPP
#define WS2812_RMT_ESP32_WS2812_RMT_PORT_HPP

#include <algorithm>
#include <cstdint>
#include <limits>
#include <span>

#include "driver/gpio.h"
#include "driver/rmt_encoder.h"
#include "driver/rmt_tx.h"
#include "esp_clk_tree.h"
#include "esp_log.h"
#include "esp_private/rmt.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "sdkconfig.h"
#include "soc/soc_caps.h"

#include "ws2812_rmt.hpp"

/// The pins and the RMT settings of the ESP-IDF port; change them to your wiring.
struct Esp32Ws2812RmtPins {
  int data = 8;   ///< The strip's data input (through a level shifter for 5 V parts).
  int power = -1; ///< Enable of the strip supply or the level shifter; -1 when always on.
#if defined(CONFIG_IDF_TARGET_ESP32H2)
  uint32_t resolution_hz = 32'000'000; ///< Requested RMT tick rate: the 32 MHz crystal, undivided.
#else
  uint32_t resolution_hz = 20'000'000; ///< Requested RMT tick rate, at least kMinResolutionHz.
#endif
  /// RMT symbols per channel; 0 picks the default of the target: two memory blocks without DMA
  /// (a multiple of SOC_RMT_MEM_WORDS_PER_CHANNEL: 128 on the ESP32 and S2, 96 on the C3, C6, H2
  /// and S3), 1024 with DMA. Not verified on a chip: read the soc_caps of yours.
  size_t mem_block_symbols = 0;
  /// Interrupt priority of the RMT channel, 1 to 3 (0 lets the driver choose the lowest). A high
  /// one shortens the stall of the refill that cuts a frame without DMA. Not verified.
  int intr_priority = 3;
#if defined(SOC_RMT_SUPPORT_DMA) && SOC_RMT_SUPPORT_DMA
  bool with_dma = true; ///< RMT DMA: the refill does not depend on interrupt latency.
#else
  bool with_dma = false; ///< No RMT DMA on this chip: a stall above the refill window cuts a frame.
#endif
};

static_assert(Esp32Ws2812RmtPins{}.resolution_hz >= ws2812_rmt::kMinResolutionHz,
              "the default RMT tick rate must be at least ws2812_rmt::kMinResolutionHz");

/// The WS2812_RMT port on ESP-IDF.
class Esp32Ws2812RmtPort : public ws2812_rmt::GpioInterface<Esp32Ws2812RmtPort> {
public:
  explicit Esp32Ws2812RmtPort(const Esp32Ws2812RmtPins& pins = {}) noexcept : pins_(pins) {}

  bool EnsureInitialized() noexcept {
    if (ready_) {
      return true;
    }
    if (pins_.power >= 0) {
      // The supply starts INACTIVE: set the level before the pin becomes an output (handbook
      // KB-HW-018).
      gpio_config_t io{};
      io.pin_bit_mask = 1ULL << static_cast<unsigned>(pins_.power);
      io.mode = GPIO_MODE_OUTPUT;
      if ((gpio_set_level(static_cast<gpio_num_t>(pins_.power), 0) != ESP_OK) ||
          (gpio_config(&io) != ESP_OK)) {
        return false;
      }
    }
    rmt_tx_channel_config_t channel_config{};
    channel_config.gpio_num = static_cast<gpio_num_t>(pins_.data);
    channel_config.clk_src = RMT_CLK_SRC_DEFAULT;
    channel_config.resolution_hz = pins_.resolution_hz;
    channel_config.flags.init_level = 0; // the line idles low
    channel_config.mem_block_symbols = memSymbols();
    channel_config.intr_priority = pins_.intr_priority;
    channel_config.trans_queue_depth = 1;
    channel_config.flags.with_dma = pins_.with_dma ? 1U : 0U;
    if (!realRate(channel_config.clk_src, real_hz_) ||
        (rmt_new_tx_channel(&channel_config, &channel_) != ESP_OK)) {
      return false;
    }
    // The rate the channel really got: another channel of the same group may have fixed the group
    // prescaler first, and then this one does not run at the computed rate.
    uint32_t got_hz = 0;
    if ((rmt_get_channel_resolution(channel_, &got_hz) != ESP_OK) || (got_hz != real_hz_)) {
      esp_log_write(ESP_LOG_ERROR, kTag, "%s\n",
                    "the RMT channel does not run at the computed tick rate (shared group clock?)");
      static_cast<void>(rmt_del_channel(channel_));
      channel_ = nullptr;
      return false;
    }
    // The encoder depends on the waveform, so Transmit() builds it when the waveform changes.
    ready_ = true;
    return true;
  }

  bool ResolutionHz(uint32_t& hz) noexcept {
    if (!ready_) {
      return false;
    }
    hz = real_hz_; // what the channel really runs at (read back), not the requested rate
    return true;
  }

  bool Transmit(std::span<const uint8_t> wire, const ws2812_rmt::Waveform& wave,
                uint32_t timeout_ms) noexcept {
    if (!ready_ || !fitsSymbol(wave) || !buildEncoder(wave)) {
      return false;
    }
    if (!enabled_) {
      if (rmt_enable(channel_) != ESP_OK) {
        return false;
      }
      enabled_ = true;
    }
    rmt_transmit_config_t config{};
    config.loop_count = 0;
    config.flags.eot_level = 0; // the line idles low: the reset level
    bool ok = rmt_transmit(channel_, encoder_, wire.data(), wire.size(), &config) == ESP_OK;
    ok = ok && (rmt_tx_wait_all_done(channel_, waitMs(timeout_ms)) == ESP_OK);
    if (!ok) {
      // Stop the channel (the line goes to its idle level, low) and enable it again next time.
      static_cast<void>(rmt_disable(channel_));
      enabled_ = false;
    }
    // The reset, on success and after a failure: the line stays low for reset_ticks after the last
    // bit, so the pixels latch what they have (the driver adds another full reset before the next
    // frame after a failure).
    esp_rom_delay_us(
        static_cast<uint32_t>((static_cast<uint64_t>(wave.reset_ticks) * 1'000'000ULL) /
                              wave.resolution_hz) +
        1U);
    return ok;
  }

  bool GpioSet(ws2812_rmt::CtrlPin pin, ws2812_rmt::GpioSignal signal) noexcept {
    if (pin != ws2812_rmt::CtrlPin::Power) {
      return false;
    }
    if (pins_.power < 0) {
      return true; // no pin: the supply is always on, so powering it on or off cannot fail
    }
    const uint32_t level = (signal == ws2812_rmt::GpioSignal::ACTIVE) ? 1U : 0U;
    return gpio_set_level(static_cast<gpio_num_t>(pins_.power), level) == ESP_OK;
  }

  void DelayUs(uint32_t us) noexcept {
    esp_rom_delay_us(us);
  }

  void Log(ws2812_rmt::LogLevel level, const char* message) noexcept {
    const esp_log_level_t esp_level =
        (level == ws2812_rmt::LogLevel::Error) ? ESP_LOG_ERROR : ESP_LOG_INFO;
    esp_log_write(esp_level, kTag, "%s\n", message); // plain text, never a format
  }

private:
  static constexpr const char* kTag = "ws2812_rmt";
  static constexpr uint32_t kHalfSymbolMax = ws2812_rmt::kRmtHalfSymbolMaxTicks;

  /// The wait for the end of the frame: ESP-IDF turns milliseconds into whole RTOS ticks rounded
  /// down, and a wait of n ticks can end after n - 1 whole tick periods (it starts part-way through
  /// the current tick). Two tick periods more keep the wait at least `timeout_ms` long.
  static int waitMs(uint32_t timeout_ms) noexcept {
    const uint64_t ms =
        static_cast<uint64_t>(timeout_ms) + (2U * static_cast<uint64_t>(portTICK_PERIOD_MS));
    return static_cast<int>(std::min(ms, static_cast<uint64_t>(std::numeric_limits<int>::max())));
  }

  /// An RMT half-symbol holds 15 bits of ticks.
  static bool fitsSymbol(const ws2812_rmt::Waveform& w) noexcept {
    return (w.resolution_hz != 0U) && (w.t0h_ticks <= kHalfSymbolMax) &&
           (w.t0l_ticks <= kHalfSymbolMax) && (w.t1h_ticks <= kHalfSymbolMax) &&
           (w.t1l_ticks <= kHalfSymbolMax) && (w.t0h_ticks > 0U) && (w.t0l_ticks > 0U) &&
           (w.t1h_ticks > 0U) && (w.t1l_ticks > 0U);
  }

  /// A bytes encoder for the waveform, most significant bit first; the previous one is deleted.
  bool buildEncoder(const ws2812_rmt::Waveform& w) noexcept {
    if ((encoder_ != nullptr) && (w == encoder_wave_)) {
      return true; // the cached encoder already has this waveform
    }
    if (encoder_ != nullptr) {
      static_cast<void>(rmt_del_encoder(encoder_));
      encoder_ = nullptr;
    }
    rmt_bytes_encoder_config_t config{};
    config.bit0.level0 = 1;
    config.bit0.duration0 = w.t0h_ticks;
    config.bit0.level1 = 0;
    config.bit0.duration1 = w.t0l_ticks;
    config.bit1.level0 = 1;
    config.bit1.duration0 = w.t1h_ticks;
    config.bit1.level1 = 0;
    config.bit1.duration1 = w.t1l_ticks;
    config.flags.msb_first = 1;
    if (rmt_new_bytes_encoder(&config, &encoder_) != ESP_OK) {
      return false;
    }
    encoder_wave_ = w;
    return true;
  }

  /// The RMT symbols to ask for: the setting, or the default of the target.
  size_t memSymbols() const noexcept {
    if (pins_.mem_block_symbols != 0U) {
      return pins_.mem_block_symbols;
    }
    return pins_.with_dma ? 1024U : (static_cast<size_t>(SOC_RMT_MEM_WORDS_PER_CHANNEL) * 2U);
  }

  /// The tick rate the RMT really gets (ws2812_rmt::ResolveRmtRate: the divider rounded to the
  /// nearest integer, as ESP-IDF v5.5 does); refuses a rate more than 1000 ppm from the request.
  bool realRate(rmt_clock_source_t source, uint32_t& real_hz) const noexcept {
    uint32_t source_hz = 0;
    if (esp_clk_tree_src_get_freq_hz(static_cast<soc_module_clk_t>(source),
                                     ESP_CLK_TREE_SRC_FREQ_PRECISION_CACHED,
                                     &source_hz) != ESP_OK) {
      return false;
    }
    const auto rate = ws2812_rmt::ResolveRmtRate(source_hz, pins_.resolution_hz);
    if (!rate) {
      return false;
    }
    real_hz = rate.value;
    return true;
  }

  Esp32Ws2812RmtPins pins_;
  bool ready_ = false;
  bool enabled_ = false;
  uint32_t real_hz_ = 0;
  rmt_channel_handle_t channel_ = nullptr;
  rmt_encoder_handle_t encoder_ = nullptr;
  ws2812_rmt::Waveform encoder_wave_{};
};

#endif // WS2812_RMT_ESP32_WS2812_RMT_PORT_HPP
