# Porting

Write a port: the small class that sends the bytes the driver built, as a timed bit stream, on your MCU.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    - Typical application circuit: Worldsemi WS2812B undated, Typical application circuit, p. 5
    - SK6812RGBW windows, low below 20 us, code period: Opsco SK6812RGBX-XX Rev. 07, 12. The data transmission time, p.
      7

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

The driver is platform-free. It builds the byte stream and resolves the waveform; the port turns bytes into pulses.
That needs a peripheral that sends a precisely timed stream without the CPU toggling a pin per bit: on ESP32 the RMT
transmitter, on other MCUs a timer with DMA, a PIO state machine or an SPI at a clock chosen for the bit time.

## The hooks

Derive from `ws2812_rmt::GpioInterface<Port>` and implement exactly these signatures, all `noexcept`. The driver
checks them with the `Ws2812RmtPort` concept and says in the compile error which hook is wrong.

| Hook | Required | Contract |
| --- | --- | --- |
| `bool EnsureInitialized() noexcept` | yes | Create and enable the channel on the first call; the line idles low and ends low; drive the supply pin inactive first; `true` at once afterwards |
| `bool ResolutionHz(uint32_t& hz) noexcept` | yes | The tick rate the channel really runs at, not the one you asked for |
| `bool Transmit(std::span<const uint8_t>, const Waveform&, uint32_t timeout_ms) noexcept` | yes | Send every byte, most significant bit first, as one gapless stream, then hold the line low `reset_ticks`; return when the last tick is out |
| `bool GpioSet(CtrlPin, GpioSignal) noexcept` | no | `CtrlPin::Power`: ACTIVE switches the strip supply or the level shifter on; a port with this hook starts unpowered, the application calls `PowerOn()` first, and a port with the hook but no pin returns `true` |
| `void DelayUs(uint32_t) noexcept` | yes | Block for at least that many microseconds with the line low; used for the power-on settle and for the reset the driver waits after a failed `Transmit()` |
| `void Log(LogLevel, const char*) noexcept` | no | Plain text, never a format string |

```cpp title="my_port.hpp"
--8<-- "tests/docs/my_port.hpp:port"
```

With the optional hooks:

```cpp title="ports.cpp"
--8<-- "tests/docs/ports.cpp:full-port"
```

## What a port must guarantee

- **A gapless stream.** Between the first and the last bit no low may exceed `Waveform::max_gap_ticks`. A longer low
  is a reset: the pixels latch what they have and ignore the rest. On a peripheral that refills its buffer from an
  interrupt, an underrun makes exactly that gap; return `false` when you can detect one, and log it. The driver cannot
  detect one itself, and the ESP32 example cannot either. Use DMA or a larger buffer where the part has them.
- **Bit 1:** high `t1h_ticks`, then low `t1l_ticks`. **Bit 0:** high `t0h_ticks`, then low `t0l_ticks`. Most
  significant bit first, one byte after the other, no padding bit.
- **The line idles low and ends low**, after a frame and after a failure (a frame cut half-way is the reason the
  driver waits a full reset before the next one). Fit a pull-down on DIN so the line is low before the port drives it
  (both circuits in [Hardware](hardware.md) show one at the buffer input).
  The first symbol of a frame starts high. A line that idles high would be read as the start of a bit.
- **The reset.** After the last bit, hold the line low for `reset_ticks`. A port may do it as a final low symbol, or as
  a timed wait after the last tick when the symbol field is too short: an RMT half-symbol holds 15 bits, so a reset
  above 32 767 ticks (1.6 ms at 20 MHz is fine; 300 µs at 20 MHz is 6000 ticks, also fine) needs a wait or several
  symbols. `kRmtHalfSymbolMaxTicks` is that limit.
- **The real tick rate.** If the peripheral rounds the requested clock, report the rate it got. The driver converts
  nanoseconds at that rate, and refuses a rate that cannot meet the windows.
- **A timeout.** Return `false` when `timeout_ms` passes before the frame is out. The driver computes it from the frame
  time plus `Config::transmit_slack_ms`.
- **Memory the peripheral reaches.** Buffers for DMA live in memory the DMA reaches, owned by the port, never on a
  caller's stack.
- **Pin idle level.** The supply pin starts INACTIVE and is set before it becomes an output (handbook KB-HW-018).
  `ACTIVE` means powered; map the polarity in the port.

| Pin | Hook | ACTIVE means | Idle level | Order at start-up |
| --- | --- | --- | --- | --- |
| Data (DIN) | `Transmit()` | n/a | low | idle low before the channel is enabled |
| Supply enable | `GpioSet(Power, ...)` | strip powered | INACTIVE (off) | set low, then make it an output |

## The ESP32 RMT port

[`examples/esp32/main/esp32_ws2812_rmt_port.hpp`](../examples/esp32/main/esp32_ws2812_rmt_port.hpp) uses the RMT TX
driver of ESP-IDF v5.x: one TX channel at 20 MHz (32 MHz on the ESP32-H2), a bytes encoder built from the waveform
(`bit0` is high `t0h` then low `t0l`, `bit1` likewise, most significant bit first), the end-of-transmission level low,
and a timed wait for the reset. It is written fresh from the public API.

- **Tick rate per target.** The RMT divides its source clock by an integer that ESP-IDF v5.5 rounds to the nearest
  (`rmt_common.c`), and `ResolveRmtRate()` computes the same rate. The default source is the 80 MHz APB or PLL clock
  on the ESP32, S2, S3, C3, C5, C6 and P4, so 20 MHz is exact. The ESP32-H2 runs its RMT from the 32 MHz crystal: 20
  MHz is not reachable there (the divider 2 gives 16 MHz, which the port refuses), so the port asks for 32 MHz on that
  target (`CONFIG_IDF_TARGET_ESP32H2`).
- **Internal RAM.** With `CONFIG_RMT_TX_ISR_CACHE_SAFE`, `rmt_transmit()` refuses a payload outside internal RAM, and
  every `Show()` or `TraceWire()` returns `Bus`. The wire buffer is a member of the driver object, so keep the object
  out of PSRAM, and pass `TraceWire()` a span in RAM, not a constant in flash.
- **The frame wait.** `rmt_tx_wait_all_done()` turns its milliseconds into whole RTOS ticks rounded down, and a wait
  of n ticks can end after n - 1 whole periods, because it starts part-way through the current tick. The port adds
  two tick periods to the driver's timeout, so the wait is never shorter than `timeout_ms`, whatever the tick rate
  and `transmit_slack_ms`.
- **The rate the channel really got.** On the C3, C5, C6, H2, S3 and P4 all RMT channels share one group prescaler
  (`rmt_common.c`, `s_rmt_set_group_prescale()`): the first channel created fixes it, and a later channel gets its
  divider from that group clock. A channel created earlier at another rate (for example an RMT receiver at 1 MHz)
  can leave this one at a rate the waveform was not computed for, and the pixels would show wrong colours with no
  error. So after `rmt_new_tx_channel()` the port reads the real rate (`rmt_get_channel_resolution()`, declared in
  ESP-IDF's internal `esp_private/rmt.h`) and refuses to start (`Bus`) when it differs from the computed one. Create
  this channel first, or give every channel of the group a rate the same group clock reaches.
- **On the host.** `tests/esp32_stub/` builds this port and both examples against stand-ins of the ESP-IDF headers,
  runs each example's `Start()` (the start-up of `app_main()`) with the default pins and checks the channel settings,
  the encoder (bit order and the 32 767-tick limit), the order of the pin and channel calls, the waits, the rate
  read back and the recovery after a failed transmission. That shows the code paths, not the chip.

On parts with RMT DMA, set `with_dma` in `Esp32Ws2812RmtPins`. Without DMA the encoder refills the channel memory
from an interrupt, so a long strip under interrupt load can underrun; the symptom is a strip that shows the first
pixels right and the rest stale or flickering ([Troubleshooting](troubleshooting.md)). How many memory symbols each
ESP32 family member has is not verified here: read the `soc_caps` of your target.

## Bench steps

None of this is verified on hardware. To confirm a port: send one pixel and a 60-pixel frame, and put a logic analyser
or a scope on the data line. Check the pulse widths against the windows of your variant in [Timing](timing.md), the
idle level (low), the absence of gaps inside a frame, the reset low time after it, and the tick rate your port reports.
Then run a 1024-pixel frame with interrupts busy and look for a gap.

Force an aborted frame: make `Transmit()` time out in the middle of a frame (a very small `transmit_slack_ms` on a long
strip), and check that the line stays low afterwards, that the driver then holds a full reset low, and that the next
`Show()` shows the right colours from the first pixel.

The reference port asks the RMT for `mem_block_symbols` symbols: 0 picks two memory blocks without DMA (a multiple of
`SOC_RMT_MEM_WORDS_PER_CHANNEL`: 128 on the ESP32 and ESP32-S2, 96 on the ESP32-C3, C6, H2 and S3) and 1024 with DMA.
These are the `SOC_RMT_MEM_WORDS_PER_CHANNEL` values of the ESP-IDF v5.5 `soc_caps.h` files (64 words on the ESP32
and S2, 48 on the others), not checked on a chip. The interrupt priority is 3 (`Esp32Ws2812RmtPins::intr_priority`),
also unverified.
