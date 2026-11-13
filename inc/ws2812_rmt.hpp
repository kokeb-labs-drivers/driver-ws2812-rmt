/**
 * @file ws2812_rmt.hpp
 * @brief WS2812_RMT driver: the one header to include.
 * @copyright Copyright (c) 2026 The driver-ws2812-rmt authors. SPDX-License-Identifier: MIT
 *
 * @details
 * `ws2812_rmt::Ws2812Rmt<Port, PixelCapacity>` drives a strip of single-wire addressable LEDs
 * (WS2812, SK6812 and compatible) through a port that sends a timed bit stream (on ESP32 the RMT
 * peripheral). It owns the pixel and wire buffers (fixed capacity, no heap), builds the byte
 * stream, resolves the waveform from the datasheet windows and the port's real tick rate, and
 * checks arguments. No SDK, no heap, no exceptions, no lock.
 */
#ifndef WS2812_RMT_HPP
#define WS2812_RMT_HPP

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "ws2812_rmt_detail.hpp"
#include "ws2812_rmt_functions.hpp"
#include "ws2812_rmt_gamma.hpp"
#include "ws2812_rmt_gpio_interface.hpp"
#include "ws2812_rmt_types.hpp"
#include "ws2812_rmt_version.h"

namespace ws2812_rmt {

namespace detail {

// Optional hooks: a `Declares*` concept only tests the name, the `Has*` concept the exact
// signature, `noexcept` and the result type; the driver rejects a hook that is declared but does
// not match (handbook KB-CPP20-007).

/// The port declares something called `GpioSet` (used to reject a misdeclared hook).
template <typename Port>
concept DeclaresGpioSet =
    requires(Port& port) { port.GpioSet(CtrlPin::Power, GpioSignal::ACTIVE); };

/// The port drives the power pin: `bool GpioSet(CtrlPin, GpioSignal) noexcept`.
template <typename Port>
concept HasGpioSet =
    requires { ExactHook<Port, bool(CtrlPin, GpioSignal) noexcept>::Take(&Port::GpioSet); };

/// The port declares something called `Log`.
template <typename Port>
concept DeclaresLog = requires(Port& port) { port.Log(LogLevel::Info, ""); };

/// The port takes log lines: `void Log(LogLevel, const char* message) noexcept`.
template <typename Port>
concept HasLog =
    requires { ExactHook<Port, void(LogLevel, const char*) noexcept>::Take(&Port::Log); };

/// A strip capacity above zero.
template <size_t N>
concept NonZeroSize = (N > 0U);

/// Log through the port, or nothing when the port has no `Log()`.
template <typename Port>
void Log(Port& port, LogLevel level, const char* message) noexcept {
  if constexpr (HasLog<Port>) {
    port.Log(level, message);
  } else {
    static_cast<void>(port);
    static_cast<void>(level);
    static_cast<void>(message);
  }
}

} // namespace detail

/**
 * @defgroup ws2812_rmt_driver WS2812_RMT driver
 * @brief The driver class.
 * @{
 */

/**
 * @brief Driver for one strip of up to `PixelCapacity` single-wire addressable LEDs.
 *
 * Lifecycle: construct with the configuration (no I/O), then `EnsureInitialized()`, the one way to
 * start; then set pixels and call `Show()`. `Show()` and `CurrentWaveform()` return
 * `NotInitialized` until `EnsureInitialized()` has succeeded; the pixel setters only store.
 *
 * Optional hooks: `bool GpioSet(CtrlPin, GpioSignal)` (the strip supply; without it `PowerOn()` and
 * `PowerOff()` return `Unsupported`; with it the strip starts unpowered, and `Show()` and
 * `TraceWire()` are refused with `NotPowered` until the first successful `PowerOn()` and again
 * after `PowerOff()`), `void Log(LogLevel, const char*)`.
 *
 * Concurrency: one owner task per strip; no locking, never from an interrupt. Memory:
 * `PixelCapacity` pixels of 4 bytes plus `PixelCapacity x 4` wire bytes live inside the object, so
 * give it static storage on a small stack.
 *
 * @tparam Port Your class derived from `GpioInterface<Port>`.
 * @tparam PixelCapacity The most pixels the strip object holds, 1 to `kMaxPixels`.
 */
template <Ws2812RmtPort Port, size_t PixelCapacity>
  requires detail::NonZeroSize<PixelCapacity>
class Ws2812Rmt {
  static_assert(PixelCapacity <= kMaxPixels, "ws2812_rmt: the capacity is above kMaxPixels");
  static_assert(!detail::DeclaresGpioSet<Port> || detail::HasGpioSet<Port>,
                "ws2812_rmt: the optional GpioSet hook must be `bool GpioSet(CtrlPin, GpioSignal) "
                "noexcept`, so "
                "that a failed pin write is reported");
  static_assert(!detail::DeclaresLog<Port> || detail::HasLog<Port>,
                "ws2812_rmt: the optional Log hook must be `void Log(LogLevel, const char* "
                "message) noexcept`");

public:
  /**
   * @brief Bind the driver to a port and store the configuration. No I/O, cannot fail; the pixels
   * start black.
   * @param port The port; it must outlive the driver.
   * @param config The configuration, checked by `EnsureInitialized()` before any I/O.
   */
  explicit Ws2812Rmt(Port& port, const Config& config = Config::Default()) noexcept
      : port_(port), config_(config), order_(OrderOf(config)), brightness_(config.brightness),
        gamma_(config.gamma) {}

  /** @brief One driver object per strip. */
  Ws2812Rmt(const Ws2812Rmt&) = delete;
  /** @brief One driver object per strip. */
  Ws2812Rmt& operator=(const Ws2812Rmt&) = delete;

  /**
   * @brief Start the driver, once: the only way to initialise it.
   *
   * Validates the configuration (no I/O on failure), lets the port create its channel, asks the
   * port for its real tick rate and resolves the waveform inside the datasheet windows. After it
   * succeeds later calls return `Ok` at once with no I/O; after it fails the driver stays
   * uninitialised and the next call starts again from the beginning.
   *
   * @return `Ok`, `InvalidArgument` (`Validate()` failed), `Bus` (the port could not start or
   * report a rate) or `TimingUnreachable` (no tick count fits a window at that rate).
   */
  Result<void> EnsureInitialized() noexcept;

  /** @return true after a successful `EnsureInitialized()`. No I/O. */
  [[nodiscard]] bool IsInitialized() const noexcept {
    return initialized_;
  }

  /**
   * @brief Store a colour; nothing is sent until `Show()`. The logical colour is kept, so
   * brightness stays reversible.
   * @param index Pixel index, 0 to `Length()` - 1.
   * @param colour The colour.
   * @return `Ok`, or `InvalidArgument` for an index out of range (nothing changes).
   */
  Result<void> SetPixel(size_t index, const Colour& colour) noexcept;

  /**
   * @brief Read back a stored colour (the logical colour, before brightness and gamma). No I/O.
   * @param index Pixel index.
   * @return The colour, or `InvalidArgument`.
   */
  [[nodiscard]] Result<Colour> GetPixel(size_t index) const noexcept;

  /**
   * @brief Set every pixel of the strip to `colour`. No I/O.
   * @param colour The logical colour.
   */
  void Fill(const Colour& colour) noexcept;

  /** @brief Set every pixel to black. No I/O. */
  void Clear() noexcept;

  /**
   * @brief Set the linear brightness applied at `Show()`; 0 is off, 255 is unchanged. No I/O.
   * @param brightness The scale, 0 to 255.
   */
  void SetBrightness(uint8_t brightness) noexcept {
    brightness_ = brightness;
  }

  /** @return The brightness. No I/O. */
  [[nodiscard]] uint8_t Brightness() const noexcept {
    return brightness_;
  }

  /**
   * @brief Set the transfer curve applied at `Show()`; an unknown value acts as `Gamma::None`. No
   * I/O.
   * @param gamma The curve.
   */
  void SetGamma(Gamma gamma) noexcept {
    gamma_ = detail::IsEnumerator(gamma) ? gamma : Gamma::None;
  }

  /**
   * @brief Encode the pixels (gamma, then brightness, in the wire order) and send them, blocking
   * until the frame and the reset are out. One pass, no allocation. A failed send leaves the pixels
   * intact and the driver usable; the next `Show()` or `TraceWire()` first waits a full reset
   * through `DelayUs()`, so a half frame the failure left in the pixels is latched and cannot shift
   * the next one.
   *
   * With a `GpioSet()` hook it is refused (`NotPowered`) and nothing is sent until the first
   * successful `PowerOn()` and again after `PowerOff()`: a driven DIN on an unpowered strip
   * back-feeds it (docs/hardware.md).
   *
   * @return `Ok`, `NotInitialized` or `NotPowered` (no port call is made), or `Bus` (the port's
   * `Transmit()` failed, timed out or underran where the port can see it; an underrun it cannot
   * see returns `Ok`, see docs/contract.md).
   */
  Result<void> Show() noexcept;

  /**
   * @brief Single-shot raw access: send `bytes` exactly as given, as one frame with the resolved
   * waveform. No colour order, brightness or gamma, no retry, the stored pixels untouched. For
   * finding the order of a strip and for bench work. Like `Show()` it blocks until the bytes and
   * the reset are out.
   * @param bytes The wire bytes, 1 to 4 per pixel of the capacity.
   * @return `Ok`, `NotInitialized` or `NotPowered` (no port call is made), `InvalidArgument` (empty
   * or too long) or `Bus`.
   */
  Result<void> TraceWire(std::span<const uint8_t> bytes) noexcept;

  /**
   * @brief The bytes `Show()` encoded last, whether or not they were sent; empty before the first
   * `Show()`. No I/O. The view is valid until the next `Show()`.
   * @return The wire bytes.
   */
  [[nodiscard]] std::span<const uint8_t> GetLastWire() const& noexcept {
    return std::span<const uint8_t>(wire_).first(last_wire_size_);
  }
  /** @brief Not on a temporary driver: the view would dangle (handbook KB-CPP20-044). */
  void GetLastWire() const&& = delete;

  /**
   * @brief Switch the strip supply on and wait `Config::power_on_settle_us`.
   * @return `Ok`, `NotInitialized`, `Unsupported` (no `GpioSet()` hook) or `Bus`.
   */
  Result<void> PowerOn() noexcept;

  /**
   * @brief Switch the strip supply off. The waveform is kept; no re-initialisation is needed.
   * `Show()` and `TraceWire()` return `NotPowered` until `PowerOn()` succeeds. The switch-off is
   * never refused for a state the driver is in doubt about.
   * @return `Ok`, `NotInitialized`, `Unsupported` (no `GpioSet()` hook) or `Bus`.
   */
  Result<void> PowerOff() noexcept;

  /** @return The number of pixels of the strip (`Config::pixel_count`, or the capacity). No I/O. */
  [[nodiscard]] size_t Length() const noexcept {
    const auto wanted = static_cast<size_t>(config_.pixel_count);
    return ((wanted == 0U) || (wanted > PixelCapacity)) ? PixelCapacity : wanted;
  }

  /** @return The channels per pixel on the wire (3 or 4). No I/O. */
  [[nodiscard]] uint8_t ChannelsPerPixel() const noexcept {
    return order_.count;
  }

  /** @return The waveform in use, or `NotInitialized`. No I/O. */
  [[nodiscard]] Result<Waveform> CurrentWaveform() const noexcept;

  /**
   * @brief Worst-case time of one `Show()`: every bit the longer of the two codes, plus the reset.
   * @return Microseconds rounded up, or `NotInitialized`. No I/O.
   */
  [[nodiscard]] Result<uint32_t> FrameTimeUs() const noexcept;

  /** @return The configuration stored. No I/O. */
  [[nodiscard]] const Config& GetConfig() const& noexcept {
    return config_;
  }
  /** @brief Not on a temporary driver: the reference would dangle (handbook KB-CPP20-044). */
  void GetConfig() const&& = delete;

private:
  Result<void> initialize() noexcept;
  [[nodiscard]] uint32_t timeoutMs(size_t bytes) const noexcept;
  Result<void> send(std::span<const uint8_t> bytes, const char* failure) noexcept;
  [[nodiscard]] uint32_t resetUs() const noexcept;

  Port& port_;
  Config config_;
  ColourOrder order_;
  uint8_t brightness_;
  Gamma gamma_;
  Waveform waveform_{};
  bool initialized_{false};
  /// A port with a supply hook starts unpowered (its pin starts inactive): true only after a
  /// successful `PowerOn()`, false again from `PowerOff()`. A port without the hook has no supply
  /// to switch.
  bool powered_{!detail::HasGpioSet<Port>};
  bool needs_reset_{false}; ///< A `Transmit()` failed: the pixels may hold half a frame.
  size_t last_wire_size_{0};
  std::array<Colour, PixelCapacity> pixels_{};
  std::array<uint8_t, PixelCapacity * 4U> wire_{};
};

/** @} */

} // namespace ws2812_rmt

// codeql::misra_deviation_next_line(MD-CPP-009)
#include "../src/ws2812_rmt.ipp" // IWYU pragma: export

#endif // WS2812_RMT_HPP
