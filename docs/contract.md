# Contract

What the WS2812_RMT driver promises, what it needs, and the rules a caller follows.

The driver inherits the [Kokeb Labs driver contract](https://github.com/kokeb-labs/.github/blob/main/standards/driver.md);
this page states only what is specific to it.

## Promises

- The constructor is `noexcept`, stores the configuration, does no I/O and cannot fail. The pixels start black.
- `EnsureInitialized()` is the only start-up call. It checks the configuration (`InvalidArgument`, no port call), lets
  the port open its channel and report its tick rate (`Bus`), and resolves the waveform (`TimingUnreachable`). After it
  has succeeded it returns `Ok` with no I/O; after a failure the driver stays uninitialised.
- `Show()`, `PowerOn()`, `PowerOff()`, `CurrentWaveform()` and `FrameTimeUs()` return `NotInitialized` before it has
  succeeded, and `Show()` makes no port call then.
- **The waveform lies inside the windows.** Every waveform the driver gives the port is inside the declared windows
  (every document of a named variant, or the declared window of `Custom`), with the reset above the minimum, at the
  tick rate the port reported. A rate that cannot do that is refused, whatever the rate (including 0 and 2^32 - 1).
- Nothing is sent after `InvalidArgument` or `TimingUnreachable`, and nothing before a successful start.
- The wire has exactly `Length() x ChannelsPerPixel()` bytes, in the order of the configuration, after gamma and
  brightness.
- A failed `Transmit()` leaves the pixels intact and the driver usable. The next `Show()` or `TraceWire()` first waits
  one full reset (`port.DelayUs`, line low), so a frame cut half-way is never continued by the next one, then sends the
  same frame.
- A port with a `GpioSet()` hook starts unpowered, and after `PowerOff()` (even when the pin write failed) `Show()` and
  `TraceWire()` return `NotPowered` and send nothing until `PowerOn()` succeeds. The reason: an input above VDD + 0.7
  V is outside the absolute maximum ratings (WS2812B-V5 V5.0, p. 2), so DIN is not driven while the supply may be off.
  After a failed `PowerOff()` the LEDs may keep their last colours; recover with `PowerOff()` again, or with
  `PowerOn()`, `Clear()`, `Show()`, `PowerOff()` ([Limitations](limitations.md)).
- Every port failure (open, rate, send, timeout, power pin) propagates as `Bus`. An underrun is reported only if the
  port detects it; the ESP32 example cannot see one, so a starved frame is not reported (see Limitations).

## Depends on

| Dependency | Version | Why |
| --- | --- | --- |
| A C++20 compiler | GCC 10, Clang 13 or later | Concepts constrain the port |
| CMake | 3.16 or later | Generates `ws2812_rmt_version.h` |
| Your port | | A timed bit stream, its real tick rate, optionally a supply pin |

## Threading

Not thread-safe; takes no lock. One owner task, or one mutex around every call. Do not call it from an ISR: `Show()`
blocks until the frame is out.

## Errors

| `Error` | Value | Returned when |
| --- | --- | --- |
| `Ok` | 0 | The call succeeded |
| `NotInitialized` | 1 | Before `EnsureInitialized()` succeeded |
| `InvalidArgument` | 2 | `Validate()` failed, or a pixel index is out of range |
| `Bus` | 3 | A port hook returned `false` |
| `Unsupported` | 9 | `PowerOn()` or `PowerOff()` on a port without `GpioSet()` |
| `TimingUnreachable` | 16 | The port's tick rate cannot meet a window, or a low phase would exceed the gap limit |
| `NotPowered` | 17 | `Show()` or `TraceWire()` before `PowerOn()` succeeded (port with `GpioSet()`) or after `PowerOff()` |

`Timeout`, `DeviceIdMismatch`, `Crc`, `Busy` and `DeviceFault` are never returned: the pixels have no read-back path,
so there is no identity, no status and no checksum to fail, and a port reports a timeout as `false`.

## Memory

No heap, exceptions, RTTI or static mutable state. The object holds a port reference, the `Config`, the colours
(`capacity` x 4 bytes) and the wire bytes (`capacity` x 4 bytes).

## Platform

Platform-free; the ESP-IDF port in `examples/esp32` is an example. The driver reaches the peripheral only through the
port.

## Deviations from the standard

None.
