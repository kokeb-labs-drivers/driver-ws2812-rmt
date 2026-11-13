/**
 * @file ws2812_rmt_gpio_interface.hpp
 * @brief The port of the WS2812_RMT driver: the CRTP base, its pin enums and the `Ws2812RmtPort`
 * concept. The part is driven by a timed bit stream (`bus: [pio]`), and the port is the only place
 * that touches an MCU peripheral (on ESP32 the RMT transmitter).
 * @copyright Copyright (c) 2026 The driver-ws2812-rmt authors. SPDX-License-Identifier: MIT
 *
 * @details
 * The driver builds the byte stream and resolves the waveform; the port sends it. Derive your class
 * from `ws2812_rmt::GpioInterface<Self>` and implement, all `noexcept` (the contract of each hook
 * is in docs/porting.md):
 *
 * - `bool EnsureInitialized()`: required; open the channel, idle LOW, drive `Power` INACTIVE first.
 * - `bool ResolutionHz(uint32_t& hz)`: required; the tick rate the channel really runs at.
 * - `bool Transmit(std::span<const uint8_t>, const Waveform&, uint32_t timeout_ms)`: required; one
 *   gapless stream, then the reset low; `false` on a failure, a timeout or an underrun it can see,
 *   leaving the line low (the driver then waits a full reset before the next frame).
 * - `void DelayUs(uint32_t)`: required; wait with the line low.
 * - `bool GpioSet(CtrlPin, GpioSignal)`: optional; `Power` ACTIVE switches the strip supply on.
 *   A port with the hook starts unpowered (the application calls `PowerOn()` before the first
 *   `Show()`); a port with the hook but no pin returns `true`.
 * - `void Log(LogLevel, const char*)`: optional; plain text, never a format.
 */
#ifndef WS2812_RMT_GPIO_INTERFACE_HPP
#define WS2812_RMT_GPIO_INTERFACE_HPP

#include <concepts>
#include <cstdint>
#include <span>

#include "ws2812_rmt_detail.hpp"
#include "ws2812_rmt_types.hpp"

namespace ws2812_rmt {

/** @brief Control pins the port may drive through the optional `GpioSet()` hook. */
enum class CtrlPin : uint8_t {
  Power =
      0, ///< The strip supply or the level shifter's enable; ACTIVE means powered. Idle: INACTIVE.
};

/** @brief Logical level of a pin; the port maps it to the physical polarity. */
enum class GpioSignal : uint8_t {
  INACTIVE = 0, ///< The pin function is deasserted.
  ACTIVE = 1,   ///< The pin function is asserted.
};

/// The lowest tick rate the driver accepts for a named variant: below 20 MHz some profile has a
/// window edge within 10 ns of its value (V5 sits exactly on one at 16 MHz and below,
/// docs/timing.md); `Variant::Custom` is not limited.
inline constexpr uint32_t kMinResolutionHz = 20'000'000;
/// The tick rate to prefer: 50 ns ticks leave margin on every window (ambiguity A-5); a finer rate
/// (40 MHz, 80 MHz) keeps the same margins.
inline constexpr uint32_t kRecommendedResolutionHz = 20'000'000;
/// The longest half-symbol of an ESP32 RMT symbol, in ticks (15 bits): the limit of every bit
/// phase. The reset is not a symbol: the port holds the line low for it after the last bit.
inline constexpr uint32_t kRmtHalfSymbolMaxTicks = 32'767;

/**
 * @brief CRTP base of the platform port.
 *
 * The base declares no hook itself (a forwarding hook would turn a missing override into infinite
 * recursion); the driver checks your class with the `Ws2812RmtPort` concept.
 *
 * @tparam Derived Your port class.
 */
template <typename Derived>
class GpioInterface {
public:
  /** @brief Ports are not copied: the driver holds a reference. */
  GpioInterface(const GpioInterface&) = delete;
  /** @brief Ports are not copied: the driver holds a reference. */
  GpioInterface& operator=(const GpioInterface&) = delete;

protected:
  /** @brief Only the derived port is constructed. */
  GpioInterface() noexcept = default;
  /** @brief Destroyed through the derived port only. */
  ~GpioInterface() noexcept = default;
};

/**
 * @brief Satisfied by a class that derives from `GpioInterface<Port>` and implements the required
 * hooks with exactly the types above, `noexcept` and returning `bool` (handbook KB-CPP20-006).
 * @tparam Port The port class.
 */
template <typename Port>
concept Ws2812RmtPort = std::derived_from<Port, GpioInterface<Port>> && requires {
  detail::ExactHook<Port, bool() noexcept>::Take(&Port::EnsureInitialized);
  detail::ExactHook<Port, bool(uint32_t&) noexcept>::Take(&Port::ResolutionHz);
  detail::ExactHook<Port, bool(std::span<const uint8_t>, const Waveform&, uint32_t) noexcept>::Take(
      &Port::Transmit);
  detail::ExactHook<Port, void(uint32_t) noexcept>::Take(&Port::DelayUs);
};

} // namespace ws2812_rmt

#endif // WS2812_RMT_GPIO_INTERFACE_HPP
