# WS2812_RMT ESP32 examples

ESP-IDF applications that run the WS2812_RMT driver on an ESP32 with the RMT transmitter. CI builds them on every
pull request. **Not verified on hardware**: the bench steps are in [`docs/porting.md`](../../docs/porting.md).

## Wiring

| ESP32 | Strip | Notes |
| --- | --- | --- |
| GPIO 8 (`Esp32Ws2812RmtPins::data`) | DIN | A 5 V strip may need a level shifter: its input high level can be above 3.3 V (see Hardware, input level) |
| GPIO `power` (optional) | Supply enable or shifter enable | `-1` when the strip is always powered (the port then reports the supply as always on) |
| GND | GND | Common ground with the strip supply, a 100 nF and a bulk capacitor at the strip |

Change the pin numbers in `Esp32Ws2812RmtPins` (`main/esp32_ws2812_rmt_port.hpp`) to your wiring.

## Build and flash

```bash
git submodule update --init scripts      # the ESP-IDF build scripts
. $IDF_PATH/export.sh                     # ESP-IDF v5.5
./scripts/build_app.sh list
./scripts/build_app.sh ws2812_rmt_basic_example Debug
./scripts/flash_app.sh flash_monitor ws2812_rmt_basic_example Debug
```

`idf.py menuconfig` shows "WS2812_RMT addressable LED driver" under Component config: the timing generation of
the LEDs, the start brightness, the reset time and the power-on settle time that `ws2812_rmt::KconfigDefaults()`
returns.

## Expected output

```text
I (312) ws2812_rmt_example: WS2812_RMT driver 0.1.0
```

The strip shows red for a second, then a rainbow that turns. An `EnsureInitialized failed: Bus` line means the RMT
channel could not be created (pin in use, no free channel); `TimingUnreachable` means the tick rate cannot meet
the variant's windows (the port's `resolution_hz` must be at least 20 MHz).

## Files

| File | What it is |
| --- | --- |
| [`main/ws2812_rmt_basic_example.cpp`](main/ws2812_rmt_basic_example.cpp) | Menuconfig timing, red, then a rainbow |
| [`main/ws2812_rmt_custom_example.cpp`](main/ws2812_rmt_custom_example.cpp) | `Variant::Custom`, RGBW order |
| [`main/esp32_ws2812_rmt_port.hpp`](main/esp32_ws2812_rmt_port.hpp) | The ESP-IDF port: RMT bytes encoder, reset wait, supply pin, log |
| [`main/ws2812_rmt_examples.hpp`](main/ws2812_rmt_examples.hpp) | Each example's strip and `Start()`, which the host tests also run (`tests/esp32_stub/`) |
| [`CMakeLists.txt`](CMakeLists.txt) | The ESP-IDF project; `EXTRA_COMPONENT_DIRS` points at the repository root, which is the component |
| [`app_config.yml`](app_config.yml) | The app matrix for the scripts, CI and the docs |
