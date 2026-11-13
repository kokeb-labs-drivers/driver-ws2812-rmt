/**
 * @file transmit_narrowed.cpp
 * @brief Must NOT compile: a Transmit hook whose timeout parameter is narrower than `uint32_t`
 * would truncate a long frame's timeout, so the port concept rejects it (handbook KB-CPP20-006).
 *
 * EXPECT: Ws2812RmtPort
 */
#include <cstdint>
#include <span>

#include "ws2812_rmt.hpp"

class NarrowPort : public ws2812_rmt::GpioInterface<NarrowPort> {
public:
  bool EnsureInitialized() noexcept {
    return true;
  }
  bool ResolutionHz(uint32_t& hz) noexcept {
    hz = 20'000'000;
    return true;
  }
  bool Transmit(std::span<const uint8_t> wire, const ws2812_rmt::Waveform& wave,
                uint16_t timeout_ms) noexcept {
    static_cast<void>(wire);
    static_cast<void>(wave);
    static_cast<void>(timeout_ms);
    return true;
  }
};

int main() {
  NarrowPort port;
  ws2812_rmt::Ws2812Rmt<NarrowPort, 4> strip(port);
  return static_cast<int>(strip.EnsureInitialized().error);
}
