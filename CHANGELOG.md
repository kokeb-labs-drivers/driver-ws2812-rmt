# Changelog

All notable changes to this repository are listed here. Versions follow [Semantic Versioning](https://semver.org/).
Entries under **Breaking** need source changes in code that used the affected API; any of them calls for a major
version bump (a minor bump before 1.0).

## [Unreleased]

### Added

- WS2812_RMT driver, first version: the pixel and wire buffers of a strip with a fixed capacity, the byte encoder
  (colour order, brightness, optional gamma 2.2), and a waveform resolver that converts the datasheet windows of the
  chosen variant to ticks at the port's real rate and refuses a rate that cannot meet them.
- Variants `Ws2812`, `Ws2812bPreV5`, `Ws2812bV5` (default), `Ws2812c`, `Ws2812d`, `Sk6812`, `Sk6812Rgbw`, `Sk6812Mini`
  and `Custom`, each the intersection of the windows of the datasheets it covers (see `docs/datasheets.yml`).
- Port contract with four required hooks (`EnsureInitialized()`, `ResolutionHz()`, `Transmit()`, `DelayUs()`) and
  optional `GpioSet()` and `Log()`; `PowerOn()` and `PowerOff()` for a switched strip supply.
- Raw access: `TraceWire()` sends bytes as given, `GetLastWire()` shows the bytes of the last `Show()`.
- Pure functions: `ResolveWaveform()`, `WaveformInWindow()`, `EncodeFrame()`, `DecodeWire()`, `FrameTimeNs()`,
  `MaxRefreshHz()`, `MaxPixelsAtRate()`, `MinLogicHighMv()`, `Wheel()`, `ExtractWhite()`, `Validate()`.
- Host example with a simulated strip, ESP32 RMT port and two ESP-IDF examples, Kconfig menu, and a documentation
  site with the datasheet map, timing and colour pages.
- Host tests against a model of the pixels written from the datasheets, and a fuzz target with the driver's promises.

### Changed

Review fixes made before the first release.

- A failed `Transmit()` now makes the driver wait one full reset (`DelayUs`, line low) before the next `Show()` or
  `TraceWire()`, so a frame cut half-way is never continued. `DelayUs()` is a required port hook.
- `Show()` and `TraceWire()` return the new `Error::NotPowered` (17) after `PowerOff()`, even when the pin write failed.
- The longest low inside a frame is derived from the smallest latch time of the documents of a variant (`PreV5` 50 us,
  `Sk6812` 50 us, 20 us for the later SK6812 sheets), with a 10 percent margin, instead of one 50 us figure.
- `MinLogicHighMv(Ws2812bV5, ...)` returns the strictest of the revisions (0.63 x VDD, at least 2.7 V): 3150 mV at 5 V.
- Named variants are refused below `kMinResolutionHz` (now 20 MHz) with `TimingUnreachable`; `Custom` is exempt.
  The V5 profile aims T1H and T1L at 650 ns (T0H 350 ns) for margin.
- A port with a `GpioSet()` hook starts unpowered: `Show()` and `TraceWire()` return `NotPowered` until `PowerOn()`
  succeeds (also after a failed `PowerOn()`); a port with the hook but no pin returns `true` from it.
- `ResolveWaveform()` returns `TimingUnreachable` when a low phase would exceed the `max_gap_ticks` it hands the port.
- New pure function `ResolveRmtRate()` (the real tick rate of the ESP32 RMT: the divider rounded to the nearest
  integer and the group prescaler search of ESP-IDF v5.5, with a ppm tolerance), used by the ESP32 port and tested
  on the host. The ESP32 port also derives `mem_block_symbols` per target (multiple of the memory block) and sets the
  RMT interrupt priority.
- Removed the unused `_u32` literal and `LiteralOutOfRange`.
- Datasheet map: the Watterott copy is revision V6.1 (2022-09), the Hestore copy V6.0 (2021-12) and the 1313 package
  sheet are listed; input level text in README, troubleshooting and the DIN pin row follows the per-revision values;
  both reference circuits show the 74HCT buffer and the 330 ohm series resistor.
- The ESP32 port reports the tick rate it really got and refuses a clock more than 1000 ppm off the profile; it
  documents that it cannot see underruns. Documentation: input levels per datasheet revision, a corrected reference
  circuit (load switch, 74HCT buffer, series resistor), relabelled datasheet entries (Watterott V6.1; the Hestore copy
  is V6.0, 2021), "mirror" instead of "vendor page" in the generated tables.
- Both ESP32 examples call `PowerOn()` in their start-up (the custom example never did, so every `Show()` returned
  `NotPowered`); the start-up of each example is a `Start()` function that the host tests run against stand-ins of
  the ESP-IDF headers (`tests/esp32_stub/`). The basic example checks every `SetPixel()` result.
- The ESP32 port requests 32 MHz on the ESP32-H2 (its RMT runs from the 32 MHz crystal, where 20 MHz is not reachable)
  and adds two RTOS ticks to the frame wait, which ESP-IDF rounds down to whole ticks.
- The ESP32 port reads back the rate its channel really got (`rmt_get_channel_resolution()`) and refuses to start when
  it differs from the computed one, as when another channel fixed the shared group prescaler first.
- The profile table passes `U` literals through explicitly qualified calls, and the conditional and `std::min`
  results widened to `uint64_t` are cast explicitly.

**Not verified**

- Hardware: the ESP32 port and the driver have not been run on a strip.
