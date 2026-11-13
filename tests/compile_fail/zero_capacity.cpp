/**
 * @file zero_capacity.cpp
 * @brief Must NOT compile: a strip object holds at least one pixel.
 *
 * EXPECT: NonZeroSize
 */
#include "mock_ws2812_rmt.hpp"
#include "ws2812_rmt.hpp"

int main() {
  ws2812_rmt_test::MinimalPort port;
  ws2812_rmt::Ws2812Rmt<ws2812_rmt_test::MinimalPort, 0> strip(port);
  return static_cast<int>(strip.EnsureInitialized().error);
}
