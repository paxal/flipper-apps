# Ice Cube port — findings from the HP-48 original

Source material: `ice1_1/` (ICE.ENT transfer file, docs, and the Saturn/System RPL
disassembly under `ice1_1/disasm/`). Engine listing: `disasm/code/code_00041.asm`
(ICE.EXE). Stage decoder: `disasm/code/code_14235.asm`. End-of-level screen:
`disasm/code/code_31865.asm`.

## Data format (what the first port got wrong)

- The 12 stage records in the PLAY array are **LZSS-compressed over nibbles**.
  Reading them raw gives noise; the first `levels_data.c` was exactly that, and
  its tests enshrined 78 pawns. The decoded maps sum to **300 pawns**, as the manual
  promises.
- Decoder (transcribed in `tools/gen_levels.py`): flag bit 1 = literal run (5-bit
  count 1..31, else 6-bit count+31, else 95 nibbles); flag 0 = match (3-bit length+4,
  else 5-bit length+12; 8-bit distance+1). A literal run shorter than 95 nibbles is
  always followed by a match without a flag bit. Bits are read MSB-first from bytes
  taken inline from the same stream as the literal nibbles. Saturn `B=B-1; GONC`
  loops run N+1 times, which is why every field is one bit wider than the `LC`
  constant suggests.
- A decoded stage is a list `{ title$  HXS  GROB }`. The HXS holds the time limit as
  **BCD seconds** (the engine counts down under SETDEC). The GROB body is the map:
  one byte per cell, width = GROB width / 8. Maps are 20 to 65 columns and 9 to 32
  rows, so both axes scroll.
- Every graphic string in PLAY uses the same compression. Decoded strings carry a
  10-nibble header (tag, width in pixels) then one or two bit planes. The map byte is
  the **index into the 192-tile bank** with no masking; the second plane is 192 tiles
  after the first. Titles: `THE PRISON`, `THE CASTLE`, `THE GLACIER`, `THE ??????`.
- Pictures recovered: intro (title), route map (level screen), cocktail picture
  (pause), six-panel comic (end of level), plus a score screen and a record-table
  background that the port does not use yet.

## Engine rules carried into `level.c` / `game.c`

| Byte(s) | Meaning |
|---|---|
| 0x00 | cube start (one per stage) |
| 0x02 0x03 0x10 0x10 0x10 | in-map TIME label; the engine writes digit tiles 0x0C+d into the three cells after 0x03 |
| 0x30 | pawn (becomes 0x01) |
| 0x31 0x32 | EXIT sign; ends the stage only when pawns = 0, solid before |
| 0x33 | mine, lethal, outcome BOOM |
| 0x34..0x37 | accelerators up/down/left/right |
| 0x38..0x3A | set cube state 1..3 (also picks the cube sprite) |
| 0x3B..0x3E | key slot: with state = byte-0x3B the cell to the right becomes floor |
| 0x3F | dynamite pickup |
| 0x40..0x7F | walls that **negate the velocity** (bounce); 0x40..0x6F are blasted to 0x04 |
| 0x80..0x89 | solid, stop |
| 0x8A..0x8F | lethal, outcome WORM |
| 0x90.. | water, lethal, outcome WATER |

- Every 8 frames the engine increments the low nibble of every bit-7 byte within its
  group of 16. That animates the water and makes the worm's lethal phase travel along
  its body; the port does the same every `ICE_ANIM_TICKS`.
- The blast clears the 8 neighbours of the cube's cell, never the centre.
- Exit codes 0..5 (cleared, time out, water, worm, boom, quit) select the comic panel
  in reading order; the original blacks out the other five, the port dims and frames.
- Movement: a per-axis signed velocity changed by ±2 per frame while a key is held,
  clamped to ±8, **no friction**. The port uses ±1 per tick clamped to ±3 at 20
  ticks/s. Because the original never loses speed on a bounce, a cube that touches a
  wall at speed 1 drifts away forever; the port drops one speed unit per bounce and
  holds still when the key is pushed into the wall.
- The cube sprite is 4 states × 8 rows of (mask byte, pixel byte): AND the mask,
  OR the pixels. It is 8×8, drawn identically on both planes.
- Viewport: 17×9 tiles drawn, the cube held at column 7 row 3 while scrolling.
  Pawns and score are never drawn; the HUD is the annunciators (ALERT = dynamite,
  BUSY = under 20 s, TRANSMITTING = all pawns taken).

## Still guessed

- Gray mapping: the HP shows both single-plane colours at the same duty. The
  generator maps first plane → dark, second → light; swap in `level_of()` if the
  device looks wrong.
- Speed and tick rate (`ICE_MAX_SPEED`, `ICE_TICKS_PER_SEC`) are by feel, not from
  the engine's frame rate.
- Scoring (100 per pawn, 200 + 10/s per stage, life every 1000) is the port's; only
  the extra life per thousand points is in the manual.
- The route map should blacken passed stages; the port only prints the title and
  remaining cubes on it.

## Tooling

- `python3 -I tools/gen_levels.py` regenerates `levels/levels_data.c`,
  `levels/gfx_data.c` and the preview PNGs from `ice1_1/ICE.ENT`.
- `make -C test check` runs host tests; `make -C test frames` renders every screen to
  `test/frames/*.png` for eyeballing without a device.
