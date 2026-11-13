/**
 * @file ws2812_rmt_types.hpp
 * @brief Public types of the WS2812_RMT driver: errors, `Result<T>`, colours, the timing profile of
 * every supported part, `Waveform` and `Config`.
 * @copyright Copyright (c) 2026 The driver-ws2812-rmt authors. SPDX-License-Identifier: MIT
 *
 * @details
 * The parts are single-wire addressable LEDs (WS2812 and WS2812B in every published timing
 * generation, WS2812C, WS2812D, SK6812, SK6812RGBW, SK6812MINI) with one 24 bit (32 bit for RGBW)
 * NRZ frame at about 800 kbit/s: a bit is a high pulse then a low pulse, its value is the length of
 * the high pulse, and a low longer than the reset time latches the frame. The family shares the
 * frame; the timing windows, the reset time, the colour order and the channel count differ, and
 * those are the data of this header (docs/timing.md and docs/datasheet.md give each value its
 * document and page).
 */
#ifndef WS2812_RMT_TYPES_HPP
#define WS2812_RMT_TYPES_HPP

#include <array>
#include <cstddef>
#include <cstdint>

#include "ws2812_rmt_detail.hpp"
#include "ws2812_rmt_version.h"

/** @brief The WS2812_RMT driver: everything it declares lives here; internals in `detail`. */
namespace ws2812_rmt {

/**
 * @defgroup ws2812_rmt_types Driver types
 * @brief Errors, results, colours, timing profiles, waveform and configuration.
 * @{
 */

/**
 * @brief Why a call failed. Values 0 to 9 are the shared base set; chip-specific values start
 * at 16.
 *
 * The pixels have no read-back path: no identity, no status, no CRC, so `Timeout`,
 * `DeviceIdMismatch`, `Crc`, `Busy` and `DeviceFault` are never returned (docs/contract.md).
 */
enum class [[nodiscard]] Error : uint8_t {
  Ok = 0,              ///< The call succeeded.
  NotInitialized = 1,  ///< `EnsureInitialized()` has not succeeded.
  InvalidArgument = 2, ///< `Validate()` failed, or a pixel index is out of range.
  Bus = 3,             ///< A port hook (init, resolution, transmit, power pin) returned `false`; an
                       ///< underrun the port cannot see is not reported (docs/contract.md).
  Timeout = 4,         ///< Not returned by this driver: the port reports a timeout as `false`.
  DeviceIdMismatch = 5, ///< Not returned by this driver: the pixels are write-only.
  Crc = 6,              ///< Not returned by this driver: the frame has no checksum.
  Busy = 7,             ///< Not returned by this driver: `Show()` blocks until the frame is out.
  DeviceFault = 8,      ///< Not returned by this driver: no pixel reports a fault.
  Unsupported = 9,      ///< `PowerOn()` or `PowerOff()` on a port without a `GpioSet()` hook.
  TimingUnreachable =
      16, ///< The port's tick rate cannot place any tick count inside a timing window.
  NotPowered =
      17 ///< `Show()` or `TraceWire()` between `PowerOff()` and `PowerOn()`: nothing is sent.
};

/**
 * @brief The name of an error, for logs.
 * @param error The error.
 * @return A static string; `"Unknown"` for a value that is not an enumerator.
 */
constexpr const char* ToString(Error error) noexcept {
  switch (error) {
  case Error::Ok:
    return "Ok";
  case Error::NotInitialized:
    return "NotInitialized";
  case Error::InvalidArgument:
    return "InvalidArgument";
  case Error::Bus:
    return "Bus";
  case Error::Timeout:
    return "Timeout";
  case Error::DeviceIdMismatch:
    return "DeviceIdMismatch";
  case Error::Crc:
    return "Crc";
  case Error::Busy:
    return "Busy";
  case Error::DeviceFault:
    return "DeviceFault";
  case Error::Unsupported:
    return "Unsupported";
  case Error::TimingUnreachable:
    return "TimingUnreachable";
  case Error::NotPowered:
    return "NotPowered";
  default:
    break;
  }
  return "Unknown";
}

/**
 * @brief The value of a call, or why there is none.
 * @tparam T The value type. `Result<void>` carries only the error.
 */
template <typename T>
struct [[nodiscard]] Result {
  T value{};              ///< The value; `T{}` when the call failed.
  Error error{Error::Ok}; ///< `Error::Ok` on success.

  /** @return true when the call succeeded. */
  [[nodiscard]] constexpr bool ok() const noexcept {
    return error == Error::Ok;
  }
  /** @return true when the call succeeded. */
  constexpr explicit operator bool() const noexcept {
    return ok();
  }
  /**
   * @brief A successful result.
   * @param v The value.
   * @return `{v, Error::Ok}`.
   */
  static constexpr Result Success(T v) noexcept {
    return {v, Error::Ok};
  }
  /**
   * @brief A failed result.
   * @param e Why it failed; never `Error::Ok`.
   * @return `{T{}, e}`.
   */
  static constexpr Result Failure(Error e) noexcept {
    return {T{}, e};
  }
};

/** @brief The result of a call that returns no value. */
template <>
struct [[nodiscard]] Result<void> {
  Error error{Error::Ok}; ///< `Error::Ok` on success.

  /** @return true when the call succeeded. */
  [[nodiscard]] constexpr bool ok() const noexcept {
    return error == Error::Ok;
  }
  /** @return true when the call succeeded. */
  constexpr explicit operator bool() const noexcept {
    return ok();
  }
  /** @return A successful result. */
  static constexpr Result Success() noexcept {
    return {Error::Ok};
  }
  /**
   * @brief A failed result.
   * @param e Why it failed; never `Error::Ok`.
   * @return `{e}`.
   */
  static constexpr Result Failure(Error e) noexcept {
    return {e};
  }
};

/** @brief Severity passed to the optional `Log()` hook. */
enum class LogLevel : uint8_t {
  Error = 0, ///< A call failed.
  Warning,   ///< Something unexpected that the driver recovered from.
  Info,      ///< Lifecycle events.
  Debug,     ///< Detail for bring-up.
};

/**
 * @brief The timing generation (window set) of the part, chosen by what is printed on or known
 * about it.
 *
 * No single waveform serves every part: the windows of the WS2812B alone differ between the 2013
 * documents, the 2017 document and the V5 and V6 documents, and their intersection is empty. Each
 * named variant is the intersection of the windows of all documents that describe the same parts,
 * so a waveform inside it satisfies every one of those documents (docs/timing.md).
 */
enum class Variant : uint8_t {
  Ws2812,       ///< WS2812 (6 pin): 0.35/0.8/0.7/0.6 us +-150 ns, reset above 50 us.
  Ws2812bPreV5, ///< WS2812B and WS2812S of 2013 to 2017: reset above 300 us.
  Ws2812bV5,  ///< WS2812B V5 and V6, WS2812B-2020, WS2812B-1313, WS2812C-2020: reset above 280 us.
  Ws2812c,    ///< WS2812C (5050) and WS2812-2020: T1L window 220 to 420 ns.
  Ws2812d,    ///< WS2812D-F8: 750 to 1000 ns on T0L and T1H.
  Sk6812,     ///< SK6812 RGB (Rev 01): 0.3/0.9/0.6/0.6 us +-150 ns, reset 80 us.
  Sk6812Rgbw, ///< SK6812RGBW: 32 bit frame, low inside a frame below 20 us.
  Sk6812Mini, ///< SK6812MINI and MINI-E: windows of the later revisions, low below 20 us. The
              ///< SK6812MINIRGBX sheet (skm) only adds windows: its RGBW part takes `Sk6812Rgbw`
              ///< or `Custom`.
  Custom,     ///< Any other part with the same frame: the whole timing is `Config::custom`.
};

namespace detail {
/// The last enumerator of `Variant`, for `IsEnumerator()`.
template <>
struct EnumLast<Variant> {
  static constexpr Variant kValue = Variant::Custom; ///< The last enumerator.
};
} // namespace detail

/** @brief A colour channel of a pixel. */
enum class Channel : uint8_t {
  R, ///< Red.
  G, ///< Green.
  B, ///< Blue.
  W, ///< White (RGBW parts only).
};

namespace detail {
/// The last enumerator of `Channel`, for `IsEnumerator()`.
template <>
struct EnumLast<Channel> {
  static constexpr Channel kValue = Channel::W; ///< The last enumerator.
};
} // namespace detail

/** @brief The transfer curve applied before brightness (convenience, not a chip feature). */
enum class Gamma : uint8_t {
  None,    ///< Identity.
  Gamma22, ///< `round(255 * (v / 255) ^ 2.2)` from a committed table.
};

namespace detail {
/// The last enumerator of `Gamma`, for `IsEnumerator()`.
template <>
struct EnumLast<Gamma> {
  static constexpr Gamma kValue = Gamma::Gamma22; ///< The last enumerator.
};
} // namespace detail

/** @brief A pixel colour, 8 bit per channel; `w` is used only by four-channel orders. */
struct Colour {
  uint8_t r{0}; ///< Red.
  uint8_t g{0}; ///< Green.
  uint8_t b{0}; ///< Blue.
  uint8_t w{0}; ///< White.

  /** @brief Hidden friend: exact member comparison. */
  friend constexpr bool operator==(const Colour& lhs, const Colour& rhs) noexcept = default;

  /**
   * @brief A colour from `0xRRGGBB`.
   * @param rgb Packed colour; the top byte is ignored.
   * @return The colour, white 0.
   */
  [[nodiscard]] static constexpr Colour FromPacked(uint32_t rgb) noexcept {
    return Colour{.r = static_cast<uint8_t>((rgb >> 16U) & 0xFFU),
                  .g = static_cast<uint8_t>((rgb >> 8U) & 0xFFU),
                  .b = static_cast<uint8_t>(rgb & 0xFFU),
                  .w = 0};
  }
  /**
   * @brief A colour from `0xWWRRGGBB`.
   * @param wrgb Packed colour with the white byte on top.
   * @return The colour.
   */
  [[nodiscard]] static constexpr Colour FromPackedW(uint32_t wrgb) noexcept {
    return Colour{.r = static_cast<uint8_t>((wrgb >> 16U) & 0xFFU),
                  .g = static_cast<uint8_t>((wrgb >> 8U) & 0xFFU),
                  .b = static_cast<uint8_t>(wrgb & 0xFFU),
                  .w = static_cast<uint8_t>((wrgb >> 24U) & 0xFFU)};
  }
  /** @return `0xRRGGBB`, white ignored. */
  [[nodiscard]] constexpr uint32_t Packed() const noexcept {
    return (static_cast<uint32_t>(r) << 16U) | (static_cast<uint32_t>(g) << 8U) |
           static_cast<uint32_t>(b);
  }
};

/** @brief The order of the channels on the wire, first sent first. */
struct ColourOrder {
  std::array<Channel, 4> ch{Channel::G, Channel::R, Channel::B,
                            Channel::W}; ///< Channels in wire order.
  uint8_t count{3};                      ///< 3 or 4 channels used.

  /** @brief Hidden friend: exact member comparison. */
  friend constexpr bool operator==(const ColourOrder& lhs,
                                   const ColourOrder& rhs) noexcept = default;
};

/** @brief Green, red, blue: the wire order of every WS2812 and SK6812 RGB part. */
inline constexpr ColourOrder kOrderGrb{.ch = {Channel::G, Channel::R, Channel::B, Channel::W},
                                       .count = 3};
/** @brief Red, green, blue. */
inline constexpr ColourOrder kOrderRgb{.ch = {Channel::R, Channel::G, Channel::B, Channel::W},
                                       .count = 3};
/** @brief Green, red, blue, white: SK6812RGBW Rev 07 (the default for `Variant::Sk6812Rgbw`). */
inline constexpr ColourOrder kOrderGrbw{.ch = {Channel::G, Channel::R, Channel::B, Channel::W},
                                        .count = 4};
/** @brief Red, green, blue, white: SK6812RGBW Rev 01 and Rev 03. */
inline constexpr ColourOrder kOrderRgbw{.ch = {Channel::R, Channel::G, Channel::B, Channel::W},
                                        .count = 4};

/** @brief A timing window in nanoseconds, with the value the driver aims for. */
struct Range {
  uint32_t min_ns{0};     ///< Inclusive lower bound.
  uint32_t max_ns{0};     ///< Inclusive upper bound; 0 means the document gives none.
  uint32_t nominal_ns{0}; ///< The target inside the window.

  /** @brief Hidden friend: exact member comparison. */
  friend constexpr bool operator==(const Range& lhs, const Range& rhs) noexcept = default;
};

/**
 * @brief The wire timing of one window set.
 *
 * A bit is a high of `t0h` or `t1h` followed by a low of `t0l` or `t1l`. Windows and the period
 * minimum are inclusive; the reset is strictly above `reset_min_ns` (docs/timing.md, ambiguity
 * A-4).
 */
struct Timing {
  Range t0h{};               ///< 0 code, high time.
  Range t0l{};               ///< 0 code, low time.
  Range t1h{};               ///< 1 code, high time.
  Range t1l{};               ///< 1 code, low time.
  uint32_t period_min_ns{0}; ///< Shortest bit period (high plus low); 0 none.
  uint32_t period_max_ns{0}; ///< Longest bit period; 0 none.
  uint32_t reset_min_ns{0};  ///< The low time that must be exceeded to latch.
  uint32_t max_low_ns{0}; ///< The low time that must stay below inside a frame; 0 = `reset_min_ns`.

  /** @brief Hidden friend: exact member comparison. */
  friend constexpr bool operator==(const Timing& lhs, const Timing& rhs) noexcept = default;
};

/** @brief Longest single high or low duration a `Custom` timing may name, in ns (covers 400
 * kbit/s). */
inline constexpr uint32_t kMaxDurationNs = 100'000;
/** @brief Longest period a `Custom` timing may name, in ns. */
inline constexpr uint32_t kMaxPeriodNs = 200'000;
/** @brief Longest reset a timing or `Config::reset_us` may name, in microseconds. */
inline constexpr uint32_t kMaxResetUs = 10'000;
/** @brief Added to the datasheet reset minimum when `Config::reset_us` is 0, in microseconds (A-3).
 */
inline constexpr uint32_t kResetMarginUs = 20;
/** @brief Largest strip the driver accepts, in pixels. */
inline constexpr size_t kMaxPixels = 65'536;
/** @brief Longest power-on settle `Config` may ask for, in microseconds. */
inline constexpr uint32_t kMaxSettleUs = 1'000'000;
/** @brief Longest transmit slack `Config` may ask for, in milliseconds. */
inline constexpr uint32_t kMaxSlackMs = 60'000;
/** @brief The highest tick rate the driver accepts from a port, in Hz. */
inline constexpr uint32_t kMaxResolutionHz = 1'000'000'000;

/** @brief Supplies above this count as 6 V in `MinLogicHighMv()` (the absolute maximum is 5.3 V).
 */
inline constexpr uint32_t kMaxSupplyMv = 6000;
/** @brief The fixed floor of VIH of the V5 family, in millivolts (V5.0, datasheet p.2); later
 * revisions print a fraction of VDD instead (`MinLogicHighMv()`). */
inline constexpr uint32_t kV5MinHighMv = 2700;
/** @brief The 3.4 V input high level of the SK6812 family at 5 V, in millivolts: the minimum of
 * SK6812RGBW Rev 01 and Rev 03 (p.4), the typical value of SK6812 RGB Rev 01 (p.4, which prints no
 * minimum). SK6812RGBX Rev 07 prints 0.7 x VDD instead (p.6); `MinLogicHighMv()` takes the larger.
 */
inline constexpr uint32_t kSk6812MinHighMv = 3400;

/** @brief What the port needs to send the frame, in ticks of `resolution_hz`. */
struct Waveform {
  uint32_t resolution_hz{0}; ///< The tick rate the port reported.
  uint32_t t0h_ticks{0};     ///< 0 code, high.
  uint32_t t0l_ticks{0};     ///< 0 code, low.
  uint32_t t1h_ticks{0};     ///< 1 code, high.
  uint32_t t1l_ticks{0};     ///< 1 code, low.
  uint32_t reset_ticks{0};   ///< Line low after the last bit.
  uint32_t max_gap_ticks{0}; ///< The longest low the pixels tolerate inside a frame.

  /** @brief Hidden friend: exact member comparison. */
  friend constexpr bool operator==(const Waveform& lhs, const Waveform& rhs) noexcept = default;
};

/**
 * @brief A timing window from its three numbers.
 * @param min_ns Inclusive lower bound.
 * @param max_ns Inclusive upper bound, 0 for none.
 * @param nominal_ns The target inside the window.
 * @return The window.
 */
[[nodiscard]] constexpr Range MakeRange(uint32_t min_ns, uint32_t max_ns,
                                        uint32_t nominal_ns) noexcept {
  return Range{.min_ns = min_ns, .max_ns = max_ns, .nominal_ns = nominal_ns};
}

/// The four limits of a `Timing` (a parameter group of `MakeTiming()`).
struct Limits {
  uint32_t period_min_ns; ///< Shortest bit period, 0 none.
  uint32_t period_max_ns; ///< Longest bit period, 0 none.
  uint32_t reset_min_ns;  ///< The low time that must be exceeded to latch.
  uint32_t max_low_ns; ///< The low time that must stay below inside a frame, 0 for `reset_min_ns`.
};

/**
 * @brief The limits of a timing from their four numbers.
 * @param period_min_ns Shortest bit period, 0 none.
 * @param period_max_ns Longest bit period, 0 none.
 * @param reset_min_ns The low time that must be exceeded to latch.
 * @param max_low_ns The low time that must stay below inside a frame, 0 for `reset_min_ns`.
 * @return The limits.
 */
[[nodiscard]] constexpr Limits MakeLimits(uint32_t period_min_ns, uint32_t period_max_ns,
                                          uint32_t reset_min_ns, uint32_t max_low_ns) noexcept {
  return Limits{.period_min_ns = period_min_ns,
                .period_max_ns = period_max_ns,
                .reset_min_ns = reset_min_ns,
                .max_low_ns = max_low_ns};
}

/**
 * @brief A timing from its four windows and its limits.
 * @param t0h 0 code, high time.
 * @param t0l 0 code, low time.
 * @param t1h 1 code, high time.
 * @param t1l 1 code, low time.
 * @param limits The period, reset and low-time limits.
 * @return The timing.
 */
[[nodiscard]] constexpr Timing MakeTiming(const Range& t0h, const Range& t0l, const Range& t1h,
                                          const Range& t1l, const Limits& limits) noexcept {
  return Timing{.t0h = t0h,
                .t0l = t0l,
                .t1h = t1h,
                .t1l = t1l,
                .period_min_ns = limits.period_min_ns,
                .period_max_ns = limits.period_max_ns,
                .reset_min_ns = limits.reset_min_ns,
                .max_low_ns = limits.max_low_ns};
}

/**
 * @brief The window set of a named variant, as nanoseconds. Values are the intersection of the
 * windows of every document of that variant (docs/timing.md, section "Profiles").
 * @param variant A named variant; `Custom` returns an all-zero timing.
 * @return The timing.
 */
[[nodiscard]] constexpr Timing ProfileTiming(Variant variant) noexcept {
  switch (variant) {
  case Variant::Ws2812: // DS ws2812 p.4
    return ws2812_rmt::MakeTiming(
        ws2812_rmt::MakeRange(200U, 500U, 350U), ws2812_rmt::MakeRange(650U, 950U, 800U),
        ws2812_rmt::MakeRange(550U, 850U, 700U), ws2812_rmt::MakeRange(450U, 750U, 600U),
        ws2812_rmt::MakeLimits(650U, 1850U, 50'000U, 0U));
  case Variant::Ws2812bPreV5: // DS b-af p.3, b-cn p.3, ws2812s p.4, b-mar17 p.4
    return ws2812_rmt::MakeTiming(
        ws2812_rmt::MakeRange(250U, 380U, 300U), ws2812_rmt::MakeRange(750U, 1000U, 850U),
        ws2812_rmt::MakeRange(750U, 950U, 800U), ws2812_rmt::MakeRange(300U, 420U, 350U),
        ws2812_rmt::MakeLimits(950U, 1550U, 300'000U, 50'000U));
  case Variant::Ws2812bV5: // DS b-v5 p.3, b-v5-w p.3, b-v6-h, b-v6 p.4, b-2020 p.4, c-2020 p.4,
                           // b-1313 p.3
    // t0h aims at 350, not the middle of its window (300): with t0l 950 the period would sit
    // exactly on the 1250 ns minimum of the V6 sheets
    return ws2812_rmt::MakeTiming(
        ws2812_rmt::MakeRange(220U, 380U, 350U), ws2812_rmt::MakeRange(580U, 1000U, 950U),
        ws2812_rmt::MakeRange(580U, 1000U, 650U), ws2812_rmt::MakeRange(580U, 1000U, 650U),
        ws2812_rmt::MakeLimits(1250U, 0U, 280'000U, 0U));
  case Variant::Ws2812c: // DS c-a p.3, c-b p.3, ws-2020 p.3
    return ws2812_rmt::MakeTiming(
        ws2812_rmt::MakeRange(220U, 380U, 300U), ws2812_rmt::MakeRange(580U, 1000U, 900U),
        ws2812_rmt::MakeRange(580U, 1000U, 700U), ws2812_rmt::MakeRange(220U, 420U, 350U),
        ws2812_rmt::MakeLimits(0U, 0U, 280'000U, 0U));
  case Variant::Ws2812d: // DS d-f8 p.4
    return ws2812_rmt::MakeTiming(
        ws2812_rmt::MakeRange(220U, 380U, 300U), ws2812_rmt::MakeRange(750U, 1000U, 900U),
        ws2812_rmt::MakeRange(750U, 1000U, 850U), ws2812_rmt::MakeRange(220U, 380U, 300U),
        ws2812_rmt::MakeLimits(0U, 0U, 280'000U, 0U));
  case Variant::Sk6812: // DS sk-01 p.5
    return ws2812_rmt::MakeTiming(
        ws2812_rmt::MakeRange(150U, 450U, 300U), ws2812_rmt::MakeRange(750U, 1050U, 900U),
        ws2812_rmt::MakeRange(450U, 750U, 600U), ws2812_rmt::MakeRange(450U, 750U, 600U),
        ws2812_rmt::MakeLimits(650U, 1850U, 80'000U, 50'000U));
  case Variant::Sk6812Rgbw: // DS skw-01 p.5, skw-03 p.5, skw-07 p.7
    return ws2812_rmt::MakeTiming(
        ws2812_rmt::MakeRange(200U, 400U, 300U), ws2812_rmt::MakeRange(800U, 1050U, 950U),
        ws2812_rmt::MakeRange(580U, 750U, 650U), ws2812_rmt::MakeRange(450U, 750U, 600U),
        ws2812_rmt::MakeLimits(1200U, 1850U, 80'000U, 20'000U));
  case Variant::Sk6812Mini: // DS skm-e p.7, skm p.3
    return ws2812_rmt::MakeTiming(
        ws2812_rmt::MakeRange(200U, 400U, 320U), ws2812_rmt::MakeRange(800U, 0U, 950U),
        ws2812_rmt::MakeRange(580U, 1000U, 650U), ws2812_rmt::MakeRange(200U, 0U, 600U),
        ws2812_rmt::MakeLimits(1200U, 0U, 80'000U, 20'000U));
  case Variant::Custom:
    break;
  default:
    break;
  }
  return Timing{};
}

/**
 * @brief The colour order a named variant uses unless `Config` overrides it.
 * @param variant The variant.
 * @return GRB for the RGB parts, GRBW for `Sk6812Rgbw`; a zero-count order for `Custom`.
 */
[[nodiscard]] constexpr ColourOrder DefaultOrder(Variant variant) noexcept {
  switch (variant) {
  case Variant::Ws2812:
  case Variant::Ws2812bPreV5:
  case Variant::Ws2812bV5:
  case Variant::Ws2812c:
  case Variant::Ws2812d:
  case Variant::Sk6812:
  case Variant::Sk6812Mini:
    return kOrderGrb;
  case Variant::Sk6812Rgbw:
    return kOrderGrbw;
  case Variant::Custom:
    break;
  default:
    break;
  }
  return ColourOrder{.ch = {Channel::R, Channel::G, Channel::B, Channel::W}, .count = 0};
}

/** @brief Everything `EnsureInitialized()` uses. The defaults suit a current WS2812B (V5, V6). */
struct Config {
  Variant variant{Variant::Ws2812bV5}; ///< The timing generation; no profile fits every part.
  bool use_default_order{true};        ///< False: `order` is used. Always false for `Custom`.
  ColourOrder order{};                 ///< Wire order when `use_default_order` is false.
  uint8_t brightness{255};             ///< Linear dimming at start, 0 to 255.
  Gamma gamma{Gamma::None};            ///< Transfer curve at start.
  uint32_t pixel_count{0};             ///< Pixels in the strip; 0 means the whole capacity.
  uint32_t reset_us{0}; ///< Reset low time; 0 = datasheet minimum plus `kResetMarginUs`.
  uint32_t power_on_settle_us{1000}; ///< Wait after `PowerOn()` (no datasheet value, A-16).
  uint32_t transmit_slack_ms{20};    ///< Added to the frame time to form the port timeout.
  Timing custom{};                   ///< Used only with `Variant::Custom`; all zero otherwise.

  /** @return The default configuration. */
  [[nodiscard]] static constexpr Config Default() noexcept {
    return Config{};
  }
};

/** @return The driver version, e.g. `"0.1.0"`. No I/O. */
constexpr const char* GetDriverVersion() noexcept {
  return WS2812_RMT_VERSION_STRING;
}
/** @return The major version. No I/O. */
constexpr uint32_t GetDriverVersionMajor() noexcept {
  return WS2812_RMT_VERSION_MAJOR;
}
/** @return The minor version. No I/O. */
constexpr uint32_t GetDriverVersionMinor() noexcept {
  return WS2812_RMT_VERSION_MINOR;
}
/** @return The patch version. No I/O. */
constexpr uint32_t GetDriverVersionPatch() noexcept {
  return WS2812_RMT_VERSION_PATCH;
}

/** @} */

} // namespace ws2812_rmt

#endif // WS2812_RMT_TYPES_HPP
