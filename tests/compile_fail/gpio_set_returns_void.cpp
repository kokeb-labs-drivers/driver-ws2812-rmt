/**
 * @file gpio_set_returns_void.cpp
 * @brief Must NOT compile: an optional hook with the wrong signature is rejected, not ignored.
 *
 * EXPECT: GpioSet hook must be
 */
#include "mock_ws2812_rmt.hpp"
#include "ws2812_rmt.hpp"

class VoidGpioPort : public ws2812_rmt_test::FakeRmtCore<VoidGpioPort> {
public:
  void DelayUs(uint32_t us) noexcept {
    static_cast<void>(us);
  }
  void GpioSet(ws2812_rmt::CtrlPin pin, ws2812_rmt::GpioSignal signal) noexcept {
    static_cast<void>(pin);
    static_cast<void>(signal);
  }
};

int main() {
  VoidGpioPort port;
  ws2812_rmt::Ws2812Rmt<VoidGpioPort, 4> strip(port);
  return static_cast<int>(strip.EnsureInitialized().error);
}
