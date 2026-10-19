---
name: babal-render-tests
description: >-
  Author, run, and update Babal Flipper host rendering tests (perspective math,
  depth LUT, soft 128x64 golden frames, invariants). Use when implementing
  babal/render_math, babal/tests, recording goldens, or debugging render
  regressions for the Babal HP-48 port.
---

# Babal render tests

Follow this skill whenever you touch Babal perspective/LUT/span-fill code or the host test suite. Plan context: [`babal/PLAN.md`](../../../babal/PLAN.md) (sections *Rendering — U-Foot’s perspective algorithm* and *Rendering tests*).

## When to use

- Implementing Phase 0+ `render_math.c` / `render.c`
- Adding or changing T1–T4 cases
- Recording or reviewing golden frames
- A render regression (“looks wrong”, LUT bounds, hole pixels painting)

## Layout (required)

```
babal/
  render_math.h
  render_math.c          # pure C — no Flipper GUI / furi includes
  render.c               # thin Canvas adapter only
  tests/
    Makefile             # `make test` on host (gcc/clang)
    test_perspective.c   # T1
    test_lut.c           # T2
    test_span_fill.c     # T3 (+ soft buffer)
    test_invariants.c    # T4
    goldens/             # 128×64 PBM (or agreed 1-bit format)
    fixtures/            # tiny ASCII 8-lane strips
```

**Invariant:** anything under test for T1–T3 must compile and link **without** the Flipper SDK.

## Commands

```sh
cd babal/tests
make test                 # compare goldens; must be green on CI
RECORD_GOLDEN=1 make test # rewrite goldens after intentional visual change
```

Never commit new goldens without a one-line note in the PR: which fixture changed and why (anchor retune, bugfix, new case).

## Test IDs (do not skip)

### T1 — Perspective math

Assert against SVG Babal formulas (`y = c - a/(b+yLevel)`, Thales X):

| Case | Tolerance |
|------|-----------|
| `yLevel → large` → `yMax` | ±1 px |
| `yLevel = LEVEL_Y_BALL` → `yBall` | ±1 px |
| `yLevel = 0` → `y0` | ±1 px |
| Monotonic depth → screen Y | non-decreasing toward horizon |
| `X(+x) == -X(-x)` at same Y | exact in fixed-point or ±1 |
| Q16 vs double oracle on a sample grid | ±1 px |

### T2 — Depth LUT (`OKTAB` analogue)

| Case | Assert |
|------|--------|
| `lut_build(progress)` | `row_y[]` ordered; no collapsed duplicate rows |
| Scroll +1 cell | near recycles, far inserts; edges consistent |
| Hot path | `lut_span_for_cell` only reads LUT + add/shift (no `a/(b+y)` recompute) |
| Bounds | edges in `[0, 127]`, left ≤ right |

### T3 — Golden frames (soft 128×64)

Render to a host 1-bit buffer, not device `Canvas`.

Minimum goldens:

- `solid_p0.pbm` — full 8×8 platforms, ball centered, progress 0
- hole/checker fixture — holes clear, platforms set
- ball-only — sprite near `yBall` band
- `solid_p05.pbm` / `solid_p1.pbm` — scroll stability

`make test` compares; `RECORD_GOLDEN=1` writes. Visually spot-check goldens when anchors change.

### T4 — Invariants / fuzz

- Drawn cells ≤ `8 * 8`
- Hole cells never set platform bits
- Jump changes ball Y only; track LUT bytes unchanged
- Fixed-seed random 8-lane strips: no crash, spans in-bounds

### T5 — Manual visual (Phase 0 gate, not CI)

After host green: screenshot Flipper (`ufbt launch`) or emulator frame; compare vanishing track / ball plane to HP-48 or SVG Babal. Attach to the PR. Does **not** replace T1–T4.

## Out of scope (v1)

- Flashing firmware `unit_tests` app
- OpenGL / Borsboom pixel parity
- Cycle-accurate HP-48 `AFFICH` match

## Agent checklist

1. Read this skill before editing render or `babal/tests`.
2. Keep math in `render_math.*` (host-linkable).
3. Add/adjust T1–T4 for the behavior you change.
4. Run `make test`; only use `RECORD_GOLDEN=1` when the visual change is intentional.
5. For Phase 0 completion: host green **and** T5 screenshot in the PR.
6. Update [`babal/PLAN.md`](../../../babal/PLAN.md) only if the test *contract* changes (new ID, format, or command).
