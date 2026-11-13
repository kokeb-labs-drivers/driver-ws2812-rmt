# Configuration

Every field of `Config`, what the defaults are, what `Validate()` refuses, and how to describe a part the driver does
not name.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    From Worldsemi WS2812B-V5 V5.0:

    - T0H, T0L, T1H, T1L windows: WS2812B V5, V6, 2020: Data Transfer Time, p. 3
    - RES: the reset low time: Data Transfer Time (RES), p. 3

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

`Config::Default()` suits a current WS2812B. The constructor stores the configuration; `EnsureInitialized()` checks it
before any port call. `Validate(config, capacity)` is `constexpr`, so a fixed configuration can be proved at build time
(see the `static_assert` in the `Custom` example below).

| Field | Default | Meaning |
| --- | --- | --- |
| `variant` | `Ws2812bV5` | The timing generation: [Timing](timing.md) |
| `use_default_order` | `true` | `false` uses `order`; always `false` for `Custom` |
| `order` | empty | The wire order of the channels: [Colour](colour.md) |
| `brightness` | 255 | Linear dimming at start, 0 to 255 |
| `gamma` | `None` | The transfer curve at start |
| `pixel_count` | 0 | Pixels in the strip; 0 means the whole capacity |
| `reset_us` | 0 | Reset low time; 0 means the datasheet minimum plus 20 µs. A value must exceed the minimum |
| `power_on_settle_us` | 1000 | Wait after `PowerOn()`; no datasheet value (an assumption) |
| `transmit_slack_ms` | 20 | Added to the frame time to form the timeout the port is given |
| `custom` | all zero | The timing of `Variant::Custom`; must stay zero for a named variant |

## What Validate refuses

An unknown variant or gamma; a `custom` timing on a named variant; an inconsistent `custom` timing on `Custom`; an
invalid order, one with a channel count that differs from the variant's, or a missing order on `Custom`; a `reset_us`
that is not above the minimum or above 10 ms; a `pixel_count` above the capacity; a capacity of 0 or above 65 536
pixels; a settle above 1 s or a slack above 60 s. It returns `InvalidArgument` and the port is not touched.

## Kconfig (ESP-IDF)

`idf.py menuconfig` has a "WS2812_RMT addressable LED driver" menu: the variant, the start brightness, the reset time
(0 for the datasheet minimum plus 20 µs) and the power-on settle time. The values reach the code only through
`ws2812_rmt_kconfig.hpp`; outside ESP-IDF `KconfigDefaults()` equals `Config::Default()`. The strip's pin, length and
tick rate belong to the application and its port, not to the driver.

```cpp title="configuration.cpp"
--8<-- "tests/docs/configuration.cpp:kconfig"
```

## A part with its own timing

`Variant::Custom` takes the windows from `Config::custom` and nothing from the named parts. Each `Range` is a minimum,
a maximum (0 for none) and the nominal value the driver aims for. A `Custom` part states its order and its reset:

```cpp title="configuration.cpp"
--8<-- "tests/docs/configuration.cpp:custom"
```

Copy the numbers from your part's datasheet with the section and page in a comment. `Validate()` rejects a window with
a zero minimum, a maximum below the minimum, a nominal outside the window, a period maximum that its own windows cannot
meet, a reset of 0 or above 10 ms, and durations above 100 µs (enough for 400 kbit/s).
