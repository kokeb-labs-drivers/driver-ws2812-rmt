# API guide

The driver by task. Every fallible call returns `Result<T>`: check it with `ok()` or `if (result)`, then use
`value` or `error`. Nothing throws, and nothing is allocated.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    From Worldsemi WS2812B-V5 V5.0:

    - 24 bit data, GRB, high bit first: Composition of 24bit Data, p. 4
    - Cascade and data transmission method: Data Transmission Method, p. 4

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

| Task | Calls |
| --- | --- |
| Start | `EnsureInitialized()`, `IsInitialized()` |
| Colours | `SetPixel()`, `GetPixel()`, `Fill()`, `Clear()`, `SetBrightness()`, `SetGamma()` |
| Send | `Show()` |
| Supply | `PowerOn()`, `PowerOff()` |
| Raw access | `TraceWire()`, `GetLastWire()` |
| Inspect | `Length()`, `ChannelsPerPixel()`, `CurrentWaveform()`, `FrameTimeUs()`, `GetConfig()` |
| Pure functions | `ResolveWaveform()`, `WaveformInWindow()`, `EncodeFrame()`, `DecodeWire()`, `Wheel()`, `ExtractWhite()`, ... |

## The object

```cpp
ws2812_rmt::Ws2812Rmt<MyPort, 60> strip(port, config);
```

The second template argument is the capacity: the object holds 60 colours and the byte buffer for them (4 bytes per
pixel at most), so no heap is needed and the buffers live where you put the object. `Config::pixel_count` can be
smaller than the capacity (0 means the whole capacity). The constructor does no I/O and cannot fail.

## Start

`EnsureInitialized()` checks the configuration, asks the port to open its channel, asks for the tick rate and
resolves the waveform. It returns `InvalidArgument` (the configuration is invalid, no port call made), `Bus` (the
port failed) or `TimingUnreachable` (the rate cannot meet a window). After a failure the driver is still
uninitialised and the next call starts again. After a success it returns `Ok` at once.

## Set and show

`SetPixel()` stores a logical colour and returns `InvalidArgument` for an index past the strip (nothing changes).
`Fill()`, `Clear()`, `SetBrightness()` and `SetGamma()` never fail and do no I/O. `Show()` encodes every pixel
(gamma, then brightness, in the wire order) and asks the port to send it; it blocks until the frame and the reset are
out. A failed `Show()` leaves the pixels intact: call it again.

## Brightness and colour order

[Colour](colour.md) has both, and [Configuration](configuration.md) every field of `Config`.

```cpp title="api_tasks.cpp"
--8<-- "tests/docs/api_tasks.cpp:rgbw"
```

## Raw access

`TraceWire(bytes)` sends bytes exactly as given, as one frame with the resolved waveform: no order, brightness or
gamma, no retry, the stored pixels untouched. It finds the order of an unknown strip (send `{0xFF, 0, 0}` and see
which colour lights). `GetLastWire()` returns the bytes `Show()` encoded last, with no I/O, valid until the next
`Show()`; compare it with what a logic analyser captured.

## Supply

`PowerOn()` drives the port's power pin active and waits `Config::power_on_settle_us`; `PowerOff()` drives it inactive
and keeps the waveform. A port without a `GpioSet()` hook gets `Unsupported`. A port with the hook starts
unpowered: call `PowerOn()` before the first `Show()`. A port with the hook but no pin must return `true` from
`GpioSet()`. After `PowerOff()`, `Show()` and
`TraceWire()` return `NotPowered` and send nothing, because a driven DIN on an unpowered strip back-feeds it
([Hardware](hardware.md)).

```cpp title="api_tasks.cpp"
--8<-- "tests/docs/api_tasks.cpp:power"
```

## Errors

```cpp title="api_tasks.cpp"
--8<-- "tests/docs/api_tasks.cpp:errors"
```

The pixels are write-only: no identity, no status, no checksum. So `Timeout`, `DeviceIdMismatch`, `Crc`, `Busy` and
`DeviceFault` are never returned; a timeout or an underrun that the port detects arrives as `Bus`. See
[Contract](contract.md) for each error.
