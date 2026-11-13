/**
 * @file ws2812_rmt_functions.hpp
 * @brief The pure functions of the WS2812_RMT driver: colour scaling, gamma,
 * pixel encoding, the waveform resolver, frame arithmetic and `Validate()`.
 * @copyright Copyright (c) 2026 The driver-ws2812-rmt authors.
 * SPDX-License-Identifier: MIT
 *
 * @details
 * Everything here is `constexpr`, `noexcept` and free of state: it is tested
 * exhaustively or against an independent oracle (tests/), and the driver class
 * only calls it. No floating point, no allocation.
 */
#ifndef WS2812_RMT_FUNCTIONS_HPP
#define WS2812_RMT_FUNCTIONS_HPP

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "ws2812_rmt_detail.hpp"
#include "ws2812_rmt_gamma.hpp"
#include "ws2812_rmt_types.hpp"

namespace ws2812_rmt {

/**
 * @defgroup ws2812_rmt_functions Pure functions
 * @brief Colour maths, encoding, the waveform resolver and frame arithmetic.
 * @{
 */

/// Nanoseconds in a second, for the integer tick arithmetic.
inline constexpr uint64_t kNsPerSecond = 1'000'000'000ULL;

/**
 * @brief `round(v * b / 255)`, halves up: exact at `b = 255` (identity) and `b
 * = 0` (zero), monotone in both arguments, at most half a least significant bit
 * of error. (The old truncating form differed by at most one.)
 * @param v The channel value.
 * @param b The brightness.
 * @return The scaled value.
 */
[[nodiscard]] constexpr uint8_t Scale8(uint8_t v, uint8_t b) noexcept {
  const uint32_t product = static_cast<uint32_t>(v) * static_cast<uint32_t>(b);
  return static_cast<uint8_t>((product + 127U) / 255U);
}

/**
 * @brief The transfer curve of a channel value.
 * @param v The value.
 * @param gamma The curve.
 * @return `v` for `Gamma::None`, the table entry for `Gamma::Gamma22`.
 */
[[nodiscard]] constexpr uint8_t ApplyGamma(uint8_t v, Gamma gamma) noexcept {
  switch (gamma) {
  case Gamma::None:
    return v;
  case Gamma::Gamma22:
    return kGamma22[v];
  default:
    break;
  }
  return v;
}

/**
 * @brief One channel of a colour.
 * @param colour The colour.
 * @param channel The channel.
 * @return Its value.
 */
[[nodiscard]] constexpr uint8_t ChannelOf(const Colour& colour, Channel channel) noexcept {
  switch (channel) {
  case Channel::R:
    return colour.r;
  case Channel::G:
    return colour.g;
  case Channel::B:
    return colour.b;
  case Channel::W:
    return colour.w;
  default:
    break;
  }
  return 0U;
}

/**
 * @brief The wire value of a channel: gamma first, then brightness (linear
 * dimming of the corrected value).
 * @param v The logical value.
 * @param brightness Linear dimming.
 * @param gamma The curve.
 * @return `Scale8(ApplyGamma(v, gamma), brightness)`.
 */
[[nodiscard]] constexpr uint8_t WireValue(uint8_t v, uint8_t brightness, Gamma gamma) noexcept {
  return Scale8(ApplyGamma(v, gamma), brightness);
}

/**
 * @brief Whether `order` is usable: a count of 3 (distinct channels among R, G,
 * B) or 4 (all four).
 * @param order The order.
 * @return true when valid.
 */
[[nodiscard]] constexpr bool IsValidOrder(const ColourOrder& order) noexcept {
  if ((order.count != 3U) && (order.count != 4U)) {
    return false;
  }
  uint32_t seen = 0U;
  for (uint32_t i = 0U; i < static_cast<uint32_t>(order.count); ++i) {
    const Channel c = order.ch[i];
    if (!detail::IsEnumerator(c)) {
      return false;
    }
    const uint32_t bit = uint32_t{1} << (static_cast<uint32_t>(detail::ToUnderlying(c)) & 31U);
    if (((seen & bit) != 0U) || ((order.count == 3U) && (c == Channel::W))) {
      return false;
    }
    seen |= bit;
  }
  return true;
}

/**
 * @brief Encode one pixel into wire order.
 * @param colour The logical colour; `w` is ignored for a three-channel order.
 * @param order The wire order.
 * @param brightness Linear dimming.
 * @param gamma The curve.
 * @param out Receives `order.count` bytes.
 * @return The bytes written; 0 when `order` is invalid or `out` is too small
 * (nothing is written).
 */
[[nodiscard]] constexpr size_t EncodePixel(const Colour& colour, const ColourOrder& order,
                                           uint8_t brightness, Gamma gamma,
                                           std::span<uint8_t> out) noexcept {
  if (!IsValidOrder(order) || (out.size() < static_cast<size_t>(order.count))) {
    return 0U;
  }
  for (size_t i = 0U; i < static_cast<size_t>(order.count); ++i) {
    out[i] = WireValue(ChannelOf(colour, order.ch[i]), brightness, gamma);
  }
  return static_cast<size_t>(order.count);
}

/**
 * @brief Encode a whole strip, pixel 0 first.
 * @param pixels The logical colours.
 * @param order The wire order.
 * @param brightness Linear dimming.
 * @param gamma The curve.
 * @param out Receives `pixels.size() x order.count` bytes.
 * @return The bytes written; 0 when `order` is invalid or `out` is too small
 * (nothing is written).
 */
[[nodiscard]] constexpr size_t EncodeFrame(std::span<const Colour> pixels, const ColourOrder& order,
                                           uint8_t brightness, Gamma gamma,
                                           std::span<uint8_t> out) noexcept {
  if (!IsValidOrder(order)) {
    return 0U;
  }
  const auto width = static_cast<size_t>(order.count);
  if (out.size() < (pixels.size() * width)) {
    return 0U;
  }
  size_t offset = 0U;
  for (const Colour& colour : pixels) {
    offset += EncodePixel(colour, order, brightness, gamma, out.subspan(offset, width));
  }
  return offset;
}

/**
 * @brief The inverse of `EncodeFrame()` at brightness 255 and no gamma, for
 * models and tests.
 * @param wire Bytes in wire order.
 * @param order The wire order.
 * @param out Receives one colour per complete pixel of `wire`; unused channels
 * stay 0.
 * @return The pixels written; 0 when `order` is invalid.
 */
[[nodiscard]] constexpr size_t DecodeWire(std::span<const uint8_t> wire, const ColourOrder& order,
                                          std::span<Colour> out) noexcept {
  if (!IsValidOrder(order)) {
    return 0U;
  }
  const size_t width = (order.count == 4U) ? 4U : 3U; // IsValidOrder() allowed only 3 or 4
  const size_t pixels = std::min(wire.size() / width, out.size());
  for (size_t p = 0U; p < pixels; ++p) {
    Colour colour{};
    for (size_t i = 0U; i < width; ++i) {
      const uint8_t v = wire[(p * width) + i];
      switch (order.ch[i]) {
      case Channel::R:
        colour.r = v;
        break;
      case Channel::G:
        colour.g = v;
        break;
      case Channel::B:
        colour.b = v;
        break;
      case Channel::W:
        colour.w = v;
        break;
      default:
        break;
      }
    }
    out[p] = colour;
  }
  return pixels;
}

/**
 * @brief Move the common part of red, green and blue into the white channel,
 * for an RGBW strip that should show an RGB colour with the white die. Never
 * applied implicitly.
 * @param colour An RGB colour (`w` is replaced).
 * @return `w = min(r, g, b)`, subtracted from the other three.
 */
[[nodiscard]] constexpr Colour ExtractWhite(const Colour& colour) noexcept {
  const uint8_t white = std::min({colour.r, colour.g, colour.b});
  return Colour{.r = static_cast<uint8_t>(colour.r - white),
                .g = static_cast<uint8_t>(colour.g - white),
                .b = static_cast<uint8_t>(colour.b - white),
                .w = white};
}

/**
 * @brief A colour wheel: red to green to blue to red in three segments of 85
 * steps.
 * @param position 0 to 255.
 * @return A fully saturated colour.
 */
[[nodiscard]] constexpr Colour Wheel(uint8_t position) noexcept {
  const uint32_t pos = position;
  if (pos < 85U) {
    return Colour{.r = static_cast<uint8_t>(255U - (pos * 3U)),
                  .g = static_cast<uint8_t>(pos * 3U),
                  .b = 0,
                  .w = 0};
  }
  if (pos < 170U) {
    const uint32_t p = pos - 85U;
    return Colour{.r = 0,
                  .g = static_cast<uint8_t>(255U - (p * 3U)),
                  .b = static_cast<uint8_t>(p * 3U),
                  .w = 0};
  }
  const uint32_t p = pos - 170U;
  return Colour{.r = static_cast<uint8_t>(p * 3U),
                .g = 0,
                .b = static_cast<uint8_t>(255U - (p * 3U)),
                .w = 0};
}

/**
 * @brief `round(ns * hz / 1e9)`, halves up, in 64 bits (ns and hz below 2^32
 * cannot overflow).
 * @param ns A duration in nanoseconds.
 * @param hz A tick rate.
 * @return The nearest tick count.
 */
[[nodiscard]] constexpr uint64_t NsToTicks(uint64_t ns, uint64_t hz) noexcept {
  return ((ns * hz) + (kNsPerSecond / 2U)) / kNsPerSecond;
}

namespace detail {

/// `ceil(ns * hz / 1e9)`: the fewest ticks that last at least `ns`.
[[nodiscard]] constexpr uint64_t CeilTicks(uint64_t ns, uint64_t hz) noexcept {
  return ((ns * hz) + (kNsPerSecond - 1U)) / kNsPerSecond;
}

/// `floor(ns * hz / 1e9)`: the most ticks that last at most `ns`.
[[nodiscard]] constexpr uint64_t FloorTicks(uint64_t ns, uint64_t hz) noexcept {
  return (ns * hz) / kNsPerSecond;
}

/// The tick range a duration may take.
struct TickWindow {
  uint64_t lo{0}; ///< Fewest ticks.
  uint64_t hi{0}; ///< Most ticks.
};

/// The ticks of `r` at `hz`; an unbounded maximum becomes the largest count
/// strictly below `cap_ns`.
[[nodiscard]] constexpr TickWindow WindowOf(const Range& r, uint64_t hz, uint64_t cap_ns) noexcept {
  TickWindow w{.lo = CeilTicks(r.min_ns, hz), .hi = 0U};
  const uint64_t cap_hi = CeilTicks(cap_ns, hz) - 1U;
  w.hi = (r.max_ns == 0U) ? cap_hi : std::min(FloorTicks(r.max_ns, hz), cap_hi);
  return w;
}

/// `ticks` limited to the window, the aim being `nominal_ns`.
[[nodiscard]] constexpr uint64_t Aim(const Range& r, const TickWindow& w, uint64_t hz) noexcept {
  return std::clamp(NsToTicks(r.nominal_ns, hz), w.lo, w.hi);
}

/// What `Timing` allows for a single duration.
[[nodiscard]] constexpr bool RangeValid(const Range& r) noexcept {
  const bool bounded_ok =
      (r.max_ns == 0U) || ((r.max_ns >= r.min_ns) && (r.max_ns <= kMaxDurationNs));
  const bool nominal_ok = (r.nominal_ns >= r.min_ns) && (r.nominal_ns <= kMaxDurationNs) &&
                          ((r.max_ns == 0U) || (r.nominal_ns <= r.max_ns));
  return (r.min_ns > 0U) && bounded_ok && nominal_ok;
}

/// A timing with consistent windows, period and reset; checked for `Custom` and
/// by the resolver.
[[nodiscard]] constexpr bool TimingValid(const Timing& t) noexcept {
  if (!RangeValid(t.t0h) || !RangeValid(t.t0l) || !RangeValid(t.t1h) || !RangeValid(t.t1l)) {
    return false;
  }
  const bool period_ok =
      (t.period_min_ns <= kMaxPeriodNs) &&
      ((t.period_max_ns == 0U) ||
       ((t.period_max_ns >= t.period_min_ns) && (t.period_max_ns <= kMaxPeriodNs) &&
        ((t.t0h.min_ns + t.t0l.min_ns) <= t.period_max_ns) &&
        ((t.t1h.min_ns + t.t1l.min_ns) <= t.period_max_ns)));
  const uint64_t reset_cap = static_cast<uint64_t>(kMaxResetUs) * 1000U;
  return period_ok && (t.reset_min_ns > 0U) && (t.reset_min_ns <= reset_cap) &&
         (t.max_low_ns <= t.reset_min_ns);
}

/// The tick rate and the low-time cap a bit is resolved against.
struct TickBase {
  uint64_t hz;     ///< Ticks per second.
  uint64_t cap_ns; ///< Strict upper bound of a low time when its window has no
                   ///< maximum.
};

/// The ticks of one bit value: a high and a low inside their windows and the
/// period rules.
[[nodiscard]] constexpr bool ResolveBit(const Range& high, const Range& low, const Timing& t,
                                        const TickBase& base, uint32_t& th, uint32_t& tl) noexcept {
  const uint64_t hz = base.hz;
  const uint64_t cap_ns = base.cap_ns;
  const TickWindow hw = WindowOf(high, hz, cap_ns);
  const TickWindow lw = WindowOf(low, hz, cap_ns);
  if ((hw.lo > hw.hi) || (lw.lo > lw.hi) || (hw.hi > 0xFFFF'FFFFULL) || (lw.hi > 0xFFFF'FFFFULL)) {
    return false;
  }
  const uint64_t h = Aim(high, hw, hz);
  uint64_t l = Aim(low, lw, hz);
  const uint64_t need = CeilTicks(t.period_min_ns, hz);
  if ((h + l) < need) {
    l = need - h;
  }
  const bool too_long =
      (t.period_max_ns != 0U) && (((h + l) * kNsPerSecond) > (t.period_max_ns * hz));
  if ((l > lw.hi) || too_long) {
    return false;
  }
  th = static_cast<uint32_t>(h);
  tl = static_cast<uint32_t>(l);
  return true;
}

/// Whether `ticks` lasts inside `r` (inclusive); an unbounded maximum is
/// strictly below `cap_ns` when `cap_ns` is non-zero (a low time) and unlimited
/// when it is zero (a high time).
[[nodiscard]] constexpr bool InWindow(const Range& r, uint32_t ticks, uint64_t hz,
                                      uint64_t cap_ns) noexcept {
  const uint64_t scaled = static_cast<uint64_t>(ticks) * kNsPerSecond;
  const bool above_min = scaled >= (static_cast<uint64_t>(r.min_ns) * hz);
  bool below_max = true;
  if (r.max_ns != 0U) {
    below_max = scaled <= (static_cast<uint64_t>(r.max_ns) * hz);
  } else if (cap_ns != 0U) {
    below_max = scaled < (cap_ns * hz);
  } else {
    // No upper bound and no cap: nothing to check.
  }
  return above_min && below_max;
}

} // namespace detail

/**
 * @brief Turn a timing into ticks at the port's real tick rate.
 *
 * Each of the four durations gets the tick count nearest its nominal that lies
 * inside its window (windows inclusive); a bit period below `period_min_ns` is
 * repaired by lengthening the low, and a period above `period_max_ns` or an
 * empty window is `TimingUnreachable`. The reset is `reset_us` (0: the minimum
 * plus `kResetMarginUs`), never at or below the minimum, which is a strict
 * bound.
 *
 * @param timing A valid timing (see `Config::custom`).
 * @param resolution_hz The tick rate the port reported, 1 to
 * `kMaxResolutionHz`.
 * @param reset_us The reset low time, 0 for the default, at most `kMaxResetUs`.
 * @return The waveform, `TimingUnreachable` when no tick count fits (or the
 * arguments are out of range).
 */
[[nodiscard]] constexpr Result<Waveform> ResolveWaveform(const Timing& timing,
                                                         uint32_t resolution_hz,
                                                         uint32_t reset_us) noexcept {
  using Failure = Result<Waveform>;
  if ((resolution_hz == 0U) || (resolution_hz > kMaxResolutionHz) || (reset_us > kMaxResetUs) ||
      !detail::TimingValid(timing)) {
    return Failure::Failure(Error::TimingUnreachable);
  }
  const uint64_t hz = resolution_hz;
  const auto cap_ns =
      static_cast<uint64_t>((timing.max_low_ns != 0U) ? timing.max_low_ns : timing.reset_min_ns);
  Waveform w{};
  w.resolution_hz = resolution_hz;
  const detail::TickBase base{.hz = hz, .cap_ns = cap_ns};
  if (!detail::ResolveBit(timing.t0h, timing.t0l, timing, base, w.t0h_ticks, w.t0l_ticks) ||
      !detail::ResolveBit(timing.t1h, timing.t1l, timing, base, w.t1h_ticks, w.t1l_ticks)) {
    return Failure::Failure(Error::TimingUnreachable);
  }
  const uint64_t default_us =
      ((static_cast<uint64_t>(timing.reset_min_ns) + 999U) / 1000U) + kResetMarginUs;
  const uint64_t wanted_ns =
      ((reset_us == 0U) ? default_us : static_cast<uint64_t>(reset_us)) * 1000U;
  const uint64_t floor_ticks = detail::FloorTicks(timing.reset_min_ns, hz) + 1U;
  w.reset_ticks = static_cast<uint32_t>(std::max(NsToTicks(wanted_ns, hz), floor_ticks));
  // The longest low inside a frame: the smallest latch time any document of the variant prints,
  // less 10 percent for the pixel's own threshold spread, and strictly below that.
  const uint64_t gap_ns = cap_ns - (cap_ns / 10U);
  w.max_gap_ticks = static_cast<uint32_t>(detail::CeilTicks(gap_ns, hz) - 1U);
  // The driver's own lows must obey the limit it hands the port.
  if ((w.t0l_ticks > w.max_gap_ticks) || (w.t1l_ticks > w.max_gap_ticks)) {
    return Failure::Failure(Error::TimingUnreachable);
  }
  return Failure::Success(w);
}

/**
 * @brief Convert ticks back to nanoseconds, rounding up (frame arithmetic,
 * never a window check).
 * @param ticks A tick count.
 * @param resolution_hz The tick rate, non-zero.
 * @return `ceil(ticks * 1e9 / hz)`; 0 when `resolution_hz` is 0.
 */
[[nodiscard]] constexpr uint64_t TicksToNs(uint64_t ticks, uint32_t resolution_hz) noexcept {
  if (resolution_hz == 0U) {
    return 0U;
  }
  return ((ticks * kNsPerSecond) + (static_cast<uint64_t>(resolution_hz) - 1U)) / resolution_hz;
}

/**
 * @brief The windows check of a waveform against a named profile, in exact
 * integer arithmetic: the mixed-chain and compatibility helper (is this
 * waveform legal for that variant).
 * @param variant A named variant; `Custom` has no profile and returns false.
 * @param wave The waveform.
 * @return true when every duration lies inside the profile's window, the bit
 * periods obey the period rules, and the reset is above the profile's minimum.
 */
[[nodiscard]] constexpr bool WaveformInWindow(Variant variant, const Waveform& wave) noexcept {
  if ((variant == Variant::Custom) || !detail::IsEnumerator(variant) ||
      (wave.resolution_hz == 0U)) {
    return false;
  }
  const Timing t = ProfileTiming(variant);
  const uint64_t hz = wave.resolution_hz;
  const auto cap_ns = static_cast<uint64_t>((t.max_low_ns != 0U) ? t.max_low_ns : t.reset_min_ns);
  const bool windows = detail::InWindow(t.t0h, wave.t0h_ticks, hz, 0U) &&
                       detail::InWindow(t.t0l, wave.t0l_ticks, hz, cap_ns) &&
                       detail::InWindow(t.t1h, wave.t1h_ticks, hz, 0U) &&
                       detail::InWindow(t.t1l, wave.t1l_ticks, hz, cap_ns);
  const uint64_t p0 = (static_cast<uint64_t>(wave.t0h_ticks) + wave.t0l_ticks) * kNsPerSecond;
  const uint64_t p1 = (static_cast<uint64_t>(wave.t1h_ticks) + wave.t1l_ticks) * kNsPerSecond;
  const uint64_t pmin = static_cast<uint64_t>(t.period_min_ns) * hz;
  const uint64_t pmax = static_cast<uint64_t>(t.period_max_ns) * hz;
  const bool periods_min = (p0 >= pmin) && (p1 >= pmin);
  const bool periods_max = (t.period_max_ns == 0U) || ((p0 <= pmax) && (p1 <= pmax));
  const bool reset_ok = (static_cast<uint64_t>(wave.reset_ticks) * kNsPerSecond) >
                        (static_cast<uint64_t>(t.reset_min_ns) * hz);
  return windows && periods_min && periods_max && reset_ok;
}

/**
 * @brief The number of bits in a frame.
 * @param pixels Pixels.
 * @param channels Channels per pixel (3 or 4).
 * @return `pixels x channels x 8`.
 */
[[nodiscard]] constexpr uint64_t FrameBits(size_t pixels, uint8_t channels) noexcept {
  return static_cast<uint64_t>(pixels) * static_cast<uint64_t>(channels) * 8U;
}

/**
 * @brief Worst-case time of a frame: every bit a one (or a zero, whichever is
 * longer), plus the reset.
 * @param pixels Pixels.
 * @param channels Channels per pixel.
 * @param wave The waveform.
 * @return Nanoseconds.
 */
[[nodiscard]] constexpr uint64_t FrameTimeNs(size_t pixels, uint8_t channels,
                                             const Waveform& wave) noexcept {
  const uint64_t zero = static_cast<uint64_t>(wave.t0h_ticks) + wave.t0l_ticks;
  const uint64_t one = static_cast<uint64_t>(wave.t1h_ticks) + wave.t1l_ticks;
  const uint64_t ticks = (FrameBits(pixels, channels) * std::max(zero, one)) + wave.reset_ticks;
  return TicksToNs(ticks, wave.resolution_hz);
}

/**
 * @brief The refresh rate the frame time allows.
 * @param pixels Pixels.
 * @param channels Channels per pixel.
 * @param wave The waveform.
 * @return Frames per second, rounded down; 0 when the frame time is 0.
 */
[[nodiscard]] constexpr uint32_t MaxRefreshHz(size_t pixels, uint8_t channels,
                                              const Waveform& wave) noexcept {
  const uint64_t ns = FrameTimeNs(pixels, channels, wave);
  return (ns == 0U) ? 0U : static_cast<uint32_t>(kNsPerSecond / ns);
}

/**
 * @brief The longest strip that still refreshes at `rate_hz`.
 * @param rate_hz Frames per second, non-zero.
 * @param channels Channels per pixel.
 * @param wave The waveform.
 * @return Pixels; 0 when the rate is 0 or the reset alone does not fit.
 */
[[nodiscard]] constexpr size_t MaxPixelsAtRate(uint32_t rate_hz, uint8_t channels,
                                               const Waveform& wave) noexcept {
  const uint64_t one_pixel =
      FrameTimeNs(size_t{1}, channels, wave) - FrameTimeNs(size_t{0}, channels, wave);
  const uint64_t budget = (rate_hz == 0U) ? 0U : (kNsPerSecond / rate_hz);
  const uint64_t reset = FrameTimeNs(size_t{0}, channels, wave);
  if ((one_pixel == 0U) || (budget <= reset)) {
    return 0U;
  }
  return static_cast<size_t>((budget - reset) / one_pixel);
}

/// The largest group and channel prescaler of the ESP32 RMT (ESP-IDF v5.5,
/// `RMT_LL_GROUP_CLOCK_MAX_INTEGER_PRESCALE` and `RMT_LL_CHANNEL_CLOCK_MAX_PRESCALE`).
inline constexpr uint32_t kRmtMaxPrescale = 256;

namespace detail {

/// The rate ESP-IDF v5.5 sets (`rmt_common.c`, `s_rmt_set_group_prescale()`): the first group
/// prescaler from 1 up whose rounded channel divider lies in 1 to 256; 0 when none does.
[[nodiscard]] constexpr uint32_t RmtRealRate(uint32_t source_hz, uint32_t requested_hz) noexcept {
  for (uint32_t group = 1U; group <= kRmtMaxPrescale; ++group) {
    const uint32_t group_hz = source_hz / group;
    const uint64_t divider =
        (static_cast<uint64_t>(group_hz) + static_cast<uint64_t>(requested_hz / 2U)) / requested_hz;
    if ((divider != 0U) && (divider <= kRmtMaxPrescale)) {
      return static_cast<uint32_t>(group_hz / divider);
    }
  }
  return 0U;
}

} // namespace detail

/**
 * @brief The tick rate an ESP32 RMT channel really runs at, computed as ESP-IDF v5.5 does
 * (`rmt_common.c`): the channel divider is rounded to the nearest integer, `(clock + request / 2)
 * / request`, so the real rate can be above or below the request. On the targets with a group
 * prescaler (all but the ESP32 and ESP32-S2) the group prescaler is the first from 1 up that brings
 * that divider into 1 to 256; on the ESP32 and ESP32-S2 the divider is the rounded one directly,
 * which gives the same rate whenever it is at most 256 (every request above `source_hz / 256`).
 * @param source_hz The source clock in hertz.
 * @param requested_hz The tick rate asked for.
 * @param tolerance_ppm The largest deviation accepted, in parts per million.
 * @return The real rate, or `TimingUnreachable` when a rate is 0, no divider reaches the request,
 * or the real rate is more than `tolerance_ppm` from the request.
 */
[[nodiscard]] constexpr Result<uint32_t> ResolveRmtRate(uint32_t source_hz, uint32_t requested_hz,
                                                        uint32_t tolerance_ppm = 1000U) noexcept {
  if ((requested_hz == 0U) || (source_hz == 0U)) {
    return Result<uint32_t>::Failure(Error::TimingUnreachable);
  }
  const uint32_t real = detail::RmtRealRate(source_hz, requested_hz);
  const uint64_t deviation = (real > requested_hz) ? static_cast<uint64_t>(real - requested_hz)
                                                   : static_cast<uint64_t>(requested_hz - real);
  if ((real == 0U) ||
      ((deviation * 1'000'000ULL) > (static_cast<uint64_t>(tolerance_ppm) * requested_hz))) {
    return Result<uint32_t>::Failure(Error::TimingUnreachable);
  }
  return Result<uint32_t>::Success(real);
}

/**
 * @brief The lowest logic-high level the part's DIN accepts at a supply
 * voltage, from the part's electrical table (informational: the driver cannot
 * see the supply; docs/hardware.md).
 * @param variant The variant; `Custom` returns the generic 0.7 x VDD.
 * @param vdd_mv The supply in millivolts.
 * @return Millivolts: 0.7 x VDD (older WS2812*, SK6812MINI), the larger of 3.4
 * V and 0.7 x VDD (SK6812RGBW: 3.4 V minimum in Rev 01 and Rev 03, 0.7 x VDD in
 * Rev 07; SK6812 RGB: its Rev 01 prints 3.4 V as a typical value only, and the
 * 0.7 x VDD is the driver's choice, the level of the other sheets of the
 * family), the larger of 2.7 V and 0.63 x VDD for
 * the V5 profile (the strictest of its documents: V6.0 and V6.1 print 0.63 x VDD,
 * V6 2025 0.55 x VDD, V5.0 2.7 V). A supply above `kMaxSupplyMv` counts as
 * `kMaxSupplyMv`.
 */
[[nodiscard]] constexpr uint32_t MinLogicHighMv(Variant variant, uint32_t vdd_mv) noexcept {
  const auto vdd = static_cast<uint64_t>(std::min(vdd_mv, kMaxSupplyMv));
  const auto seventy = static_cast<uint32_t>((vdd * 70U) / 100U);
  switch (variant) {
  case Variant::Ws2812bV5:
    // The strictest of the documents of the profile: V5.0 2.7 V fixed, V6.0 and V6.1 0.63 x VDD,
    // V6 (2025) 0.55 x VDD, 1313-V6 0.5 x VDD.
    return std::max(kV5MinHighMv, static_cast<uint32_t>((vdd * 63U) / 100U));
  case Variant::Sk6812:
  case Variant::Sk6812Rgbw:
    return std::max(kSk6812MinHighMv, seventy);
  case Variant::Ws2812:
  case Variant::Ws2812bPreV5:
  case Variant::Ws2812c:
  case Variant::Ws2812d:
  case Variant::Sk6812Mini:
  case Variant::Custom:
    return seventy;
  default:
    break;
  }
  return seventy;
}

/**
 * @brief The timing a configuration selects.
 * @param config The configuration.
 * @return `config.custom` for `Custom`, the profile otherwise.
 */
[[nodiscard]] constexpr Timing TimingOf(const Config& config) noexcept {
  return (config.variant == Variant::Custom) ? config.custom : ProfileTiming(config.variant);
}

/**
 * @brief The wire order a configuration selects.
 * @param config The configuration.
 * @return `config.order` unless `use_default_order` is set, then the variant's
 * default.
 */
[[nodiscard]] constexpr ColourOrder OrderOf(const Config& config) noexcept {
  return config.use_default_order ? DefaultOrder(config.variant) : config.order;
}

namespace detail {

/// Whether the colour order fits the variant and `reset_us` is above the
/// variant's minimum.
[[nodiscard]] constexpr bool OrderAndResetOk(const Config& config) noexcept {
  const bool custom = config.variant == Variant::Custom;
  const ColourOrder order = OrderOf(config);
  const bool order_ok =
      IsValidOrder(order) && (custom || (order.count == DefaultOrder(config.variant).count));
  const uint64_t reset_ns = static_cast<uint64_t>(config.reset_us) * 1000U;
  const bool reset_ok = (config.reset_us == 0U) || ((config.reset_us <= kMaxResetUs) &&
                                                    (reset_ns > TimingOf(config).reset_min_ns));
  return order_ok && reset_ok;
}

} // namespace detail

/**
 * @brief Check a configuration against a strip capacity without touching
 * hardware. Pure and `constexpr`, so a fixed configuration can be proved with
 * `static_assert`.
 *
 * Rejected: an unknown variant or gamma; a `custom` timing on a named variant,
 * or an inconsistent one on `Custom`; an order that is invalid, does not match
 * the variant's channel count, or is missing on `Custom`; a `reset_us` that is
 * not above the variant's minimum or above `kMaxResetUs`; a pixel count above
 * the capacity or a capacity of 0 or above `kMaxPixels`; a settle or slack
 * above its maximum.
 *
 * @param config The configuration.
 * @param capacity The pixel capacity of the driver object.
 * @return `Ok` or `InvalidArgument`.
 */
[[nodiscard]] constexpr Result<void> Validate(const Config& config, size_t capacity) noexcept {
  using Failure = Result<void>;
  if (!detail::IsEnumerator(config.variant) || !detail::IsEnumerator(config.gamma)) {
    return Failure::Failure(Error::InvalidArgument);
  }
  const bool custom = config.variant == Variant::Custom;
  if (custom ? !detail::TimingValid(config.custom) : !(config.custom == Timing{})) {
    return Failure::Failure(Error::InvalidArgument);
  }
  const bool count_ok =
      (capacity > 0U) && (capacity <= kMaxPixels) && (config.pixel_count <= capacity);
  const bool misc_ok =
      (config.power_on_settle_us <= kMaxSettleUs) && (config.transmit_slack_ms <= kMaxSlackMs);
  if (!(detail::OrderAndResetOk(config) && count_ok && misc_ok)) {
    return Failure::Failure(Error::InvalidArgument);
  }
  return Failure::Success();
}

/** @} */

} // namespace ws2812_rmt

#endif // WS2812_RMT_FUNCTIONS_HPP
