# Babal (Flipper Zero)

Port of the classic HP-48 arcade game **Babal** (public domain, U-Foot / Christian Mauduit).

Drive a ball along an 8-lane perspective track of platforms and holes. Accelerate, steer, and jump — falling rewinds you 64 platforms. Finish the level as fast as you can.

Design plan: [`PLAN.md`](PLAN.md) · host render tests: [babal-render-tests skill](../../.cursor/skills/babal-render-tests/SKILL.md)

## Controls

| Key | Action |
|-----|--------|
| Left / Right | Steer (title: change level) |
| Up / Down | Accelerate / brake (title: setup / about) |
| OK | Jump / start / retry; toggle invert in setup |
| Back | Title / exit |

Setup (title → Up): invert display, bounce mode.

## Build & test

```bash
cd games/babal
ufbt
ufbt launch              # USB Flipper required
make -C tests test       # host render suite T1–T4
```

The `.fap` lands in `dist/`. After an intentional visual change, re-record goldens with `RECORD_GOLDEN=1 make -C tests record_golden`, then review and commit them.

## Render findings (vs original HP-48)

Compared Flipper soft-buffer frames to HP-48 LCD photos (`shot1` / cropped Babal screens) and U-Foot’s SVG Babal math notes. Early Flipper drafts looked wrong for three separate reasons:

1. **Polarity** — HP Babal is **dark ink on a light LCD** (solid dark pavés, light void). White-on-black “corridor” frames read as a different game.
2. **Fill style** — Original pavés are **solid filled trapezoids**, not wireframe spokes, dashed seams, or thin far-band planks.
3. **Ball** — HP draws a **chunky circle with 50% checkerboard dither** so the ball stays visible on dark platforms. A solid disc or bright outline reads as a cross/blob on 128×64.

### What we kept from SVG Babal

- Hyperbolic depth: `yViewport = c − a / (b + yLevel)` with anchors `y0` (near), `yBall ≈ H/4`, `yMax` (horizon).
- Thales lateral: `xViewport = (yMax − yViewport) · e · xLevel`.
- **Relative depth** `row − progress` (may be negative near the camera). Using absolute `progress + d` put the ball in the void under the near plane.
- Start at `progress = 0.5` (SVG `START_Y`) so the nearest cell straddles the ball plane.
- Ground sample under the ball at `floor(progress + LEVEL_Y_BALL)`.

### Flipper retunes (128×64)

| Choice | Why |
|--------|-----|
| Soft FB bit=1 = dark ink; blit white void + black dots | Match HP LCD; Setup → Invert swaps |
| Full `fill_trap` for every visible platform cell | Solid pavés, no wireframe artifacts |
| Checkerboard ball, `BABAL_BALL_R = 8` | SVG `BALL_SIZE=16` on H=64; chunky HP dithered GROB |
| `BABAL_VIEWPORT_X_NEAR = W/11` (not SVG `W/1.666`) | Literal SVG unit width fills the whole Flipper frame for solid near rows; retune keeps light side voids (~60% width at the ball plane), like the HP screenshots |
| Project pavé corners with Thales X (not `half_w >> 2`) | Truncating lane step bowed the track edges; SVG-style corner projection keeps straight vanishing lines |
| Draw far → near | Nearer pavés cover shared edges |

Constants live in [`render_math.h`](render_math.h). Host goldens under `tests/goldens/` encode this look; change anchors only with `RECORD_GOLDEN=1` and a visual check.

## Credit

Original Babal 2.0 by Christian Mauduit (U-Foot) for HP-48 — public domain. Perspective formulas from U-Foot’s SVG Babal notes (reimplemented in C; not a copy of the GPL SVG sources).
