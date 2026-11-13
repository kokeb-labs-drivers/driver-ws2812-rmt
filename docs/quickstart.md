# Quickstart

Light a strip from your MCU: wire it, write a small port class over your timed-output peripheral, call three driver
functions.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    From Worldsemi WS2812B-V5 V5.0:

    - Pins and package: four pin 5050: PIN Configuration, p. 2

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

## 1. Wire it

| MCU | Strip | Notes |
| --- | --- | --- |
| Timed-output pin (on ESP32 an RMT channel) | DIN | A 5 V strip needs a level shifter for a 3.3 V MCU: see [Hardware](hardware.md) |
| GPIO (optional) | Supply enable | Only if you want `PowerOn()` and `PowerOff()` |
| GND | GND | Common ground with the strip supply |
| | VDD | 5 V supply with a large capacitor at the strip input and 100 nF at each pixel |

## 2. Add the driver

```bash
git submodule add https://github.com/kokeb-labs-drivers/driver-ws2812-rmt.git external/driver-ws2812-rmt
```

## 3. Write the port

```cpp title="my_port.hpp"
--8<-- "tests/docs/my_port.hpp:port"
```

On ESP32 use the finished port in [`examples/esp32`](../examples/esp32/README.md) instead.

## 4. Show a colour

```cpp title="main.cpp"
--8<-- "tests/docs/quickstart.cpp:quickstart"
```

1. `EnsureInitialized()` creates the channel through your port, asks it for its real tick rate and resolves the
   waveform. A rate that cannot meet the datasheet windows is refused here (`TimingUnreachable`).
2. `SetPixel()` and `Fill()` only store colours. `Show()` encodes them and sends the frame.

The defaults suit a current WS2812B (V5 or V6, GRB). For another part set `Config::variant`:
[Timing](timing.md) says which one to pick.

Next: [API guide](api-guide.md).
