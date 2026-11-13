/**
 * @file ports.cpp
 * @brief The port with every optional hook, as shown on the Porting page. Compiled, never run.
 */
#include <cstdint>
#include <span>

#include "ws2812_rmt.hpp"

bool platform_tx_open(int32_t gpio, uint32_t wanted_hz) noexcept;
uint32_t platform_tx_rate() noexcept;
bool platform_tx_send(const uint8_t* bytes, uint32_t count, const ws2812_rmt::Waveform& wave,
                      uint32_t timeout_ms) noexcept;
bool platform_gpio_output_init(int32_t pin) noexcept;
bool platform_gpio_write(int32_t pin, bool high) noexcept;
void platform_delay_us(uint32_t us) noexcept;
void platform_log(const char* line) noexcept;

// --8<-- [start:full-port]
class FullPort : public ws2812_rmt::GpioInterface<FullPort> {
public:
  bool EnsureInitialized() noexcept {
    // The supply switch is INACTIVE (strip off) before anything drives the data line.
    return platform_gpio_output_init(kPowerGpio) && platform_gpio_write(kPowerGpio, false) &&
           platform_tx_open(kDataGpio, ws2812_rmt::kRecommendedResolutionHz);
  }
  bool ResolutionHz(uint32_t& hz) noexcept {
    hz = platform_tx_rate();
    return hz != 0U;
  }
  bool Transmit(std::span<const uint8_t> wire, const ws2812_rmt::Waveform& wave,
                uint32_t timeout_ms) noexcept {
    return platform_tx_send(wire.data(), static_cast<uint32_t>(wire.size()), wave, timeout_ms);
  }

  // Optional: the strip supply or a level shifter's enable. ACTIVE means powered (a high level
  // here).
  bool GpioSet(ws2812_rmt::CtrlPin pin, ws2812_rmt::GpioSignal signal) noexcept {
    if (pin != ws2812_rmt::CtrlPin::Power) {
      return false;
    }
    return platform_gpio_write(kPowerGpio, signal == ws2812_rmt::GpioSignal::ACTIVE);
  }

  // Required: the power-on settle, and the reset the driver waits after a failed frame.
  void DelayUs(uint32_t us) noexcept {
    platform_delay_us(us);
  }

  // Optional: one line per failure. The driver never formats; the port does.
  void Log(ws2812_rmt::LogLevel level, const char* message) noexcept {
    static_cast<void>(level);
    platform_log(message); // plain text: never use it as a format string
  }

private:
  static constexpr int32_t kDataGpio = 4;
  static constexpr int32_t kPowerGpio = 5;
};
// --8<-- [end:full-port]
