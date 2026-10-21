#!/usr/bin/env python3
"""Generate a 128x64 2-bit Flipper-themed demo image as a C array."""

from __future__ import annotations

import pathlib
import struct
import zlib

W, H = 128, 64
OUT_C = pathlib.Path(__file__).resolve().parents[1] / "helpers" / "image_flipper.c"
OUT_H = pathlib.Path(__file__).resolve().parents[1] / "helpers" / "image_flipper.h"
OUT_PNG = pathlib.Path(__file__).resolve().parents[1] / "preview_flipper_2bit.png"

# Gray levels: 0=black, 1=dark, 2=light, 3=white
B, D, L, WHT = 0, 1, 2, 3


def clamp(v: int, lo: int, hi: int) -> int:
    return lo if v < lo else hi if v > hi else v


def in_ellipse(x: float, y: float, cx: float, cy: float, rx: float, ry: float) -> bool:
    if rx <= 0 or ry <= 0:
        return False
    return ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2 <= 1.0


def in_round_rect(x: float, y: float, x0: float, y0: float, x1: float, y1: float, r: float) -> bool:
    if x < x0 or x > x1 or y < y0 or y > y1:
        return False
    # Interior of rounded rectangle
    cx = clamp(x, x0 + r, x1 - r)
    cy = clamp(y, y0 + r, y1 - r)
    return (x - cx) ** 2 + (y - cy) ** 2 <= r * r or (
        x0 + r <= x <= x1 - r or y0 + r <= y <= y1 - r
    )


def paint() -> list[int]:
    pix = [WHT] * (W * H)

    def put(x: int, y: int, g: int) -> None:
        if 0 <= x < W and 0 <= y < H:
            pix[y * W + x] = g

    # Soft vertical sky gradient (light → mid)
    for y in range(H):
        g = L if y < 28 else (D if y < 48 else B)
        # smoother bands with slight dither of adjacent levels for silhouette edges later
        for x in range(W):
            # gentle left-right vignette
            edge = min(x, W - 1 - x)
            if edge < 6 and g > B:
                put(x, y, max(B, g - 1))
            else:
                put(x, y, g)

    # Ground strip
    for y in range(50, H):
        for x in range(W):
            put(x, y, D if (x + y) % 5 else B)

    # Flipper body (rounded rectangle) — 2-bit shaded
    body = (18, 10, 110, 46, 8)  # x0,y0,x1,y1,r
    for y in range(H):
        for x in range(W):
            if in_round_rect(x + 0.5, y + 0.5, *body):
                # Top bevel light, bottom dark
                t = (y - body[1]) / max(1, body[3] - body[1])
                if t < 0.18:
                    put(x, y, L)
                elif t < 0.55:
                    put(x, y, D)
                else:
                    put(x, y, B)

    # Screen bezel (dark) + screen (gradient showing all 4 levels)
    sx0, sy0, sx1, sy1 = 28, 16, 78, 40
    for y in range(sy0 - 2, sy1 + 3):
        for x in range(sx0 - 2, sx1 + 3):
            if sx0 - 2 <= x <= sx1 + 2 and sy0 - 2 <= y <= sy1 + 2:
                put(x, y, B)
    for y in range(sy0, sy1 + 1):
        for x in range(sx0, sx1 + 1):
            # Horizontal 4-level gradient across the "LCD"
            t = (x - sx0) / max(1, sx1 - sx0)
            if t < 0.25:
                put(x, y, B)
            elif t < 0.5:
                put(x, y, D)
            elif t < 0.75:
                put(x, y, L)
            else:
                put(x, y, WHT)

    # Tiny dolphin silhouette on the screen (black on gradient)
    # Compact pixel dolphin (classic Flipper mascot vibe, original art)
    dolphin = [
        "......####......",
        "....########....",
        "...##########...",
        "..####..######..",
        ".####.#.#######.",
        ".##############.",
        "..############..",
        "...##########...",
        "....###..####...",
        ".....#....##....",
    ]
    dx0, dy0 = 40, 20
    for j, row in enumerate(dolphin):
        for i, ch in enumerate(row):
            if ch == "#":
                put(dx0 + i, dy0 + j, B)

    # D-pad (left of buttons)
    cx, cy, r = 92, 28, 7
    for y in range(H):
        for x in range(W):
            if abs(x - cx) <= 2 and abs(y - cy) <= 6:
                put(x, y, B if abs(y - cy) > 1 else D)
            if abs(y - cy) <= 2 and abs(x - cx) <= 6:
                put(x, y, B if abs(x - cx) > 1 else D)
            if in_ellipse(x + 0.5, y + 0.5, cx, cy, 1.8, 1.8):
                put(x, y, L)

    # OK button
    for y in range(H):
        for x in range(W):
            if in_ellipse(x + 0.5, y + 0.5, 108, 28, 5.5, 5.5):
                put(x, y, B)
            if in_ellipse(x + 0.5, y + 0.5, 108, 28, 3.2, 3.2):
                put(x, y, D)

    # Back button small
    for y in range(H):
        for x in range(W):
            if in_ellipse(x + 0.5, y + 0.5, 108, 40, 3.5, 3.5):
                put(x, y, B)

    # Antenna nub
    for y in range(4, 12):
        for x in range(100, 106):
            put(x, y, D if y > 6 else B)
    for y in range(0, 6):
        for x in range(102, 108):
            if in_ellipse(x + 0.5, y + 0.5, 105, 3, 3.5, 2.5):
                put(x, y, B)

    # Title bar: four level swatches at top-left outside body
    labels_y = 2
    for i, g in enumerate((B, D, L, WHT)):
        x0 = 2 + i * 8
        for y in range(labels_y, labels_y + 5):
            for x in range(x0, x0 + 6):
                put(x, y, g)
                if y in (labels_y, labels_y + 4) or x in (x0, x0 + 5):
                    put(x, y, B if g != B else L)

    # Soft shadow under body
    for y in range(47, 52):
        for x in range(22, 108):
            if pix[y * W + x] == WHT or pix[y * W + x] == L:
                put(x, y, D if (x + y) % 2 == 0 else L)

    return pix


def pack(pix: list[int]) -> bytes:
    out = bytearray((W * H) // 4)
    for i, g in enumerate(pix):
        byte_i = i >> 2
        shift = (3 - (i & 3)) << 1
        out[byte_i] |= (g & 3) << shift
    return bytes(out)


def write_preview_png(pix: list[int], path: pathlib.Path) -> None:
    """Expand 2-bit → 8-bit grayscale PNG for README preview."""
    palette = {0: 0, 1: 85, 2: 170, 3: 255}
    raw = bytearray()
    for y in range(H):
        raw.append(0)
        for x in range(W):
            raw.append(palette[pix[y * W + x]])

    def chunk(tag: bytes, data: bytes) -> bytes:
        return (
            struct.pack(">I", len(data))
            + tag
            + data
            + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
        )

    ihdr = struct.pack(">IIBBBBB", W, H, 8, 0, 0, 0, 0)
    png = (
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", ihdr)
        + chunk(b"IDAT", zlib.compress(bytes(raw), 9))
        + chunk(b"IEND", b"")
    )
    path.write_bytes(png)


def emit_c(packed: bytes) -> None:
    OUT_H.write_text(
        """#pragma once

#include <stdint.h>
#include "gray2bit.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Full-screen 2-bit Flipper demo image (packed, see gray2bit_get). */
extern const uint8_t image_flipper_2bit[GRAY2BIT_PACKED_SIZE];

#ifdef __cplusplus
}
#endif
""",
        encoding="utf-8",
    )

    lines = [
        '#include "image_flipper.h"',
        "",
        "const uint8_t image_flipper_2bit[GRAY2BIT_PACKED_SIZE] = {",
    ]
    for i in range(0, len(packed), 16):
        chunk = ", ".join(f"0x{b:02X}" for b in packed[i : i + 16])
        lines.append(f"    {chunk},")
    lines.append("};")
    lines.append("")
    OUT_C.write_text("\n".join(lines), encoding="utf-8")


def main() -> None:
    pix = paint()
    packed = pack(pix)
    assert len(packed) == (W * H) // 4
    emit_c(packed)
    write_preview_png(pix, OUT_PNG)
    # Sanity: unique levels present
    levels = sorted(set(pix))
    print(f"wrote {OUT_C} ({len(packed)} bytes), levels={levels}, preview={OUT_PNG}")


if __name__ == "__main__":
    main()
