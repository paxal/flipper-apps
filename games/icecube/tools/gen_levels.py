#!/usr/bin/env python3
"""Extract ICE-CUBE 1.1 stages and graphics from ice1_1/ICE.ENT.

Every stage record and every graphic string in the PLAY secondary is packed
with the same LZSS-over-nibbles scheme. The decoder is the 512-nibble code
object at PLAY nibble 14235 (ice1_1/disasm/code/code_14235.asm):

  flag 1 -> literal run: 5-bit count 1..31, or 0 then 6-bit count+31,
            or 0 again for 95 nibbles (that case returns to the flag loop);
            any other literal run is followed by a match with no flag bit.
  flag 0 -> match: 3-bit length+4 (5..11) or 0 then 5-bit length+12,
            8-bit distance+1, copied nibble by nibble (overlap allowed).
  Bits are consumed MSB first from bytes taken inline from the stream, and
  literal nibbles are also taken inline, so the two interleave.

A decoded stage is a list { title$  HXS  GROB }: the HXS holds the time
limit as BCD seconds, the GROB body is the tile map, one byte per cell,
width = GROB width / 8. Graphic strings decode to a string whose body is a
10-nibble header (tag, width in pixels) followed by 1 or 2 bit planes.
"""

import re
import struct
import sys
import zlib
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
ENT = REPO / "ice1_1" / "ICE.ENT"
OUT_DIR = Path(__file__).resolve().parents[1] / "levels"
OUT_LEVELS = OUT_DIR / "levels_data.c"
OUT_GFX = OUT_DIR / "gfx_data.c"
OUT_PNG = OUT_DIR / "preview_stage01.png"
OUT_BANK_PNG = OUT_DIR / "preview_tiles.png"

DOARRY = 0x029E8
DOCSTR = 0x02A2C
DOHSTR = 0x02A4E
DOLIST = 0x02A74
DOGROB = 0x02B1E
SEMI = 0x0312B

PLAY_START = 6025
PLAY_END = 39713
TILE_COUNT = 192
SCREEN_W = 128
SCREEN_H = 64


def rd(n, i, k):
    return sum(n[i + j] << (4 * j) for j in range(k))


def load_hp(path):
    data = path.read_bytes()
    if data[:7] != b"HPHP48-":
        raise SystemExit(f"{path} is not an HP-48 transfer file")
    nibs = []
    for b in data[8:]:
        nibs.append(b & 15)
        nibs.append(b >> 4)
    return nibs


class Bits:
    def __init__(self, n, pos):
        self.n = n
        self.pos = pos
        self.buf = 0
        self.cnt = 0

    def bit(self):
        if self.cnt == 0:
            self.buf = self.n[self.pos] | (self.n[self.pos + 1] << 4)
            self.pos += 2
            self.cnt = 8
        self.cnt -= 1
        b = (self.buf >> 7) & 1
        self.buf = (self.buf << 1) & 0xFF
        return b

    def bits(self, k):
        v = 0
        for _ in range(k):
            v = (v << 1) | self.bit()
        return v


def decompress(n, pos):
    size = rd(n, pos, 5)
    br = Bits(n, pos + 5)
    out = []
    while len(out) < size:
        if br.bit():
            cnt = br.bits(5)
            if cnt == 0:
                cnt = br.bits(6)
                if cnt == 0:
                    for _ in range(0x5F):
                        out.append(n[br.pos])
                        br.pos += 1
                    continue
                cnt += 0x1F
            for _ in range(cnt):
                out.append(n[br.pos])
                br.pos += 1
            if len(out) >= size:
                break
        ln = br.bits(3)
        ln = ln + 4 if ln else br.bits(5) + 12
        dist = br.bits(8) + 1
        for _ in range(ln):
            out.append(out[-dist])
    if len(out) != size:
        raise SystemExit("decompressed size mismatch")
    return out


def to_bytes(nibs):
    nibs = list(nibs) + [0] * (len(nibs) & 1)
    return bytes(nibs[i] | (nibs[i + 1] << 4) for i in range(0, len(nibs), 2))


def parse_objects(n):
    i = 5 if rd(n, 0, 5) == DOLIST else 0
    items = []
    while i + 5 <= len(n):
        pro = rd(n, i, 5)
        if pro == SEMI:
            break
        if pro not in (DOCSTR, DOHSTR, DOGROB):
            raise SystemExit(f"unexpected prologue {pro:05X}")
        ln = rd(n, i + 5, 5)
        items.append((pro, n[i + 10 : i + 5 + ln]))
        i += 5 + ln
    return items


def find_stage_array(n):
    for i in range(PLAY_START, PLAY_END):
        if rd(n, i, 5) == DOARRY and rd(n, i + 10, 5) == DOCSTR:
            return i
    raise SystemExit("stage array not found")


def read_stages(n):
    arr = find_stage_array(n)
    ndim = rd(n, arr + 15, 5)
    dims = [rd(n, arr + 20 + 5 * k, 5) for k in range(ndim)]
    cur = arr + 20 + 5 * ndim
    stages = []
    for _ in range(dims[0]):
        ln = rd(n, cur, 5)
        items = parse_objects(decompress(n, cur + 5))
        kinds = [k for k, _ in items]
        if kinds != [DOCSTR, DOHSTR, DOGROB]:
            raise SystemExit("stage record layout changed")
        title = to_bytes(items[0][1]).decode("latin1")
        hxs = to_bytes(items[1][1])
        grob = items[2][1]
        height = rd(grob, 0, 5)
        width_bits = rd(grob, 5, 5)
        cols = width_bits // 8
        body = to_bytes(grob[10:])
        time_sec = int(f"{hxs[1]:02x}{hxs[0]:02x}")
        stages.append(
            {
                "title": re.sub(r"\s+", " ", title).strip(),
                "time": time_sec,
                "w": cols,
                "h": height,
                "cells": body[: cols * height],
            }
        )
        cur += ln
    return stages


def read_graphics(n):
    """Decode every compressed string in PLAY, in file order."""
    pics = []
    i = PLAY_START + 5
    while i + 10 < min(PLAY_END, len(n)):
        if rd(n, i, 5) != DOCSTR:
            i += 1
            continue
        ln = rd(n, i + 5, 5)
        pos = i + 10
        try:
            out = decompress(n, pos)
            items = parse_objects(out)
            if items and items[0][0] == DOCSTR:
                content = items[0][1]
                width = rd(content, 5, 5)
                data = content[10:]
                pics.append({"at": i, "w": width, "nibbles": data})
        except (SystemExit, IndexError):
            pass
        i += 5 + ln
    return pics


def plane_rows(nibs, width, planes):
    stride = ((width + 7) // 8) * 2
    rows = len(nibs) // stride // planes
    out = []
    for p in range(planes):
        plane = []
        for y in range(rows):
            base = p * rows * stride + y * stride
            plane.append([(nibs[base + x // 4] >> (x % 4)) & 1 for x in range(width)])
        out.append(plane)
    return out


def level_of(a, b):
    """gray2bit level: both planes = black, plane A = dark, plane B = light."""
    if a and b:
        return 0
    if a:
        return 1
    if b:
        return 2
    return 3


def pack_2bpp(levels, width, height):
    packed = bytearray((width * height + 3) // 4)
    for y in range(height):
        for x in range(width):
            idx = y * width + x
            shift = (3 - (idx & 3)) * 2
            packed[idx >> 2] |= (levels[y][x] & 3) << shift
    return bytes(packed)


def picture_levels(pic, planes):
    pl = plane_rows(pic["nibbles"], pic["w"], planes)
    rows = len(pl[0])
    levels = []
    for y in range(SCREEN_H):
        row = []
        for x in range(SCREEN_W):
            if y < rows and x < pic["w"]:
                a = pl[0][y][x]
                b = pl[1][y][x] if planes == 2 else a
                row.append(level_of(a, b))
            else:
                row.append(3)
        levels.append(row)
    return levels


def tile_levels(bank):
    pl = plane_rows(bank["nibbles"], 8, 1)[0]
    tiles = []
    for t in range(TILE_COUNT):
        rows = []
        for y in range(8):
            a = pl[t * 8 + y]
            b = pl[(TILE_COUNT + t) * 8 + y]
            rows.append([level_of(a[x], b[x]) for x in range(8)])
        tiles.append(rows)
    return tiles


def c_bytes(name, data, per_line=16):
    lines = [f"const uint8_t {name}[{len(data)}] = {{"]
    for i in range(0, len(data), per_line):
        chunk = ", ".join(f"0x{b:02x}" for b in data[i : i + per_line])
        lines.append(f"    {chunk},")
    lines.append("};")
    return "\n".join(lines)


def c_string(text):
    return '"' + text.replace("\\", "\\\\").replace('"', '\\"') + '"'


def write_png(path, width, height, rows):
    def chunk(tag, data):
        crc = zlib.crc32(tag + data) & 0xFFFFFFFF
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", crc)

    raw = b"".join(b"\x00" + bytes(row) for row in rows)
    ihdr = struct.pack(">IIBBBBB", width, height, 8, 0, 0, 0, 0)
    path.write_bytes(
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", ihdr)
        + chunk(b"IDAT", zlib.compress(raw, 9))
        + chunk(b"IEND", b"")
    )


GRAY = {0: 0, 1: 85, 2: 170, 3: 255}


def write_stage_preview(stage, tiles, tile_index):
    scale = 2
    w, h = stage["w"], stage["h"]
    rows = [[255] * (w * 8 * scale) for _ in range(h * 8 * scale)]
    for i, raw in enumerate(stage["cells"]):
        tx, ty = i % w, i // w
        tile = tiles[tile_index(raw)]
        for ly in range(8):
            for lx in range(8):
                v = GRAY[tile[ly][lx]]
                for sy in range(scale):
                    for sx in range(scale):
                        rows[(ty * 8 + ly) * scale + sy][(tx * 8 + lx) * scale + sx] = v
    write_png(OUT_PNG, w * 8 * scale, h * 8 * scale, rows)


def write_bank_preview(tiles):
    per_row = 16
    scale = 3
    n_rows = (TILE_COUNT + per_row - 1) // per_row
    wpx = per_row * 9 * scale
    hpx = n_rows * 9 * scale
    rows = [[128] * wpx for _ in range(hpx)]
    for t, tile in enumerate(tiles):
        tx, ty = t % per_row, t // per_row
        for ly in range(8):
            for lx in range(8):
                v = GRAY[tile[ly][lx]]
                for sy in range(scale):
                    for sx in range(scale):
                        rows[(ty * 9 + ly) * scale + sy][(tx * 9 + lx) * scale + sx] = v
    write_png(OUT_BANK_PNG, wpx, hpx, rows)


def tile_index_default(raw):
    return raw if raw < TILE_COUNT else 0


def main():
    n = load_hp(ENT)
    stages = read_stages(n)
    pics = read_graphics(n)

    by_size = {}
    for p in pics:
        by_size.setdefault((p["w"], len(p["nibbles"])), []).append(p)
    bank = [p for p in pics if p["w"] == 8][0]
    cube = [p for p in pics if p["w"] == 16][0]
    full2 = [p for p in pics if p["w"] in (131, 133) and len(p["nibbles"]) == 4352]
    route = [p for p in pics if p["w"] == 132][0]
    title = full2[0]  # "Ice-Cube v1.1 by Lilian PIGALLIO", shown at start
    comic = full2[1]  # six end-of-level panels
    pause = [p for p in pics if p["w"] == 131 and len(p["nibbles"]) == 2108][0]

    tiles = tile_levels(bank)

    parts = [
        '#include "levels/levels_data.h"',
        "",
        "/* Generated by tools/gen_levels.py from ice1_1/ICE.ENT. Do not edit.",
        " * Maps are one byte per cell, row-major, width x height cells.",
        " * Time limits are the BCD seconds stored in each stage record.",
        " */",
        "",
    ]
    table = []
    pawn_total = 0
    max_w = max_h = max_cells = 0
    for idx, st in enumerate(stages, 1):
        name = f"stage_{idx:02d}_cells"
        pawns = st["cells"].count(0x30)
        pawn_total += pawns
        max_w = max(max_w, st["w"])
        max_h = max(max_h, st["h"])
        max_cells = max(max_cells, st["w"] * st["h"])
        parts.append(c_bytes(f"{name}", st["cells"]).replace("const uint8_t", "static const uint8_t", 1))
        parts.append("")
        table.append(
            f"    {{{c_string(st['title'])}, {name}, {st['w']}, {st['h']}, {pawns}, {st['time']}}},"
        )
        print(f"{idx:2d}. {st['title']:<22} {st['w']}x{st['h']} pawns {pawns:2d} time {st['time']}")
    parts.append("const IceStageDef ice_stages[ICE_STAGE_COUNT] = {")
    parts.extend(table)
    parts.append("};")
    parts.append("")
    parts.append(f"const uint16_t ice_pawn_total = {pawn_total};")
    parts.append("")
    OUT_LEVELS.write_text("\n".join(parts))
    print(f"wrote {OUT_LEVELS}: pawn total {pawn_total}, max {max_w}x{max_h} = {max_cells} cells")
    if max_cells > 2080 or len(stages) != 12:
        raise SystemExit("levels_data.h limits need updating")

    tile_bytes = bytearray()
    for tile in tiles:
        tile_bytes += pack_2bpp(tile, 8, 8)
    cube_rows = plane_rows(cube["nibbles"], 16, 1)[0]
    cube_bytes = bytearray()
    for row in cube_rows:
        cube_bytes += bytes([sum(row[x] << x for x in range(8)), sum(row[8 + x] << x for x in range(8))])

    gfx = [
        '#include "levels/gfx_data.h"',
        "",
        "/* Generated by tools/gen_levels.py from ice1_1/ICE.ENT. Do not edit.",
        " * Tiles and pictures are gray2bit packed (4 px per byte, MSBits first,",
        " * 0 = black, 1 = dark, 2 = light, 3 = white): both HP planes = black,",
        " * first plane only = dark, second plane only = light.",
        " * Pictures are the left 128 columns of the 131-px HP-48 screen.",
        " */",
        "",
        c_bytes("ice_tiles", tile_bytes),
        "",
        c_bytes("ice_cube_rows", cube_bytes),
        "",
        c_bytes("ice_pic_comic", pack_2bpp(picture_levels(comic, 2), SCREEN_W, SCREEN_H)),
        "",
        c_bytes("ice_pic_route", pack_2bpp(picture_levels(route, 1), SCREEN_W, SCREEN_H)),
        "",
        c_bytes("ice_pic_pause", pack_2bpp(picture_levels(pause, 1), SCREEN_W, SCREEN_H)),
        "",
        c_bytes("ice_pic_title", pack_2bpp(picture_levels(title, 2), SCREEN_W, SCREEN_H)),
        "",
    ]
    OUT_GFX.write_text("\n".join(gfx))
    print(f"wrote {OUT_GFX}: {len(tiles)} tiles, cube rows {len(cube_rows)}, pictures at {[p['at'] for p in (comic, route, pause, title)]}")

    write_stage_preview(stages[0], tiles, tile_index_default)
    write_bank_preview(tiles)
    print(f"wrote {OUT_PNG} and {OUT_BANK_PNG}")


if __name__ == "__main__":
    sys.exit(main())
