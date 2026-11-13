# Timing

How the driver turns a datasheet into a waveform, and which variant to pick for your LEDs.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    - WS2812 (6 pin) windows: Worldsemi WS2812 undated, Data transfer time, p. 4
    - WS2812B 2013 windows: Worldsemi WS2812B undated, Data transfer time, p. 3
    - WS2812B Mar-2017 windows: Worldsemi WS2812B Mar-2017, Data Transfer Time, p. 4
    - T0H, T0L, T1H, T1L windows: WS2812B V5, V6, 2020: Worldsemi WS2812B-V5 V5.0, Data Transfer Time, p. 3
    - RES: the reset low time: Worldsemi WS2812B-V5 V5.0, Data Transfer Time (RES), p. 3
    - Propagation delay DIN to DOUT: Worldsemi WS2812B-V5 V5.0, Switching Characteristics, p. 3
    - Cascade and data transmission method: Worldsemi WS2812B-V5 V5.0, Data Transmission Method, p. 4
    - Same windows, V6 edition: Worldsemi WS2812B-V6 V1.1, Data Transfer Time, p. 4
    - WS2812C windows: Worldsemi WS2812C V1.0, Data Transfer Time, p. 3
    - WS2812D-F8 windows: Worldsemi WS2812D-F8 V1.1, Data Transfer Time, p. 4
    - SK6812 windows: Opsco SK6812 Rev. 01, Data transfer time, p. 5
    - Reset low time of the SK6812: Opsco SK6812 Rev. 01, Data transfer time (Trst), p. 5
    - SK6812RGBW windows, low below 20 us, code period: Opsco SK6812RGBX-XX Rev. 07, 12. The data transmission time, p.
      7
    - SK6812MINI windows: Opsco SK6812MINI-E Rev. 02, Data transmission time, p. 7

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

## The bit

A bit is a high pulse followed by a low one, and the length of the high pulse is the value: a short high is 0, a long
high is 1. The data line idles low. After the last bit of the frame the line stays low for the reset time, which
latches the colours.

| Symbol | Meaning |
| --- | --- |
| T0H, T0L | 0 code: high time, low time |
| T1H, T1L | 1 code: high time, low time |
| RES | Low time, above which the pixels latch and the next bit starts a new frame |

Each datasheet gives a window (a minimum and a maximum) for each of the four times, sometimes a bit period, and a
minimum for RES. The windows are not the same across parts or even across revisions of one part. The WS2812B has four
generations (2013, Mar-2017, V5, V6) whose windows have **no waveform in common**: the T1L of the Mar-2017 sheet is
220 to 420 ns, and the T1L of V5 is 580 ns to 1 µs.

## Pick the variant of your LEDs

`Config::variant` selects one set of windows. Each named variant is the **intersection** of the windows of every
document of that variant, so a waveform inside it satisfies all of them.

| Variant | Parts | Reset above | Documents (id of [the map](datasheet.md)) |
| --- | --- | --- | --- |
| `Ws2812bV5` (default) | WS2812B V5 and V6, WS2812B-2020, WS2812C-2020 | 280 µs | b-v5, b-v5-w, b-v6-h, b-v6, b-2020, c-2020, b-1313 |
| `Ws2812bPreV5` | WS2812B of 2013 to 2017, WS2812S | 300 µs; low inside a frame below 50 µs | b-af, b-cn, b-mar17, ws2812s |
| `Ws2812` | WS2812 (6 pin) | 50 µs | ws2812 |
| `Ws2812c` | WS2812C (5050), WS2812-2020 | 280 µs | c-a, c-b, ws-2020 |
| `Ws2812d` | WS2812D-F8 | 280 µs | d-f8 |
| `Sk6812` | SK6812 (RGB) | 80 µs; low inside a frame below 50 µs | sk-01 |
| `Sk6812Rgbw` | SK6812RGBW | 80 µs; low inside a frame below 20 µs | skw-01, skw-03, skw-07 |
| `Sk6812Mini` | SK6812MINI, MINI-E | 80 µs; low inside a frame below 20 µs | skm-e; skm (windows only) |
| `Custom` | any other part with this frame | your value | yours |

How to tell a WS2812B generation: the V5 and V6 sheets say "Ver. No." and show a reset of 280 µs, T1H and T1L of
580 ns to 1 µs; older sheets show 300 ± 150 ns style values and a reset of 50 or 300 µs. The date of the datasheet
you bought the strip with, or the silkscreen revision, is the guide. A strip that flickers or shows wrong colours
with the default variant usually wants `Ws2812bPreV5`. `WaveformInWindow(variant, wave)` checks one waveform against
one variant's windows: [mixed chains](#mixed-chains).

The `Ws2812bPreV5` bit period is limited to 950 to 1550 ns, not the 650 to 1850 ns of the 2013 sheets: the WS2812S
sheet prints 1.25 µs +-300 ns and its documents are part of the variant. The profile is the intersection, so the
tighter period applies.

The windows of every document are in the model of the tests, `tests/mock_ws2812_rmt.hpp` (`kDocuments`), which is
written from the datasheets and kept apart from the driver's profile table, so that a transcription error in either
shows up as a test failure.

## How a waveform is resolved

`EnsureInitialized()` asks the port for its real tick rate and calls `ResolveWaveform()`:

```cpp title="timing.cpp"
--8<-- "tests/docs/timing.cpp:waveform"
```

1. Each duration is converted to ticks: the smallest tick count at or above the window minimum, the largest at or
   below the maximum. The edges are inclusive. The aim is the nominal value of the profile, clamped to the window.
2. If no tick count fits a window (the tick is longer than the window), the result is `TimingUnreachable`, and nothing
   is sent. The driver also refuses a named variant at a rate below `kMinResolutionHz` (20 MHz): see
   [Why 20 MHz](#why-20-mhz).
3. A period minimum (the V6 data cycle of 1250 ns, the SK6812 code period of 1200 ns) is met by lengthening the low
   of a bit, which stays inside its window.
4. The reset is strictly above the minimum: `Config::reset_us`, or the datasheet minimum plus 20 µs when it is 0.
   The reset is a timed low after the last bit: the ESP32 port waits it with the line idle low (`eot_level`), so no
   RMT symbol has to hold it and the 32 767-tick half-symbol limit does not apply to it.
5. The longest low inside a frame, `max_gap_ticks`, is the smallest latch time any document of the variant prints,
   less 10 percent, and strictly below that: 252 µs for V5 (latch above 280 µs), 45 µs for the 2013 WS2812B and the
   SK6812 (their sheets print a 50 µs latch next to the larger reset the driver sends), 18 µs for SK6812RGBW and
   MINI (the sheet says the low must stay below 20 µs). The port must not leave a gap that long inside a frame, or
   the pixels latch half a frame. The reset the driver sends is the larger value, which every generation accepts.

The V5 nominals at 20 MHz are 7 ticks for T0H, 19 for T0L, and 13 each for T1H and T1L (350, 950, 650 and 650 ns). The
tick rate must be the **real** one: the port reports what the peripheral runs at, and the driver converts at that rate.

### Why V5 aims T0H at 350 ns

The V5 window of T0H is 220 to 380 ns (the sheets print windows, no typical value; its centre is 300 ns), but the
V6 sheet adds a data cycle of at least 1250 ns (T0H + T0L and T1H + T1L). The `Ws2812bV5` profile covers V6, so the
cycle applies. With T0H 300 ns and T0L 950 ns (19 ticks of 50 ns) the cycle is exactly 1250 ns: any clock error
below nominal shortens it under the limit (at +1000 ppm 1248.75 ns). With T0H 350 ns (7 ticks) the cycle is 1300 ns,
4 % above the minimum, while T0H keeps 30 ns of margin below its 380 ns maximum and T0L 50 ns below its 1000 ns
maximum. 350 and 950 ns are the best pair for the 0 code at 20 MHz: no other tick pair beats 30 ns of margin on
every edge. The 1 code is 650 + 650 = 1300 ns as well, 70 ns from both window edges, so every bit takes 1300 ns and
a strip of 1024 pixels still meets the 30 frames per second of the sheets. The test `A tick-rate error of +-1000
ppm stays inside every window` fails with 300 ns.

Thin margins that remain at 20 MHz, against the printed windows: 30 ns on the V5 profile (T0H; the 1300 ns bit is 50 ns
above the 1250 ns cycle),
50 ns on the 2013 to 2017 WS2812B, SK6812RGBW and SK6812MINI profiles, 70 ns on WS2812C and 80 ns on WS2812D. A clock
within 1000 ppm moves an edge by about 1 ns, so none of them leaves its window.

### Why 20 MHz

At 10 MHz a tick is 100 ns. The T0H window of V5 (220 to 380 ns) then holds a single tick count (300 ns), and a bit
period of 1250 ns can only be met with T0L at the very edge of its window (1000 ns): the margin is 0 ns, and a clock
error of 1000 ppm leaves the window (the same holds at 8, 9.6 and 12 MHz). At 16 MHz V5 still sits exactly on an edge,
and the rates between 16 and 20 MHz leave no margin to spare either. At 20 MHz (50 ns ticks) every window holds several
tick counts, so the aimed values sit 30 ns or more from the edges and a clock error of 1000 ppm cannot push them out.
`kRecommendedResolutionHz` and `kMinResolutionHz` are both 20 MHz (a finer rate such as 40 MHz or 80 MHz keeps the
margins), and the driver returns `TimingUnreachable` below it for a named variant (`Custom` is not limited: its windows
are yours).

## Frame time

The time of a frame is `pixels x bits per pixel x the longer bit period`, plus the reset. `FrameTimeNs()`,
`MaxRefreshHz()` and `MaxPixelsAtRate()` compute it from a waveform:

```cpp title="timing.cpp"
--8<-- "tests/docs/timing.cpp:frame"
```

A 60-pixel WS2812B strip takes about 2.2 ms per frame, and a 1024-pixel strip about 32 ms (the datasheets promise at
least 1024 pixels at 30 frames per second). A frame is sent in one gapless stream and the call blocks until it and the
reset are out; the reference ESP32 port runs the stream without the CPU (RMT, with DMA where the part has it) and then
spends the reset (300 µs) in a busy wait of the CPU. The port must not underrun: see [Porting](porting.md).

## Mixed chains

Pixels of two WS2812B generations in one chain need one waveform inside both windows, and for the 2013 and V5
generations there is none. `WaveformInWindow()` tells which variants accept a waveform:

```cpp title="timing.cpp"
--8<-- "tests/docs/timing.cpp:mixed"
```

When it returns false for a pair you must have, measure which waveform both strips accept and declare it with
`Custom` ([Configuration](configuration.md)), or split the strip into two outputs.

## What is not covered

- **WS2812E** is not named: no datasheet could be obtained for it. Reach it through `Custom` once you hold its sheet.
- WS2812D-F5 is the same family as the F8 and unverified; the current WS2812B V7 sheet was not obtained.
- The original WS2812 lists a low-speed 400 kbit/s mode with no windows. It is reachable through `Custom`.
- WS2811 and other drivers with a 400 kbit/s mode, dual-signal parts (a backup data line), SPI-clocked pixels and
  16-bit-per-channel parts use a different frame or wire.
- No datasheet gives the pixel's power-on reset time or an upper bound for the reset: both are assumptions with a knob
  ([Limitations](limitations.md)).
