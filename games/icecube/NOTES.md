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
  ticks/s. Bounce walls negate the velocity with no loss, as in the original, so a
  slow cube keeps drifting between walls; a tap the other way stops it. The port adds
  one rule: if the key toward the wall is held (or tapped) in the tick of the impact,
  the cube stops against the wall instead of bouncing, so it can be parked.
- Collision: all four corners of the box are checked for lethal tiles before any
  solid one, so a step that touches a block and water at once always kills.
- The cube sprite is 4 states × 8 rows of (mask byte, pixel byte): AND the mask,
  OR the pixels. It is 8×8, drawn identically on both planes.
- Viewport: 17×9 tiles drawn, the cube held at column 7 row 3 while scrolling.
  Pawns and score are never drawn; the HUD is the annunciators (ALERT = dynamite,
  BUSY = under 20 s, TRANSMITTING = all pawns taken). The port shows 16×8 tiles,
  the whole screen, with the camera leading 8 px (x) and 5 px (y) per unit of speed.
  Lives, pawns (EXIT at zero) and dynamite sit in a 43×9 box in a screen corner. The
  box moves when the cube comes within 12 px, to the corner farthest from the cube
  that is not where the camera leads. It blinks inverted under 20 s.

## Stage flow (`ICE.ENT.rpl`, PLAY secondary; route menu `code_25204.asm`)

| LAM | Holds |
|---|---|
| 4 | time limit (HXS, BCD seconds) |
| 5 | map GROB, passed to ICE.EXE without TOTEMPOB, so the engine edits it in place |
| 6 | stage title |
| 7 | score |
| 8 | stage index, 0-based; also the number of passed stages |
| 9 | 0x10 when CONTINUE is offered, else 0; reset after every menu |
| 10 | lives |

- Route menu: START (1) decodes the stage again into LAMs 4-6; CONTINUE (2) runs
  ICE.EXE on the map as left; QUIT (3) leaves the game. CONTINUE is drawn only when
  bit 4 of its argument (LAM 9 + LAM 8) is set.
- After the comic, exit 0 (cleared) advances the stage and loads only its title.
  Any other exit costs a life. Exit 1 (time out) stops there. Exits 2..5 (water,
  worm, boom, quit) store the **remaining time** as the new limit and set LAM 9, so
  CONTINUE resumes with the map as left, the remaining time, and (per the manual) a
  new white cube without dynamite. Quitting a stage therefore costs a life in the
  original too; the port does the same and goes to the route menu, whose QUIT
  returns to the title.
- The loop ends when lives reach 0 or the stage index reaches 12.
- Passed stages: the menu fills the second plane at rows 15..43, x = 12·k for each
  passed stage, over the scenery band of the route picture. The port turns white
  pixels there dark gray.

## Display

- Gray (stagger FRC, baked every 16 ms) is used only on the title, route and comic
  pictures. Play, pause, game over and win are shown thresholded (levels 0-1 black,
  2-3 white), baked only when a frame changes. Measured on the tile bank: 35 of 192
  tiles use gray and 26% of map cells use one, but all stay legible thresholded. The
  tiles drawn only in gray are the EXIT sign (0x31, 0x32) and the accelerators
  (0x34..0x37); they keep readable shapes. Bayer dithering made pawns and arrows
  noisy. The pause and route pictures are single-plane anyway.

## Still guessed

- Gray mapping: the HP shows both single-plane colours at the same duty. The
  generator maps first plane → dark, second → light; swap in `level_of()` if the
  device looks wrong.
- Speed and tick rate (`ICE_MAX_SPEED`, `ICE_TICKS_PER_SEC`) are by feel, not from
  the engine's frame rate.
- Scoring (100 per pawn, 200 + 10/s per stage, life every 1000) is the port's; only
  the extra life per thousand points is in the manual.
- Passed stages on the route map: the manual says they "turn black"; the second
  plane alone maps to light gray in `level_of()`, but the port uses dark gray so the
  marks read clearly.
- Feedback (sounds, vibration, LED) is the port's; the HP-48 had none.

## Tooling

- `python3 -I tools/gen_levels.py` regenerates `levels/levels_data.c`,
  `levels/gfx_data.c` and the preview PNGs from `ice1_1/ICE.ENT`.
- `make -C test check` runs host tests; `make -C test frames` renders every screen to
  `test/frames/*.png` for eyeballing without a device.
