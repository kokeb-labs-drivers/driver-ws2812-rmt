# WS2812_RMT host example

The driver on your PC, with no hardware: the host port in
[`host_ws2812_rmt_port.hpp`](host_ws2812_rmt_port.hpp) connects the real driver to a simulated transmit path (the model
the host tests use), and [`main.cpp`](main.cpp) shows one pixel, a rainbow and an RGBW frame, then reads back what a
simulated chain of pixels latched.

## Build and run

From the repository root, with CMake 3.16+ and a C++20 compiler:

```bash
cmake -S examples/host -B build-example
cmake --build build-example
./build-example/ws2812_rmt_host_example
```

## Expected output

```text
WS2812_RMT driver 0.1.0 on the host
frame time 550 us for 8 pixels
one pixel: 123456 000000
wire bytes: 34 12 56
rainbow at brightness 64: 400000 281800 103000 003808 002020 000838 11002F 290017
violations 0, gaps 0
RGBW wire (G R B W): 32 96 00 32
```

`violations 0, gaps 0` means every pulse the driver sent lay inside the datasheet windows of the model. The host
tests build this folder too and run the example (CTest `ws2812_rmt_host_example`), so CI fails if it stops
compiling or a call fails.

## Files

| File | What it is |
| --- | --- |
| [`main.cpp`](main.cpp) | The example: start, set, show, read back |
| [`host_ws2812_rmt_port.hpp`](host_ws2812_rmt_port.hpp) | The host port: the simulated transmit path, `DelayUs()`, `GpioSet()` and `Log()` |
| [`CMakeLists.txt`](CMakeLists.txt) | Standalone project; adds the driver with `add_subdirectory(../..)` |
