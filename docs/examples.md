# Examples

One folder per platform under `examples/`, each with its own port, built by CI on every pull request. The driver
itself has no platform code; [Platforms](platforms.md) lists what exists.

## Host

[`examples/host/`](../examples/host/README.md) runs the driver on any PC against a simulated strip, through the
host port [`host_ws2812_rmt_port.hpp`](../examples/host/host_ws2812_rmt_port.hpp). No hardware:

```bash
cmake -S examples/host -B build-example && cmake --build build-example && ./build-example/ws2812_rmt_host_example
```

It shows one pixel, a rainbow and an RGBW frame, and reads back what the simulated chain of pixels latched. The host
tests build it with warnings as errors and run it (CTest `ws2812_rmt_host_example`).

## ESP32

ESP-IDF applications that run the driver on an ESP32 with the RMT transmitter; the wiring is in the
[example folder's README](../examples/esp32/README.md) and the port in
[`esp32_ws2812_rmt_port.hpp`](../examples/esp32/main/esp32_ws2812_rmt_port.hpp). The project points
`EXTRA_COMPONENT_DIRS` at the repository root, which is the driver's ESP-IDF component.

### The apps

<!-- generated: examples -->
| App | What it shows | Source | CI |
| --- | --- | --- | --- |
| `ws2812_rmt_basic_example` :material-star:{ title="featured" } | Start an 8 pixel strip from the menuconfig values, show red, then a turning rainbow | [`ws2812_rmt_basic_example.cpp`](../examples/esp32/main/ws2812_rmt_basic_example.cpp) | yes |
| `ws2812_rmt_custom_example` | A part with its own timing: Variant::Custom, a declared window set and an RGBW order | [`ws2812_rmt_custom_example.cpp`](../examples/esp32/main/ws2812_rmt_custom_example.cpp) | yes |

Build and flash one (from `examples/esp32`, ESP-IDF environment loaded):

```bash
./scripts/build_app.sh ws2812_rmt_basic_example Release
./scripts/flash_app.sh flash_monitor ws2812_rmt_basic_example Release
```

Source of this table: [`examples/esp32/app_config.yml`](../examples/esp32/app_config.yml).
<!-- /generated -->

### Set up once

```bash
git submodule update --init examples/esp32/scripts   # the ESP-IDF build scripts
cd examples/esp32
. $IDF_PATH/export.sh                                 # ESP-IDF v5.5
./scripts/build_app.sh list                           # every app and build type
```

The scripts read [`app_config.yml`](../examples/esp32/app_config.yml) (the same file this page's table comes
from) and build each app as its own firmware image.

### What to read in an example

| You want to | Read |
| --- | --- |
| See the lifecycle and every `Result` checked | `ws2812_rmt_basic_example.cpp` |
| Describe a part with its own timing | `ws2812_rmt_custom_example.cpp` |
| Write an ESP-IDF port over the RMT transmitter | `esp32_ws2812_rmt_port.hpp` |
| Use menuconfig values | `ws2812_rmt_kconfig.hpp` and [`_config/Kconfig`](../_config/Kconfig) |
