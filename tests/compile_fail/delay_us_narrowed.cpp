/**
 * @file delay_us_narrowed.cpp
 * @brief Must NOT compile: a DelayUs hook with a narrower parameter would truncate long waits, so
 * the driver rejects it instead of using it (handbook KB-CPP20-007).
 *
 * EXPECT: Ws2812RmtPort
 */
#include <cstdint>

#include "mock_ws2812_rmt.hpp"
#include "ws2812_rmt.hpp"

class ShortDelayPort : public ws2812_rmt_test::FakeRmtCore<ShortDelayPort> {
public:
  void DelayUs(uint16_t us) noexcept {
    static_cast<void>(us);
  }
};

int main() {
  ShortDelayPort port;
  ws2812_rmt::Ws2812Rmt<ShortDelayPort, 4> strip(port);
  return static_cast<int>(strip.EnsureInitialized().error);
}
