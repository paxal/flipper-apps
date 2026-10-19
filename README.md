# flipper-apps

Flipper Zero external apps (FAPs) and shared agent guidance for building them.

Games live under [`games/`](games/).

## Agent guidance

- Always-on: [`AGENTS.md`](AGENTS.md)
- Build/test skill: [`.cursor/skills/flipper-fap-build/SKILL.md`](.cursor/skills/flipper-fap-build/SKILL.md)
- Babal render-test skill: [`.cursor/skills/babal-render-tests/SKILL.md`](.cursor/skills/babal-render-tests/SKILL.md)

## Quick start (single app)

```bash
python3 -m pip install --upgrade ufbt
export PATH="$HOME/.local/bin:$PATH"

mkdir hello_flipper && cd hello_flipper
ufbt create APPID=hello_flipper
ufbt update --channel=release
ufbt                 # → dist/hello_flipper.fap
ufbt launch          # USB Flipper required
```

## Apps

### Falldown (`games/falldown`)

Port of the classic TI calculator **Falldown** arcade game.

Steer a ball left and right through gaps in ever-rising floors. Survive as long as you can — if a floor pushes you into the top of the screen, the run ends. Score increases for each floor you pass; the best score is saved on the SD card.

#### Controls

| Key | Action |
|-----|--------|
| Left / Right | Move the ball |
| OK | Start / retry |
| Back | Quit to title (from play or game over); exit from title |

#### Build

```bash
cd games/falldown
ufbt
ufbt launch   # USB Flipper required
```

The `.fap` is written under `games/falldown/dist/`.

#### Credit

Gameplay concept from the classic TI-83/84 Falldown / Fall Down calculator games (e.g. Jedraszczak, Rosenberg). This is an original Flipper remake, not a binary dump of calculator software.

### Babal (`games/babal`)

Port of the classic HP-48 arcade game **Babal** (public domain, U-Foot / Christian Mauduit).

Drive a ball along an 8-lane perspective track of platforms and holes. Accelerate, steer, and jump — falling rewinds you 64 platforms. Finish the level as fast as you can.

Plan / design notes: [`games/babal/PLAN.md`](games/babal/PLAN.md)

#### Controls

| Key | Action |
|-----|--------|
| Left / Right | Steer (title: change level) |
| Up / Down | Accelerate / brake (title: setup / about) |
| OK | Jump / start / retry; toggle invert in setup |
| Back | Title / exit |

Setup (from title → Up): invert display, bounce mode.

#### Build & test

```bash
cd games/babal
ufbt
ufbt launch              # USB Flipper required
make -C tests test       # host render suite T1–T4
```

The `.fap` is written under `games/babal/dist/`.

#### Credit

Original Babal 2.0 by Christian Mauduit (U-Foot) for HP-48 — public domain. Flipper remake with perspective math from U-Foot’s SVG Babal notes.
