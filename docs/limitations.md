# Limitations

What the driver does not cover, what it assumes where the datasheets are silent, and what is not verified. Each
assumption has a knob, and the knob is named.

## Not covered

- **WS2812E** is not named. No datasheet for it could be obtained, so the driver does not guess its timing. Hold its
  sheet and use `Custom`, or ask for a named variant.
- WS2812D-F5 (same family as the F8, unverified) and the vendor's current WS2812B V7 sheet were not obtained.
- WS2811 and other external driver ICs with a 400 kbit/s mode, dual-signal parts with a backup data line, SPI-clocked
  pixels with a separate clock line, and 16-bit-per-channel parts: different frame or wire.
- The original WS2812's low-speed mode (400 kbit/s minimum, no windows given): reachable through `Custom`.
- Animations, effects and colour-space conversions: the driver sends colours. `Wheel()` and `ExtractWhite()` are
  helpers, not an effects engine.
- Underruns are not detected by the reference ESP32 port: the RMT TX driver reports no underrun, so a starved frame
  latches early and shows stale pixels without an error. `Error::Bus` for an underrun appears only for a port that can
  see one. Prevent it with DMA, a larger channel memory, and the bench step in [Porting](porting.md#bench-steps).
- `Sk6812Mini` has three colour channels (the MINI and MINI-E sheets); a four-channel mini LED is a different part:
  use `Sk6812Rgbw`, or `Custom` windows with its datasheet. The SK6812MINIRGBX sheet (`skm` in the Datasheet map)
  describes such an RGBW part (CSPM1411RGBW-IC-6): `Sk6812Mini` takes only its timing windows, and the
  `Sk6812Rgbw` waveform also meets them.
- A failed `PowerOff()` (the pin write failed) leaves the supply in doubt, and the driver then refuses every frame,
  a blank one included, with `NotPowered`: DIN must not be driven while VDD may be off (input at most VDD + 0.7 V,
  WS2812B-V5 V5.0, Absolute Maximum Ratings, p. 2). The LEDs may keep their last colours. To recover, call
  `PowerOff()` again, or, when the supply is on after all, `PowerOn()`, `Clear()`, `Show()` and `PowerOff()`.
- A port with a `GpioSet()` hook starts unpowered: `Show()` and `TraceWire()` return `NotPowered` until `PowerOn()`
  succeeds. A port with the hook but no pin (`pins.power < 0` in the ESP32 example) returns `true` from it, because the
  supply is always on. `PowerOn()` and `PowerOff()` return `Unsupported` when the port has no hook.
- The ESP32 port shares the RMT group clock with every other RMT channel on the C3, C5, C6, H2, S3 and P4. A channel
  created earlier at another rate can leave this one at a rate the waveform does not fit; the port reads the real
  rate back after creating its channel and refuses to start (`Bus`) on a mismatch, so it never sends wrong colours,
  but it cannot share the group with such a channel. It reads the rate through `rmt_get_channel_resolution()`, an
  internal ESP-IDF function (`esp_private/rmt.h`, present in v5.5) that a later ESP-IDF may change.
- Mixed generations in one chain: there may be no waveform legal for both ([Timing](timing.md#mixed-chains)).

## Assumptions

| Item | What the driver does | Knob |
| --- | --- | --- |
| Reset is given only as a lower bound | Sends the datasheet minimum plus 20 µs | `Config::reset_us`, `kResetMarginUs` |
| No pixel power-on reset time in any datasheet | Waits 1 ms after `PowerOn()` | `Config::power_on_settle_us` |
| SK6812RGBW order differs between revisions | Defaults to GRBW (the later revision) | `Config::order` |
| The V6 data cycle of 1250 ns and the SK6812 code period of 1200 ns are minimums | Met by lengthening a low; V5 aims T0H at 350 ns for margin | the variant, or `Custom` |
| "Low below 20 µs" appears only in the SK6812 Rev 07 and MINI sheets | Applied to those variants; for the others the limit is the smallest latch time of the variant's documents, less 10 percent, minus one tick (`Sk6812`: its 50 µs figure) | profile `max_low_ns` |
| The SK6812 Rev 01 states 80 µs in the table and 50 µs in a figure | Uses 80 µs | the profile reset |
| WS2812 6-pin supply printed as 6.0 V to 7.0 V | Informational only; copied as printed | none |
| Number of memory symbols of each ESP32 RMT channel | The port derives a default from `SOC_RMT_MEM_WORDS_PER_CHANNEL`, not verified on a chip | `mem_block_symbols` in the port |
| Gamma and brightness are not chip features | Convenience, off by default | `Config::gamma`, `brightness` |

## Not verified

- **Hardware.** Nothing in this driver is verified on hardware: it is host-tested against a model written from the
  datasheets. The ESP32 port, the RMT tick rate it reports, the reset wait and the absence of underruns are bench
  items ([Porting, bench steps](porting.md#bench-steps)).
- The ESP32 examples are built by CI, not by the host tests, and are first confirmed by the first CI run.
- The MISRA C++:2023 scan is a CI job (CodeQL) and is first confirmed by the first CI run; locally the code follows
  the patterns of the handbook's review.
- Two pin orders: the SK6812 5050 package differs between Rev 01 and later; the pin positions of the drawing were not
  compared with a part.
- The windows of the two copies kept in the datasheet map for their input level (Watterott V6.1, Hestore V6.0) match
  the sheets listed with them where their text could be extracted, and the 1313 package sheet's T1 rows were not
  extractable and are taken to equal V5.
