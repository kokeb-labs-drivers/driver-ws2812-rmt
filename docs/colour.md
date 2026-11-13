# Colour

The colour order, RGBW, brightness and gamma: what goes on the wire for a colour you set.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    - 24 bit data, GRB, high bit first: Worldsemi WS2812B-V5 V5.0, Composition of 24bit Data, p. 4
    - 32 bit data and its order: RGBW (revisions 01 and 03): Opsco SK6812RGBX-XX Rev. 03, 13. The data structure of
      32bit, p. 6
    - 32 bit data and its order: GRBW: Opsco SK6812RGBX-XX Rev. 07, 13. The data structure of 32bit, p. 9

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

## What a pixel receives

A pixel takes 24 bits (32 on SK6812RGBW), high bit first, one byte per colour channel, and the order of the channels
is fixed by the part. Pixel 0 is the one nearest the controller; a frame is every pixel back to back, then the reset.

| Parts | Order on the wire | Datasheet |
| --- | --- | --- |
| WS2812, WS2812B, WS2812C, WS2812D, SK6812, SK6812MINI | G, R, B (`kOrderGrb`) | V5, p. 4 |
| SK6812RGBW Rev 07 | G, R, B, W (`kOrderGrbw`) | skw-07, p. 9 |
| SK6812RGBW Rev 01 and Rev 03 | R, G, B, W (`kOrderRgbw`) | skw-03, p. 6 |

The three SK6812RGBW documents disagree. `Variant::Sk6812Rgbw` defaults to GRBW, the later revision. If red and green
are swapped on your strip, it is an older part: set the order to RGBW.

```cpp title="api_tasks.cpp"
--8<-- "tests/docs/api_tasks.cpp:rgbw"
```

The order is data: a `ColourOrder` of up to four channels. Any permutation of R, G, B and (for four channels) W is
valid, which covers clone parts that use another order. `Validate()` refuses an order with a repeated channel, a
white channel in a three-channel order, or a count other than 3 or 4.

```cpp title="colour.cpp"
--8<-- "tests/docs/colour.cpp:encode"
```

## Brightness and gamma

`Config::brightness` and `SetBrightness()` scale every channel linearly at `Show()`: 255 leaves a value unchanged, 0
is off, and the value is rounded, not truncated. The stored colours are never changed, so brightness is reversible.
Gamma, off by default, maps each value through a 2.2 table before brightness:

```cpp title="api_tasks.cpp"
--8<-- "tests/docs/api_tasks.cpp:brightness"
```

The pixels have 256 PWM levels per channel and nothing else: brightness and gamma are convenience of the driver, not
chip features. A strip dimmed by brightness 20 has about 5 levels per channel left; if you need smooth fades at low
light, dim with the strip's supply or work in a wider colour type of your own.

## White

An RGBW pixel has a white die. `ExtractWhite()` moves the common part of red, green and blue into white, for a strip
that should show an RGB colour with the white die. It is never applied implicitly:

```cpp title="colour.cpp"
--8<-- "tests/docs/colour.cpp:white"
```

## Wheel

`Wheel()` returns a fully saturated colour for a position from 0 to 255 (red to green to blue to red). It is a helper
for demos; the driver has no animations or effects:

```cpp title="colour.cpp"
--8<-- "tests/docs/colour.cpp:wheel"
```
