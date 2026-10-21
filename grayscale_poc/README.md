# Grayscale POC

Proof-of-concept **2-bit grayscale** (4 levels) on the Flipper Zero’s native 1-bit LCD, using the same tricks calculator scene programmers used on **TI-83/89** and **HP-48**.

The panel can only drive each pixel on or off. Perceived gray comes from either **temporal** plane switching (TI/HP FRC) or **spatial** dither (HP GROB / Babal checkerboard family).

## Heritage

| Source | Idea we borrow |
|--------|----------------|
| [TIGCC grayscale tutorial](http://tigcc.ticalc.org/tut/grayscale.html) (TI-89/92+) | Two bitplanes; show the dark plane ~⅔ of the time and the light plane ~⅓ so light/dark gray separate |
| TI-83 Plus interrupt gray | Same 1/3 · 2/3 duty on a fast timer |
| HP-48 [GREYSCAL](https://www.hpcalc.org/hp48/docs/misc/) (Randy Ding) | Multi-frame GROBs with **2:1** (2 frames) or **4:2:1** (3 frames) weighting; design frames light→dark |
| HP Babal (this repo) | Spatial 50% checkerboard on the ball — flicker-free mid-tone |

Flipper cannot retarget LCD DMA like TI HW1, so the “plane switch” is a software blit of a pre-baked XBM each tick — same visual math, different plumbing (closer to TI HW2’s copy path).

## Demo image

Embedded 128×64 Flipper-shaped scene with all four levels, including a 4-band gradient on the “screen” and a small dolphin silhouette.

Static preview (true 2-bit expanded to 8-bit PNG — not FRC):

![2-bit Flipper demo](preview_flipper_2bit.png)

```bash
python3 tools/gen_flipper_image.py
```

## Controls

| Key | Action |
|-----|--------|
| Left / Right / OK | Cycle display mode |
| Up / Down | Toggle HUD |
| Back | Exit |

### Modes

| Mode | Kind | Notes |
|------|------|-------|
| **Stagger** | Temporal | Same 3/3·2/3·1/3·0/3 duties with `(x+y)%3` phase offset (**default**) |
| **TI planes** | Temporal | Dual plane + 2:1 schedule |
| **HP Bayer** | Spatial | 2×2 ordered dither — no flicker |
| **Thresh** | Static | Hard 1-bit clamp for comparison |

Backlight stays on while the app runs so temporal modes are visible.

### Device notes

On a real Flipper LCD (release SDK), **Stagger** looks better than TI planes / Bayer for this demo: less objectionable flicker while keeping the four gray levels readable. Stick with the classic **3-tick 2:1** duties (dark = 2/3). A trial **4-tick** schedule that lightened dark gray to 50% made flicker worse, so it was dropped.

## Build

```bash
cd grayscale_poc
ufbt update --channel=release
ufbt                 # → dist/grayscale_poc.fap
make -C test
ufbt launch          # USB Flipper required
```

## How TI planes map to gray

| Level | Dark plane | Light plane | Duty (dark×2 + light×1) |
|-------|------------|-------------|-------------------------|
| Black | ✓ | ✓ | 3/3 |
| Dark | ✓ | | 2/3 |
| Light | | ✓ | 1/3 |
| White | | | 0/3 |

Pure logic lives in `helpers/gray2bit.c` (no Flipper APIs) and is covered by host tests under `test/`.
