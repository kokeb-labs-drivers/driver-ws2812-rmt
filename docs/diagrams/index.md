# Figures

<!-- Written by kokeb-docs figures from docs/diagrams/figures/*.py. Do not edit by hand. -->

Every figure on this site is drawn by a Python module from facts extracted from the code, so it is rebuilt,
not redrawn, when the code changes. CI runs `kokeb-docs figures --check` and fails when a
committed figure no longer matches the code. Each figure below lists the files it is drawn from.

## Inside the WS2812B

<!-- figure: chip-blocks -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-blocks.dark.svg">
  <img src="chip-blocks.svg"
    alt="Block diagram of the WS2812B: its function blocks, how they connect and which package pins reach
    each block.">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/ws2812_rmt.hpp`](../../inc/ws2812_rmt.hpp)
- [`inc/ws2812_rmt_gpio_interface.hpp`](../../inc/ws2812_rmt_gpio_interface.hpp)

## WS2812B: Level shifter for a 3.3 V MCU, supply always on

<!-- figure: chip-circuit-shifted -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-circuit-shifted.dark.svg">
  <img src="chip-circuit-shifted.svg"
    alt="Schematic of the WS2812B level shifter for a 3.3 v mcu, supply always on: the chip with U2 MCU, U4
    74HCT buffer, R1 resistor 330 Ω, C1 capacitor 100 nF, R2 resistor 10 kΩ; the MCU wires carry the
    port hooks GpioSet(CtrlPin::Power), Transmit().">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/ws2812_rmt.hpp`](../../inc/ws2812_rmt.hpp)
- [`inc/ws2812_rmt_gpio_interface.hpp`](../../inc/ws2812_rmt_gpio_interface.hpp)

## WS2812B: Typical application

<!-- figure: chip-circuit-typical -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-circuit-typical.dark.svg">
  <img src="chip-circuit-typical.svg"
    alt="Schematic of the WS2812B typical application: the chip with U2 MCU, U3 Load switch, U4 74HCT buffer,
    J1 5 V supply, R1 resistor 330 Ω, C1 capacitor 100 nF, R2 resistor 10 kΩ; the MCU wires carry the
    port hooks GpioSet(CtrlPin::Power), Transmit().">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/ws2812_rmt.hpp`](../../inc/ws2812_rmt.hpp)
- [`inc/ws2812_rmt_gpio_interface.hpp`](../../inc/ws2812_rmt_gpio_interface.hpp)

## WS2812B package, 5050 (WS2812B)

<!-- figure: chip-outline -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-outline.dark.svg">
  <img src="chip-outline.svg"
    alt="Outline of the WS2812B in 5050 (WS2812B) drawn to scale from above: body 5.4 x 5.0 x 1.57 mm, 4
    pins.">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/ws2812_rmt.hpp`](../../inc/ws2812_rmt.hpp)
- [`inc/ws2812_rmt_gpio_interface.hpp`](../../inc/ws2812_rmt_gpio_interface.hpp)

## WS2812B pinout, 5050 (WS2812B)

<!-- figure: chip-pinout -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-pinout.dark.svg">
  <img src="chip-pinout.svg"
    alt="Pinout of the WS2812B in 5050 (WS2812B), top view: 4 pins, each with its name, its electrical type
    as a colour and the driver hook that uses it.">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/ws2812_rmt.hpp`](../../inc/ws2812_rmt.hpp)
- [`inc/ws2812_rmt_gpio_interface.hpp`](../../inc/ws2812_rmt_gpio_interface.hpp)

## The layers, one of them yours

<!-- figure: driver-layers -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="driver-layers.dark.svg">
  <img src="driver-layers.svg"
    alt="Layer diagram: your application calls the Ws2812Rmt core, which sends whole frames of pixel bytes
    through the GPIO port contract to the port class you write, which drives the chip; the core and the
    contract are the same on every MCU.">
</picture>
<!-- /figure -->

Figure module: [`docs/diagrams/figures/driver-layers.py`](../../docs/diagrams/figures/driver-layers.py)

Drawn from:

- `public calls` in [`inc/ws2812_rmt.hpp`](../../inc/ws2812_rmt.hpp)
- `GpioInterface` in [`inc/ws2812_rmt_gpio_interface.hpp`](../../inc/ws2812_rmt_gpio_interface.hpp)
