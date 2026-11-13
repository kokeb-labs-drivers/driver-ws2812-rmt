/**
 * @file port_without_delay.cpp
 * @brief Must NOT compile: `DelayUs()` is a required hook, because the driver waits a full reset
 * through it after a failed transmit.
 *
 * EXPECT: Ws2812RmtPort
 */
#include <cstdint>
#include <span>

#include "ws2812_rmt.hpp"

class NoDelayPort : public ws2812_rmt::GpioInterface<NoDelayPort> {
public:
  bool EnsureInitialized() noexcept {
    return true;
  }
  bool ResolutionHz(uint32_t& hz) noexcept {
    hz = 20'000'000;
    return true;
  }
  bool Transmit(std::span<const uint8_t> wire, const ws2812_rmt::Waveform& wave,
                uint32_t timeout_ms) noexcept {
    static_cast<void>(wire);
    static_cast<void>(wave);
    static_cast<void>(timeout_ms);
    return true;
  }
};

int main() {
  NoDelayPort port;
  ws2812_rmt::Ws2812Rmt<NoDelayPort, 4> strip(port);
  return static_cast<int>(strip.EnsureInitialized().error);
}
