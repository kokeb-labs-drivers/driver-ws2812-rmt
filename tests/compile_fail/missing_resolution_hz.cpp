/**
 * @file missing_resolution_hz.cpp
 * @brief Must NOT compile: without `ResolutionHz()` the driver cannot turn nanoseconds into ticks.
 *
 * EXPECT: Ws2812RmtPort
 */
#include <cstdint>
#include <span>

#include "ws2812_rmt.hpp"

class NoRateWsPort : public ws2812_rmt::GpioInterface<NoRateWsPort> {
public:
  bool EnsureInitialized() noexcept {
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
  NoRateWsPort port;
  ws2812_rmt::Ws2812Rmt<NoRateWsPort, 4> strip(port);
  return static_cast<int>(strip.EnsureInitialized().error);
}
