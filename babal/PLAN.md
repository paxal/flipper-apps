# Babal — Flipper Zero Port Plan

Port of **Babal 2.0** (HP-48 / HP-49 arcade game by Christian Mauduit / U-Foot) to Flipper Zero as a FAP under `paxal/flipper-apps`.

| | |
|---|---|
| **Original** | Public domain (author clarified) · [ufoot.org](https://ufoot.org/software/babal/hp48/en/) · [hpcalc.org](https://www.hpcalc.org/details/880) |
| **Inspiration** | TrailBlazer (Amstrad CPC) |
| **Target** | Flipper Zero · 128×64 monochrome · C FAP via uFBT |
| **Scope** | Faithful HP-era feel, not the later Godot/Android remake |

---

## 1. What Babal is

Arcade runner on a scrolling track of rectangular **platforms** and **holes**. You drive a chunky pixel ball and finish the level as fast as possible.

### Core rules (from the HP manual)

- Level = sequence of rectangular platforms and holes.
- **5 controls:** left, right, accelerate, decelerate, jump.
- Falling in a hole **rewinds ~64 platforms** (time penalty).
- Win condition: reach the end; score = best time / progress.
- Optional: bounce mode, invert display, key remap, key sensitivity.

### Why Flipper is a natural fit

| HP-48 | Flipper Zero |
|-------|----------------|
| ~131×64 mono LCD | 128×64 mono LCD |
| Arrow keys + Enter | D-pad + OK |
| ON to quit | Back |
| Tiny RAM / assembly | Enough RAM; C + optional game engine |

Visual language (chunky ball, perspective track, black/white) maps almost 1:1.

---

## 2. Product decisions

### In scope (v1)

1. One playable original-style level (or a short original recreation).
2. Ball physics: lateral move, speed up/down, jump, gravity, fall detection.
3. Pseudo-3D / TrailBlazer-style track render on 128×64.
4. Hole penalty (rewind platforms) + finish screen with time.
5. Menu: Play / High score / About (Back exits).
6. Fixed default controls (no remap in v1).

### Explicitly out of scope (v1)

- Full reverse-engineering / binary-compatible `.bab` loader (nice-to-have later).
- Godot remake features (endless tunnel, boost pads, music).
- Video Game Module gyro input.
- Multiplayer / online scores.

### Later (v2+)

- Multiple levels (community `.bab`-inspired packs).
- Setup: invert, bounce, sensitivity.
- Persist high scores to storage.
- Optional Flipper Game Engine refactor if entity model helps.

---

## 3. Controls (proposed)

| Action | Flipper input |
|--------|----------------|
| Left / Right | Left / Right |
| Accelerate | Up |
| Decelerate | Down |
| Jump | OK |
| Quit / menu | Back |

Hold-to-repeat for left/right/accel matches the HP “sensitivity” idea without a settings UI in v1.

---

## 4. Technical architecture

### Stack

- **Language:** C (Flipper FAP).
- **Build:** [uFBT](https://github.com/flipperdevices/flipperzero-ufbt) (`application.fam`, `babal.c`, icons if needed).
- **GUI:** `ViewPort` + `Canvas` draw loop, or [flipperzero-game-engine](https://github.com/flipperdevices/flipperzero-game-engine) if entity/level helpers pay off.
- **Loop:** `FuriTimer` tick (~20–30 FPS) + input queue → update → `view_port_update`.

### Suggested layout

```
babal/
  PLAN.md              ← this document
  application.fam
  babal.c              ← app entry, menus, loop
  game.h / game.c      ← state machine, physics
  level.h / level.c    ← track data + collision
  render.h / render.c  ← perspective draw
  levels/
    tutorial.h         ← packed platform strip
  assets/              ← optional icons / splash
```

### Data model (sketch)

```c
typedef struct {
    int16_t x;      // lateral position on track
    int16_t y;      // height (jump)
    int16_t z;      // progress along track
    int16_t vx, vz; // lateral + forward speed
    int16_t vy;     // vertical velocity
    bool on_ground;
} Ball;

typedef enum { PLATFORM, HOLE } CellKind;

typedef struct {
    CellKind* cells;  // ring or strip along z
    uint16_t length;
    uint8_t width;    // lanes (e.g. 8–16)
} Level;
```

### Rendering — U-Foot’s perspective algorithm (planned baseline)

Do **not** invent a random 3D stack. Use the math U-Foot documented when he re-explained Babal in [SVG Babal (2006)](https://ufoot.org/software/babal/svg/en/), and the same optimization ideas visible in the HP-48 `AFFICH` assembly ([bb20dev.zip](https://www.hpcalc.org/details/869)).

#### Geometric model (from SVG Babal comments)

Depth → screen **Y** is a hyperbolic projection (not linear):

```text
yViewport = c − a / (b + yLevel)
```

Coefficients are fixed from three anchors on the HP-sized viewport:

| Anchor | Meaning | SVG default (131×64 scaled) |
|--------|---------|-----------------------------|
| `yMax` | Horizon (far / infinite) | top of viewport |
| `yBall` | Plane where the ball sits | ~1/4 up from bottom |
| `y0` | Nearest visible ground | bottom |

Lateral → screen **X** uses Thales (linear in X for a fixed screen Y):

```text
xViewport = (yMax − yViewport) / (yMax − y0) · x0 · xLevel
          = (d − yViewport) · e · xLevel
```

Each platform cell (“pavé”) is a **trapezoid**: project the four corners of cell `(lane, depth)`, then fill. Holes = skip draw.

Key SVG constants to port (adapt to 128×64):

| Constant | Value | Role |
|----------|-------|------|
| `LEVEL_WIDTH` | 8 | Lanes across the track |
| `MAX_NB_Y_DISPLAY` | 8 | Only this many depth rows drawn ahead |
| `REWIND_Y` | 64 | Hole penalty (platforms) |
| `LEVEL_Y_BALL` | ~1/6 | Ball’s depth plane in level space |

#### Why the HP-48 version was “fast”

From strings/comments in `SBABALGAME` / `AFFICH` (assembly inside `BABALDEV.DIR`):

1. **`OKTAB` / `ACTAB` / “NOUVEAU TAB”** — rebuild a small **lookup table** when the view changes (player progress / scroll), not per pixel.
2. **Per scanline / per depth row:** store **start offset** + **offset increment** into the screen GROB (`OFFSET`, `INCREMENT D'OFFSET`), then walk the framebuffer with adds — not divides.
3. **Inner loop is span fill:** for each cell, test a “wall” bit; if solid, read left edge (`POS TRAIT`) and right limit (`PT LIM DROIT`), compute width (`LARGEUR TRAIT`), paint a horizontal run.
4. **Nested cheap loops:** depth rows × lane width (comments: `LIGNES*4`, `LIGNES*16`, `LARGEUR*8`) — fixed small bounds, same idea as `MAX_NB_Y_DISPLAY × LEVEL_WIDTH`.
5. **Double buffer:** work buffer vs visible screen (`ECRAN VU HAUT` / `ECRAN TRAVAIL BAS` + swap).
6. **Ball = sprite blit** (`BOULE` + prebuilt GROBs), with position using a baked `/16` (`DIVISION PAR 16 INTEGREE`) — not a second perspective solver.

So the “fast algorithm” is: **closed-form perspective → precomputed tables → horizontal span fill → sprite ball**, with only ~8 depth rows visible.

#### Flipper implementation plan (`render.c`)

| Step | What | Notes |
|------|------|--------|
| R1 | Port `levelToViewportY` / `levelToViewportX` with fixed-point (Q8 or Q16) | Mirror SVG formulas; retune anchors for 128×64 |
| R2 | At scroll change (or once per frame if cheap): build LUT `row_y[8]`, `row_half_width[8]` or left/right edges for each depth | HP `OKTAB` analogue — **avoid div in inner loops** |
| R3 | For each of 8 depth × 8 lanes: if platform, fill trapezoid as horizontal spans between projected edges | Prefer `canvas_draw_line` / dot runs; no OpenGL |
| R4 | Draw ball as fixed sprite near `yBall` plane; jump offsets Y only | Match chunky HP look |
| R5 | Optional: invert colors for “inv” mode later | Same buffers |

**Coordinate convention (match SVG):** level `y` = forward progress along the track; level `x` = lateral lane (centered); ball jump is a separate visual offset (`z` in SVG). Align naming in C (`progress`, `lane`, `height`) to avoid HP vs SVG axis confusion.

**Non-goals for render:** full 3D meshes, OpenGL (Borsboom’s Java remake), Godot camera, per-pixel divides every frame.

### Physics (v1 targets)

- Constant gravity; jump impulse while on ground.
- Forward speed clamped `[vz_min, vz_max]`; Up/Down adjust.
- Lateral speed with soft clamp to track edges; falling off edge = hole.
- Collision: sample cell under ball at `(lane, z)`; if `HOLE` and `y≈0` → penalty.
- Penalty: `z = max(0, z - 64)`; brief invulnerability / flash.

Tune numbers against gameplay feel (HP G/GX was “twice as fast” as S/SX — pick a Flipper speed that feels crisp, not frantic).

---

## 5. Reference materials

| Resource | Use |
|----------|-----|
| [User manual](https://ufoot.org/babal/hp48/manual) | Rules, menus, setup options |
| [SVG Babal source](https://ufoot.org/software/babal/svg/en/) (`svgbabal.svg`) | **Authoritative readable perspective math** (`y=c−a/(b+y)`, Thales X, pavé trapezoids) |
| [Babal 2.0 source `bb20dev.zip`](https://www.hpcalc.org/details/869) | HP-48 asm: `AFFICH`, `OKTAB`, span-fill + LUT speed tricks |
| [hpcalc Babal 2.0](https://www.hpcalc.org/details/880) | Binary + community `.bab` levels |
| [borsboom/babal](https://github.com/borsboom/babal) | OpenGL remake + ASCII map data — **feel/levels only**, not the fast path |
| Emulator play (Emu48 etc.) | Feel, timing, look — requires legal ROM |
| TrailBlazer footage | Perspective / lane design reference |
| Author’s modern [Babal](https://ufoot.org/software/babal/en/) | Different game (endless / Godot); ignore for v1 render |

**License note:** Original is public domain. Credit U-Foot / Christian Mauduit in About screen and README. Do not copy proprietary ROM or emulator ROMs into the repo. SVG Babal is GPL — use it as a **math/algorithm reference**, reimplement in C for the FAP (do not copy the SVG/JS verbatim into a GPL-incompatible tree without deciding license).

---

## 6. Implementation phases

### Phase 0 — Spike (prove the look + perspective)

- Empty FAP skeleton with menu + blank game view.
- Port SVG formulas to fixed-point; draw a hard-coded 8×N strip of pavés.
- Build the first **depth LUT** (row Y + half-widths); fill trapezoids as spans.
- Ball sprite that moves left/right on the `yBall` plane.

**Done when:** Screenshot matches Babal’s vanishing-track look; no per-pixel divides in the hot path.

### Phase 1 — Playable loop

- Forward scroll driven by `vz`; rebuild / advance LUT as progress changes.
- Jump + gravity.
- Holes + rewind penalty (`−64`).
- Finish line + elapsed time.

**Done when:** Can complete a short level without soft-locking.

### Phase 2 — Polish

- One full-length level (density of holes/jumps like original).
- HUD: time, speed bar (optional).
- High score in RAM (or storage).
- About / credit screen.

**Done when:** Fun for a 1–2 minute run; shippable FAP.

### Phase 3 — Content & options

- 3–5 levels (easy → hard).
- Invert + bounce toggles.
- Optional `.bab`-inspired level format (simple custom text/bin, not necessarily original binary).

---

## 7. Risks & mitigations

| Risk | Mitigation |
|------|------------|
| Perspective wrong / “not Babal” | Copy U-Foot SVG anchors first; only then retune for 128×64 |
| Divisions kill frame time | HP lesson: LUT (`OKTAB`) + span fill; max 8×8 cells/frame |
| Physics feel wrong vs original | Play original / watch footage; expose `#define` tunables |
| Original `.bab` format opaque | Author new packed level format; treat community levels as design refs |
| Frame budget on Flipper | Cap draw depth; precompute edge tables; target 20–25 FPS |
| Scope creep toward Godot remake | Keep v1 = finite level + five controls + hole penalty |
| GPL SVG vs public-domain game | Reimplement math in C; don’t vendor `svgbabal.svg` as source |

---

## 8. Acceptance criteria (v1)

- [ ] Installs and launches as FAP on current official firmware SDK.
- [ ] Playable start → finish on one level with all five controls.
- [ ] Falling in a hole applies a clear progress penalty.
- [ ] Run time shown at finish; best time retained for session.
- [ ] About credits original author and public-domain status.
- [ ] Back always returns to menu / exits cleanly (no hang).

---

## 9. Open questions (resolve before coding Phase 1)

1. **Engine:** raw `ViewPort` vs Flipper Game Engine submodule?
2. **Levels:** hand-authored C arrays vs external asset files on SD?
3. **Fidelity:** pixel-faithful HP look vs slightly cleaner Flipper UI chrome?
4. **Name:** `Babal` vs `BabalFZ` app id to avoid catalog clashes?

**Recommendation defaults:** raw ViewPort for v1; C arrays for one level; HP-faithful look; app id `babal`; perspective = U-Foot SVG math + HP-style LUT/span fill.
