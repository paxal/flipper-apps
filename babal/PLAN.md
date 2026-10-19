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

### Rendering approach

Classic TrailBlazer / Babal look:

1. Track extends “into” the screen (near = bottom, far = horizon).
2. Draw consecutive platform segments as trapezoids / filled strips.
3. Holes = gaps (no fill / background).
4. Ball drawn as a filled circle or 3×3–5×5 square near the bottom third.
5. Keep contrast high; optional invert later.

Start with a **simple 2.5D strip renderer** (lanes × depth rows), not a full 3D engine.

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
| [hpcalc Babal 2.0](https://www.hpcalc.org/details/880) | Binary + community levels |
| [Babal 2.0 source](https://www.hpcalc.org/details/3038) *(hpcalc “Source code” listing)* | Assembly / level insights if available |
| Emulator play (Emu48 etc.) | Feel, timing, look — requires legal ROM |
| TrailBlazer footage | Perspective / lane design reference |
| Author’s modern [Babal](https://ufoot.org/software/babal/en/) | Design inspiration only; different game |

**License note:** Original is public domain. Credit U-Foot / Christian Mauduit in About screen and README. Do not copy proprietary ROM or emulator ROMs into the repo.

---

## 6. Implementation phases

### Phase 0 — Spike (prove the look)

- Empty FAP skeleton with menu + blank game view.
- Hard-coded short track strip drawn in perspective.
- Ball sprite that moves left/right on the near lane.

**Done when:** Screenshot looks recognizably “Babal-like” on device/emulator.

### Phase 1 — Playable loop

- Forward scroll driven by `vz`.
- Jump + gravity.
- Holes + rewind penalty.
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
| Perspective renderer is hard on 64px height | Prototype strip renderer early (Phase 0); simplify lanes |
| Physics feel wrong vs original | Play original / watch footage; expose `#define` tunables |
| Original `.bab` format opaque | Author new packed level format; treat community levels as design refs |
| Frame budget on Flipper | Cap draw depth; precompute trapezoid tables; target 20–25 FPS |
| Scope creep toward Godot remake | Keep v1 = finite level + five controls + hole penalty |

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

**Recommendation defaults:** raw ViewPort for v1; C arrays for one level; HP-faithful look; app id `babal`.
