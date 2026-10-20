# Grayscale POC

Proof-of-concept **2-bit grayscale** (4 levels) on the Flipper Zero’s native 1-bit LCD.

The panel can only drive each pixel on or off. This app stores a packed 2-bit framebuffer and uses **frame-rate control (FRC)** / temporal dithering: mid-grays blink on for 1/3 or 2/3 of frames so persistence of vision reads them as gray.

## Demo image

Embedded 128×64 Flipper-shaped scene with all four levels (black / dark / light / white), including a 4-band gradient on the “screen” and a small dolphin silhouette.

Static preview (true 2-bit expanded to 8-bit PNG — not FRC):

![2-bit Flipper demo](preview_flipper_2bit.png)

Regenerate the C array + preview:

```bash
python3 tools/gen_flipper_image.py
```

## Controls

| Key | Action |
|-----|--------|
| OK | Toggle FRC grayscale ↔ hard threshold (1-bit) |
| Up / Down | Toggle HUD hint |
| Back | Exit |

Backlight is held on while the app runs so the FRC flicker is visible.

## Build

```bash
cd grayscale_poc
ufbt update --channel=release
ufbt                 # → dist/grayscale_poc.fap
make -C test         # host unit tests
ufbt launch          # USB Flipper required
```

## How it works

| Level | Duty (ink / 3 frames) | Appearance |
|-------|----------------------|------------|
| 0 | 3/3 | Black |
| 1 | 2/3 | Dark gray |
| 2 | 1/3 | Light gray |
| 3 | 0/3 | White |

Per-pixel spatial phase `(x + y) % 3` staggers which sub-frame is inked so large flat regions flicker less in sync.

Pure logic lives in `helpers/gray2bit.c` (no Flipper APIs) and is covered by host tests under `test/`.
