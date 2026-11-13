/**
 * @file my_port.hpp
 * @brief The port shown on the Quickstart and Porting pages, compiled by the host build.
 */
#ifndef WS2812_RMT_MY_PORT_HPP
#define WS2812_RMT_MY_PORT_HPP

#include <cstdint>
#include <span>

#include "ws2812_rmt_gpio_interface.hpp"

// Your SDK's calls. Declared only: the docs build checks that the port compiles.
bool platform_tx_open(int32_t gpio,
                      uint32_t wanted_hz) noexcept; // a transmit channel, line idle low
uint32_t platform_tx_rate() noexcept;               // the rate it really runs at
bool platform_tx_send(const uint8_t* bytes, uint32_t count, const ws2812_rmt::Waveform& wave,
                      uint32_t timeout_ms) noexcept; // bits expanded by the peripheral
void platform_delay_us(uint32_t us) noexcept;        // a busy wait or a timer wait

// --8<-- [start:port]
class MyPort : public ws2812_rmt::GpioInterface<MyPort> {
public:
  // Create and enable the channel once; the line idles low. Later calls return true at once.
  bool EnsureInitialized() noexcept {
    if (!ready_) {
      ready_ = platform_tx_open(kDataGpio, ws2812_rmt::kRecommendedResolutionHz);
    }
    return ready_;
  }

  // The rate the channel really runs at, not the one you asked for.
  bool ResolutionHz(uint32_t& hz) noexcept {
    hz = platform_tx_rate();
    return hz != 0U;
  }

  // Send every byte, most significant bit first, as one gapless stream, then hold the line low for
  // wave.reset_ticks. Return when the last tick is out, or false on a failure or a timeout (leave
  // the line low: the driver then waits a full reset through DelayUs() before the next frame).
  bool Transmit(std::span<const uint8_t> wire, const ws2812_rmt::Waveform& wave,
                uint32_t timeout_ms) noexcept {
    return platform_tx_send(wire.data(), static_cast<uint32_t>(wire.size()), wave, timeout_ms);
  }

  // A wait with the line low.
  void DelayUs(uint32_t us) noexcept {
    platform_delay_us(us);
  }

private:
  static constexpr int32_t kDataGpio = 4;
  bool ready_ = false;
};
// --8<-- [end:port]

#endif // WS2812_RMT_MY_PORT_HPP
