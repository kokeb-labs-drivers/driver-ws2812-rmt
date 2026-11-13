# Troubleshooting

Find the cause of an error or of a strip that does not look right. Errors first, then symptoms.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    - Input levels VIH and VIL: Worldsemi WS2812B-V5 V5.0, Electrical Characteristics, p. 2
    - RES: the reset low time: Worldsemi WS2812B-V5 V5.0, Data Transfer Time (RES), p. 3
    - Input level 0.63 x VDD (later V5 family sheets): Worldsemi WS2812B-V5 V6.1, Electrical Characteristics, p. 3
    - 32 bit data and its order: RGBW (revisions 01 and 03): Opsco SK6812RGBX-XX Rev. 03, 13. The data structure of
      32bit, p. 6
    - 32 bit data and its order: GRBW: Opsco SK6812RGBX-XX Rev. 07, 13. The data structure of 32bit, p. 9

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

## Errors

| Symptom | Likely cause | What to do |
| --- | --- | --- |
| `EnsureInitialized()` returns `InvalidArgument` | `Validate()` refused the configuration: a repeated channel in the order, a pixel count above the capacity, a `custom` timing on a named variant, a reset not above the minimum | `Validate(config, capacity)` returns the same error at build time; check each field against [Configuration](configuration.md) |
| `EnsureInitialized()` returns `Bus` | The port could not open the channel (pin in use, no free channel) or report its rate | Read the port's log line; check the pin and the channel count of your MCU |
| `EnsureInitialized()` returns `TimingUnreachable` | The port's tick rate is too slow to place a tick count inside a window | Run the channel at 20 MHz or more for a named variant ([Timing](timing.md)); or your `Custom` windows are narrower than a tick |
| `Show()` returns `NotInitialized` | `EnsureInitialized()` did not succeed | Check its result first |
| `Show()` returns `Bus` | `Transmit()` failed: a timeout, an underrun, a peripheral error | The pixels are intact: call `Show()` again; raise `transmit_slack_ms` for a slow port; see "first pixels right, rest wrong" below |
| `Show()` returns `NotPowered` | The supply is off: `PowerOn()` was not called, failed, or `PowerOff()` came last | Call `PowerOn()` and check its result; a port with a `GpioSet()` hook starts unpowered |
| `PowerOn()` returns `Unsupported` | The port has no `GpioSet()` hook | Add the hook, or do not call it: the strip's supply is then yours |
| `SetPixel()` returns `InvalidArgument` | The index is past `Length()` | Nothing changed; `Length()` is `pixel_count`, or the capacity |

## Symptoms

| Symptom | Likely cause | What to do |
| --- | --- | --- |
| Nothing lights | No ground in common, supply off, DIN not driven, a 3.3 V output on a part that needs 3.5 V, DIN wired to DOUT | Check ground and supply first, then the level ([Hardware](hardware.md#input-level-and-the-level-shifter)), then the pin order of the package |
| The first pixel is a wrong colour, the rest follow | The input level is marginal, or the first pixel sees ringing | Fit a level shifter and a 300 to 500 ohm series resistor close to the first pixel |
| Red and green are swapped | The part is an older SK6812RGBW, or a clone with another order | Set `Config::order` to `kOrderRgbw` or the order your part uses ([Colour](colour.md)) |
| Colours are all wrong but stable | The wire order does not match | Send red, green and blue one at a time and read off the order; set it in `Config::order` |
| All pixels show one colour | The reset is too short, so each frame latches early, or the frames run together | Check `reset_us` and the generation of your WS2812B: the Mar-2017 sheet needs a reset above 300 µs, the 2013 sheets say 50 µs |
| Random flicker, or the strip shows stale colours | A gap inside the frame longer than the reset: an underrun | Use DMA where the peripheral has it, enlarge the channel memory, or keep interrupts short during `Show()` |
| First pixels right, the rest wrong or stale | The frame was cut by a low above the reset | Same as above; log from the port when it detects an underrun and return `false` |
| Lights on the first pixels only | The frame is longer than the strip's usable chain, or a pixel is dead | A dead pixel stops the chain: swap or bypass it |
| Wrong colours with the default variant on an older strip | The default is the V5 generation | Try `Ws2812bPreV5`; look at the date of your datasheet ([Timing](timing.md)) |
| Two generations in one chain misbehave | There is no waveform inside both windows | [Timing, mixed chains](timing.md#mixed-chains) |
| Far end dim or tinted | Voltage drop along the strip | Feed the strip from several points, lower the brightness |
| Dim on a 3.3 V logic line only | The input threshold of your part is above 3.3 V at 5 V supply | A level shifter: `MinLogicHighMv()` is 3.15 V at 5 V and 3.34 V at 5.3 V for the V5 family (only the V5.0 sheet accepts 2.7 V) |
| `Show()` takes longer than expected | A frame of N pixels takes N x 24 bits x 1.3 µs (the V5 default; 1.25 µs for the shortest legal cycle), plus the reset | `FrameTimeUs()` gives the worst case; split the strip across channels if you need more frames per second |
