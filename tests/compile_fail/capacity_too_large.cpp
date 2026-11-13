/**
 * @file capacity_too_large.cpp
 * @brief Must NOT compile: the frame arithmetic is proved only up to `kMaxPixels`.
 *
 * EXPECT: the capacity is above kMaxPixels
 */
#include "mock_ws2812_rmt.hpp"
#include "ws2812_rmt.hpp"

int main() {
  ws2812_rmt_test::MinimalPort port;
  return sizeof(ws2812_rmt::Ws2812Rmt<ws2812_rmt_test::MinimalPort, ws2812_rmt::kMaxPixels + 1U>) >
                 0U
             ? 0
             : 1;
}
