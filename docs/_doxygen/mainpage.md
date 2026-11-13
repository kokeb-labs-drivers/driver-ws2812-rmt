# WS2812_RMT driver {#mainpage}

A C++20 driver for single-wire addressable RGB and RGBW LEDs (WS2812, SK6812 and compatible), sent as a timed bit
stream through a port class you provide. The one header to include is `ws2812_rmt.hpp`; the class is
`ws2812_rmt::Ws2812Rmt<Port, PixelCapacity>`.

- Start with `Ws2812Rmt::EnsureInitialized()`, then `SetPixel()` and `Show()`.
- The port contract is `ws2812_rmt::GpioInterface` and the `Ws2812RmtPort` concept.
- The pure functions (`ResolveWaveform()`, `EncodeFrame()`, `FrameTimeNs()`, ...) are in the `ws2812_rmt` namespace.
- Which variant fits which part, and why: the Timing page of the documentation site.
