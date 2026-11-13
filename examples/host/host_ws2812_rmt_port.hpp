/**
 * @file host_ws2812_rmt_port.hpp
 * @brief The host port: the driver's hooks over a simulated transmit path, so the example runs on
 * any PC.
 *
 * `ws2812_rmt_test::FakeRmtCore` (tests/mock_ws2812_rmt.hpp) supplies the required hooks: it
 * expands every frame into the half-symbols the ESP32 RMT bytes encoder would send, and keeps them
 * for the pixel-chain model to read. This class adds what a real port adds: `GpioSet()` for the
 * strip supply, `DelayUs()` (the simulation does not sleep) and `Log()` to the console. A port for
 * real hardware on a PC (a USB LED controller, a bit-banged GPIO) implements the same hooks: see
 * docs/porting.md.
 */
#ifndef WS2812_RMT_HOST_WS2812_RMT_PORT_HPP
#define WS2812_RMT_HOST_WS2812_RMT_PORT_HPP

#include <cstdint>
#include <cstdio>

#include "mock_ws2812_rmt.hpp"

/// The host port: simulated transmit path, real console.
class HostWs2812RmtPort : public ws2812_rmt_test::FakeRmtCore<HostWs2812RmtPort> {
public:
  /// Nothing sleeps; the simulation is instantaneous.
  void DelayUs(uint32_t us) noexcept {
    static_cast<void>(us);
  }

  /// The strip supply of the simulated strip.
  bool GpioSet(ws2812_rmt::CtrlPin pin, ws2812_rmt::GpioSignal signal) noexcept {
    if (pin != ws2812_rmt::CtrlPin::Power) {
      return false;
    }
    powered = signal == ws2812_rmt::GpioSignal::ACTIVE;
    return true;
  }

  /// Driver messages (they carry their own `ws2812_rmt:` prefix) go to the console, one line each.
  void Log(ws2812_rmt::LogLevel level, const char* message) noexcept {
    static_cast<void>(level);
    static_cast<void>(std::puts(message)); // plain text: never pass it as a format string
  }

  bool powered = false; ///< The simulated supply.
};

#endif // WS2812_RMT_HOST_WS2812_RMT_PORT_HPP
