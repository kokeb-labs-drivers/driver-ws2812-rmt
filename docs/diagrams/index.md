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

## What the WS2812B does

<!-- figure: chip-capabilities -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-capabilities.dark.svg">
  <img src="chip-capabilities.svg"
    alt="Capability map of the WS2812B: 24-bit pixel colour (supported): Stores the colours and sends each
    pixel as G, R, B, high bit first; the colour order is a Config field; Cascaded pixels (supported):
    Sends a whole strip in one frame: each pixel keeps its 24 bits and forwards the rest; Bit timing
    (supported): Turns the datasheet pulse windows into ticks of the rate your port really runs at, or
    refuses; Frame latch (supported): Ends a frame with a low longer than the reset time (above 280 µs
    on the V5 sheet) so the pixels latch; 256 brightness levels (supported): Scales the colours before
    sending, with an optional gamma table; the chip itself takes 8 bits per colour; Shared power source
    (supported): Logic and LEDs share one supply, so the driver can switch the strip's supply from an
    MCU pin; Other generations (supported): Timing windows for the WS2812 and WS2812B generations,
    WS2812C and D, and SK6812 parts, or a Custom timing; Input level (not covered): DIN needs at least
    2.7 V on the V5 sheet; the driver cannot see the level, so a level shifter is yours.">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/ws2812_rmt.hpp`](../../inc/ws2812_rmt.hpp)
- [`inc/ws2812_rmt_gpio_interface.hpp`](../../inc/ws2812_rmt_gpio_interface.hpp)

## WS2812B: Direct data line

<!-- figure: chip-circuit-direct -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-circuit-direct.dark.svg">
  <img src="chip-circuit-direct.svg"
    alt="Schematic of the WS2812B direct data line: the chip with U2 MCU, J1 5 V supply, R1 resistor 330 Ω,
    C1 capacitor 100 nF; the MCU wires carry the port hooks GpioSet(CtrlPin::Power), Transmit().">
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

## WS2812B: Pixel data

<!-- figure: chip-frame-pixel -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-frame-pixel.dark.svg">
  <img src="chip-frame-pixel.svg"
    alt="Pixel data of the WS2812B, 24 bits, most significant bit first. DIN: G7 to G0 (bits 23 to 16), R7 to
    R0 (bits 15 to 8), B7 to B0 (bits 7 to 0).">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/ws2812_rmt.hpp`](../../inc/ws2812_rmt.hpp)
- [`inc/ws2812_rmt_gpio_interface.hpp`](../../inc/ws2812_rmt_gpio_interface.hpp)

## WS2812B

<!-- figure: chip-hero -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-hero.dark.svg">
  <img src="chip-hero.svg"
    alt="Overview of the WS2812B, an addressable RGB LED with its control circuit in one 5050 package: pixels
    chain on a single data wire, each keeps the first 24 bits it receives for its three LEDs and passes
    the rest on to the next pixel. A MCU talks to it over one data wire; it drives Red, green and blue
    LEDs, then Next pixel. Headline figures: 800 kbit/s data rate on one wire; 24 bit per pixel: G, R,
    B, 8 bits each; 256 brightness levels per colour; > 280 µs low time that latches a frame.">
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

## Wiring: Direct data line

<!-- figure: chip-wiring-direct -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-wiring-direct.dark.svg">
  <img src="chip-wiring-direct.svg"
    alt="Wiring of the WS2812B for the Direct data line: MCU RMT data to DIN through R1 330 Ω (Transmit()).
    Supplies, grounds and the support parts are listed under the drawing.">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/ws2812_rmt.hpp`](../../inc/ws2812_rmt.hpp)
- [`inc/ws2812_rmt_gpio_interface.hpp`](../../inc/ws2812_rmt_gpio_interface.hpp)

## Wiring: Level shifter for a 3.3 V MCU, supply always on

<!-- figure: chip-wiring-shifted -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-wiring-shifted.dark.svg">
  <img src="chip-wiring-shifted.svg"
    alt="Wiring of the WS2812B for the Level shifter for a 3.3 V MCU, supply always on: . Supplies, grounds
    and the support parts are listed under the drawing.">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/ws2812_rmt.hpp`](../../inc/ws2812_rmt.hpp)
- [`inc/ws2812_rmt_gpio_interface.hpp`](../../inc/ws2812_rmt_gpio_interface.hpp)

## Wiring: Typical application

<!-- figure: chip-wiring-typical -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-wiring-typical.dark.svg">
  <img src="chip-wiring-typical.svg"
    alt="Wiring of the WS2812B for the Typical application: . Supplies, grounds and the support parts are
    listed under the drawing.">
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
