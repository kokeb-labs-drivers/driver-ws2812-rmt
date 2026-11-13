/**
 * @file timing.cpp
 * @brief The Timing page's examples, compiled by the host build (never run).
 */
#include "ws2812_rmt.hpp"

// --8<-- [start:waveform]
// What the driver does at start-up, by hand: nanoseconds into ticks at the port's real rate.
bool WaveformFor20Mhz() {
  const auto wave = ws2812_rmt::ResolveWaveform(
      ws2812_rmt::ProfileTiming(ws2812_rmt::Variant::Ws2812bV5), 20'000'000, 0);
  if (!wave) {
    return false; // TimingUnreachable
  }
  // A 20 MHz tick is 50 ns: T0H 7 ticks (350 ns), T0L 19, T1H 14, T1L 12, reset 6000 ticks (300
  // us).
  return (wave.value.t0h_ticks == 7U) && (wave.value.reset_ticks == 6000U);
}
// --8<-- [end:waveform]

// --8<-- [start:mixed]
// Two generations in one chain: is one waveform legal for both?
bool OneWaveformServesBoth() {
  const auto wave = ws2812_rmt::ResolveWaveform(
      ws2812_rmt::ProfileTiming(ws2812_rmt::Variant::Ws2812bV5), 20'000'000, 0);
  return wave && ws2812_rmt::WaveformInWindow(ws2812_rmt::Variant::Ws2812bV5, wave.value) &&
         ws2812_rmt::WaveformInWindow(ws2812_rmt::Variant::Ws2812bPreV5,
                                      wave.value); // false: use Custom
}
// --8<-- [end:mixed]

// --8<-- [start:frame]
// How many pixels fit in a 30 Hz refresh, and what a 60-pixel frame costs.
void FrameBudget() {
  const auto wave = ws2812_rmt::ResolveWaveform(
      ws2812_rmt::ProfileTiming(ws2812_rmt::Variant::Ws2812bV5), 20'000'000, 0);
  const size_t longest = ws2812_rmt::MaxPixelsAtRate(30, 3, wave.value); // 1058
  const uint64_t frame_ns = ws2812_rmt::FrameTimeNs(60, 3, wave.value);  // 2 172 000 ns
  const uint32_t level_mv =
      ws2812_rmt::MinLogicHighMv(ws2812_rmt::Variant::Ws2812bV5, 5000); // 2750
  static_cast<void>(longest);
  static_cast<void>(frame_ns);
  static_cast<void>(level_mv);
}
// --8<-- [end:frame]
