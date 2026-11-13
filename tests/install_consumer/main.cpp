/**
 * @file main.cpp
 * @brief Builds the driver from the installed package only: headers, version header and .ipp.
 */
#include <cstdint>
#include <span>
#include <string_view>

#include "ws2812_rmt.hpp"

namespace {

// A port that accepts every frame: enough to instantiate the whole driver template.
class SinkPort : public ws2812_rmt::GpioInterface<SinkPort> {
public:
  bool EnsureInitialized() noexcept {
    return true;
  }
  bool ResolutionHz(uint32_t& hz) noexcept {
    hz = ws2812_rmt::kRecommendedResolutionHz;
    return true;
  }
  bool Transmit(std::span<const uint8_t> wire, const ws2812_rmt::Waveform& wave,
                uint32_t timeout_ms) noexcept {
    static_cast<void>(wave);
    static_cast<void>(timeout_ms);
    bytes += wire.size();
    return true;
  }
  bool GpioSet(ws2812_rmt::CtrlPin pin, ws2812_rmt::GpioSignal signal) noexcept {
    static_cast<void>(pin);
    static_cast<void>(signal);
    return true;
  }
  void DelayUs(uint32_t us) noexcept {
    static_cast<void>(us);
  }
  size_t bytes = 0;
};

} // namespace

int main() {
  SinkPort port;
  ws2812_rmt::Ws2812Rmt<SinkPort, 4> strip(port);
  strip.Fill(ws2812_rmt::Colour{1, 2, 3, 0});
  const auto r = strip.EnsureInitialized();
  const auto powered = strip.PowerOn(); // the port has a supply hook, so it starts unpowered
  const auto shown = strip.Show();
  return (r.ok() && powered.ok() && shown.ok() && (port.bytes == 12U) &&
          !std::string_view(ws2812_rmt::GetDriverVersion()).empty())
             ? 0
             : 1;
}
