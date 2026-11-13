/**
 * @file not_a_port.cpp
 * @brief Must NOT compile: the driver is constrained by the port concept.
 *
 * EXPECT: Ws2812RmtPort
 */
#include "ws2812_rmt.hpp"

struct NotAPort {
  bool EnsureInitialized() noexcept {
    return true;
  }
};

int main() {
  NotAPort port;
  ws2812_rmt::Ws2812Rmt<NotAPort, 4> strip(port);
  return static_cast<int>(strip.EnsureInitialized().error);
}
