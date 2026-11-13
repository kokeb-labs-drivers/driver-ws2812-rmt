#!/usr/bin/env python3
"""Write inc/ws2812_rmt_gamma.hpp: the 256-entry gamma 2.2 table, round(255 * (v / 255) ** 2.2).

    python3 tools/gen_gamma.py            # rewrite the header
    python3 tools/gen_gamma.py --check    # exit 1 when the header differs from what this script writes

The host test tests/test_colour.cpp recomputes every entry with std::pow and compares.
"""
from __future__ import annotations

import math
import sys
from pathlib import Path

HEADER = Path(__file__).resolve().parents[1] / "inc" / "ws2812_rmt_gamma.hpp"


def entries() -> list[int]:
    return [int(math.floor(255.0 * (v / 255.0) ** 2.2 + 0.5)) for v in range(256)]


def render() -> str:
    values = entries()
    rows = []
    for i in range(0, 256, 16):
        rows.append("    " + ", ".join(f"{v:3d}" for v in values[i:i + 16]) + ",")
    body = "\n".join(rows)
    return f"""// clang-format off: the table keeps sixteen entries per row.
/**
 * @file ws2812_rmt_gamma.hpp
 * @brief The gamma 2.2 table of the WS2812_RMT driver, written by tools/gen_gamma.py.
 * @copyright Copyright (c) 2026 The driver-ws2812-rmt authors. SPDX-License-Identifier: MIT
 *
 * @details
 * `kGamma22[v]` is `round(255 * (v / 255) ^ 2.2)` with halves rounded up. Gamma is not a feature of
 * the pixels; it is a convenience, off by default (`Gamma::None`).
 */
// clang-format on
#ifndef WS2812_RMT_GAMMA_HPP
#define WS2812_RMT_GAMMA_HPP

#include <array>
#include <cstdint>

namespace ws2812_rmt {{

/** @brief The gamma 2.2 transfer curve for 8 bit values. */
inline constexpr std::array<uint8_t, 256> kGamma22{{{{
{body}
}}}};

}} // namespace ws2812_rmt

#endif // WS2812_RMT_GAMMA_HPP
"""


def main() -> int:
    text = render()
    if "--check" in sys.argv:
        return 0 if HEADER.read_text() == text else 1
    HEADER.write_text(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
