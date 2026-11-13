/**
 * @file ws2812_rmt.ipp
 * @brief WS2812_RMT driver implementation, included by `ws2812_rmt.hpp`.
 * @copyright Copyright (c) 2026 The driver-ws2812-rmt authors. SPDX-License-Identifier: MIT
 */
#ifndef WS2812_RMT_IPP
#define WS2812_RMT_IPP

namespace ws2812_rmt {

template <Ws2812RmtPort Port, size_t PixelCapacity>
  requires detail::NonZeroSize<PixelCapacity>
Result<void> Ws2812Rmt<Port, PixelCapacity>::EnsureInitialized() noexcept {
  if (initialized_) {
    return Result<void>::Success();
  }
  if (auto r = initialize(); !r) {
    return r;
  }
  initialized_ = true;
  return Result<void>::Success();
}

template <Ws2812RmtPort Port, size_t PixelCapacity>
  requires detail::NonZeroSize<PixelCapacity>
Result<void> Ws2812Rmt<Port, PixelCapacity>::initialize() noexcept {
  if (const auto valid = Validate(config_, PixelCapacity); !valid) {
    return valid;
  }
  if (!port_.EnsureInitialized()) {
    detail::Log(port_, LogLevel::Error,
                "ws2812_rmt: the port could not start the transmit channel");
    return Result<void>::Failure(Error::Bus);
  }
  uint32_t hz = 0U;
  if (!port_.ResolutionHz(hz)) {
    detail::Log(port_, LogLevel::Error, "ws2812_rmt: the port could not report its tick rate");
    return Result<void>::Failure(Error::Bus);
  }
  if ((config_.variant != Variant::Custom) && (hz < kMinResolutionHz)) {
    detail::Log(port_, LogLevel::Error,
                "ws2812_rmt: the tick rate is below kMinResolutionHz, too coarse for the windows");
    return Result<void>::Failure(Error::TimingUnreachable);
  }
  const auto wave = ResolveWaveform(TimingOf(config_), hz, config_.reset_us);
  if (!wave) {
    detail::Log(port_, LogLevel::Error, "ws2812_rmt: the tick rate cannot meet the timing windows");
    return Result<void>::Failure(wave.error);
  }
  waveform_ = wave.value;
  order_ = OrderOf(config_);
  return Result<void>::Success();
}

template <Ws2812RmtPort Port, size_t PixelCapacity>
  requires detail::NonZeroSize<PixelCapacity>
Result<void> Ws2812Rmt<Port, PixelCapacity>::SetPixel(size_t index, const Colour& colour) noexcept {
  if (index >= Length()) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  pixels_[index] = colour;
  return Result<void>::Success();
}

template <Ws2812RmtPort Port, size_t PixelCapacity>
  requires detail::NonZeroSize<PixelCapacity>
Result<Colour> Ws2812Rmt<Port, PixelCapacity>::GetPixel(size_t index) const noexcept {
  if (index >= Length()) {
    return Result<Colour>::Failure(Error::InvalidArgument);
  }
  return Result<Colour>::Success(pixels_[index]);
}

template <Ws2812RmtPort Port, size_t PixelCapacity>
  requires detail::NonZeroSize<PixelCapacity>
void Ws2812Rmt<Port, PixelCapacity>::Fill(const Colour& colour) noexcept {
  const std::span<Colour> strip = std::span<Colour>(pixels_).first(Length());
  std::fill(strip.begin(), strip.end(), colour);
}

template <Ws2812RmtPort Port, size_t PixelCapacity>
  requires detail::NonZeroSize<PixelCapacity>
void Ws2812Rmt<Port, PixelCapacity>::Clear() noexcept {
  Fill(Colour{});
}

template <Ws2812RmtPort Port, size_t PixelCapacity>
  requires detail::NonZeroSize<PixelCapacity>
uint32_t Ws2812Rmt<Port, PixelCapacity>::timeoutMs(size_t bytes) const noexcept {
  const uint64_t ns = FrameTimeNs(bytes, uint8_t{1}, waveform_);
  return static_cast<uint32_t>(((ns + 999'999U) / 1'000'000U) + config_.transmit_slack_ms);
}

template <Ws2812RmtPort Port, size_t PixelCapacity>
  requires detail::NonZeroSize<PixelCapacity>
Result<void> Ws2812Rmt<Port, PixelCapacity>::Show() noexcept {
  if (!initialized_) {
    return Result<void>::Failure(Error::NotInitialized);
  }
  if (!powered_) {
    return Result<void>::Failure(Error::NotPowered);
  }
  const size_t used = Length() * static_cast<size_t>(order_.count);
  const std::span<uint8_t> wire = std::span<uint8_t>(wire_).first(used);
  const std::span<const Colour> strip = std::span<const Colour>(pixels_).first(Length());
  // The order was validated by EnsureInitialized() and `wire_` holds four bytes per pixel, so the
  // encoder cannot refuse; if that invariant is ever broken nothing is sent.
  last_wire_size_ = 0U;
  if (EncodeFrame(strip, order_, brightness_, gamma_, wire) != used) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  last_wire_size_ = used;
  return send(std::span<const uint8_t>(wire), "ws2812_rmt: the frame was not sent");
}

template <Ws2812RmtPort Port, size_t PixelCapacity>
  requires detail::NonZeroSize<PixelCapacity>
uint32_t Ws2812Rmt<Port, PixelCapacity>::resetUs() const noexcept {
  return static_cast<uint32_t>(TicksToNs(waveform_.reset_ticks, waveform_.resolution_hz) / 1000U) +
         1U;
}

template <Ws2812RmtPort Port, size_t PixelCapacity>
  requires detail::NonZeroSize<PixelCapacity>
Result<void> Ws2812Rmt<Port, PixelCapacity>::send(std::span<const uint8_t> bytes,
                                                  const char* failure) noexcept {
  if (needs_reset_) {
    // The last frame was cut: the pixels may hold half a frame and would shift this one. A low
    // longer than the reset latches it first.
    port_.DelayUs(resetUs());
    needs_reset_ = false;
  }
  if (!port_.Transmit(bytes, waveform_, timeoutMs(bytes.size()))) {
    needs_reset_ = true;
    detail::Log(port_, LogLevel::Error, failure);
    return Result<void>::Failure(Error::Bus);
  }
  return Result<void>::Success();
}

template <Ws2812RmtPort Port, size_t PixelCapacity>
  requires detail::NonZeroSize<PixelCapacity>
Result<void> Ws2812Rmt<Port, PixelCapacity>::TraceWire(std::span<const uint8_t> bytes) noexcept {
  if (!initialized_) {
    return Result<void>::Failure(Error::NotInitialized);
  }
  if (!powered_) {
    return Result<void>::Failure(Error::NotPowered);
  }
  if (bytes.empty() || (bytes.size() > (PixelCapacity * 4U))) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  return send(bytes, "ws2812_rmt: the raw bytes were not sent");
}

template <Ws2812RmtPort Port, size_t PixelCapacity>
  requires detail::NonZeroSize<PixelCapacity>
Result<void> Ws2812Rmt<Port, PixelCapacity>::PowerOn() noexcept {
  if constexpr (!detail::HasGpioSet<Port>) {
    return Result<void>::Failure(Error::Unsupported);
  } else {
    if (!initialized_) {
      return Result<void>::Failure(Error::NotInitialized);
    }
    if (!port_.GpioSet(CtrlPin::Power, GpioSignal::ACTIVE)) {
      detail::Log(port_, LogLevel::Error, "ws2812_rmt: the power pin could not be set");
      return Result<void>::Failure(Error::Bus);
    }
    powered_ = true;
    port_.DelayUs(config_.power_on_settle_us);
    return Result<void>::Success();
  }
}

template <Ws2812RmtPort Port, size_t PixelCapacity>
  requires detail::NonZeroSize<PixelCapacity>
Result<void> Ws2812Rmt<Port, PixelCapacity>::PowerOff() noexcept {
  if constexpr (!detail::HasGpioSet<Port>) {
    return Result<void>::Failure(Error::Unsupported);
  } else {
    if (!initialized_) {
      return Result<void>::Failure(Error::NotInitialized);
    }
    // Refuse sending first: whatever the pin write does, DIN must not be driven from here on.
    powered_ = false;
    if (!port_.GpioSet(CtrlPin::Power, GpioSignal::INACTIVE)) {
      detail::Log(port_, LogLevel::Error, "ws2812_rmt: the power pin could not be set");
      return Result<void>::Failure(Error::Bus);
    }
    return Result<void>::Success();
  }
}

template <Ws2812RmtPort Port, size_t PixelCapacity>
  requires detail::NonZeroSize<PixelCapacity>
Result<Waveform> Ws2812Rmt<Port, PixelCapacity>::CurrentWaveform() const noexcept {
  if (!initialized_) {
    return Result<Waveform>::Failure(Error::NotInitialized);
  }
  return Result<Waveform>::Success(waveform_);
}

template <Ws2812RmtPort Port, size_t PixelCapacity>
  requires detail::NonZeroSize<PixelCapacity>
Result<uint32_t> Ws2812Rmt<Port, PixelCapacity>::FrameTimeUs() const noexcept {
  if (!initialized_) {
    return Result<uint32_t>::Failure(Error::NotInitialized);
  }
  const uint64_t ns = FrameTimeNs(Length(), order_.count, waveform_);
  return Result<uint32_t>::Success(static_cast<uint32_t>((ns + 999U) / 1000U));
}

} // namespace ws2812_rmt

#endif // WS2812_RMT_IPP
