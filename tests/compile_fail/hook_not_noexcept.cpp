/**
 * @file hook_not_noexcept.cpp
 * @brief Must NOT compile: a Transmit hook that is not noexcept breaks the port contract, so the
 * port concept rejects it (handbook KB-CPP20-006).
 *
 * EXPECT: Ws2812RmtPort
 */
#include <cstdint>
#include <span>

#include "ws2812_rmt.hpp"

class ThrowingPort : public ws2812_rmt::GpioInterface<ThrowingPort> {
public:
  bool EnsureInitialized() noexcept {
    return true;
  }
  bool ResolutionHz(uint32_t& hz) noexcept {
    hz = 20'000'000;
    return true;
  }
  bool Transmit(std::span<const uint8_t> wire, const ws2812_rmt::Waveform& wave,
                uint32_t timeout_ms) {
    static_cast<void>(wire);
    static_cast<void>(wave);
    static_cast<void>(timeout_ms);
    return true;
  }
};

int main() {
  ThrowingPort port;
  ws2812_rmt::Ws2812Rmt<ThrowingPort, 4> strip(port);
  return static_cast<int>(strip.EnsureInitialized().error);
}
