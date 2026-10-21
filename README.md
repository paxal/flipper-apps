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

### Grayscale POC (`grayscale_poc`)

2-bit grayscale proof-of-concept on the 1-bit LCD via temporal FRC (frame-rate control). Ships with a packed Flipper-themed demo image.

See [`grayscale_poc/README.md`](grayscale_poc/README.md).

```bash
cd grayscale_poc
ufbt
make -C test
```

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

App readme (controls, build, **HP render findings**): [`games/babal/README.md`](games/babal/README.md) · plan: [`games/babal/PLAN.md`](games/babal/PLAN.md)

### Mono Tactics (`games/mono_tactics`)

**Planning only.** Original Flipper tactical RPG prototype (grid battles, reduced 1-bit graphics, no sound). Genre-inspired mechanics — not a Shining Force 2 port.

Feasibility + independent feature-set roadmap: [`games/mono_tactics/PLAN.md`](games/mono_tactics/PLAN.md)

### NFC Write Debug (`nfc_write_debug`)

Emulate a `.nfc` from the SD card and log reader memory writes to a timestamped file under `apps_data/nfc_write_debug/logs/`. Supports MIFARE Classic (blocks) and Ultralight/NTAG (pages).

See [`nfc_write_debug/README.md`](nfc_write_debug/README.md).

```bash
cd nfc_write_debug
ufbt
make -C test
```

### CSC Debug (`csc_debug`)

BLE central debugger for Cycling Speed and Cadence sensors (GATT service `0x1816`).

Scan nearby advertisers, connect to a sensor, subscribe to CSC Measurement, and show live speed, cadence, rev counters, and raw hex.

**Requires Moon Firmware (BLE Full stack)** — stock Flipper is peripheral-only and cannot scan or connect to CSC sensors. See [`csc_debug/README.md`](csc_debug/README.md) for details and build steps.

#### Build (Moon Firmware)

```bash
# From a Moon Firmware tree with this app linked into applications_user/
./fbt COMPACT=1 DEBUG=0 fap_csc_debug
```

Host-side parser tests (no Flipper SDK):

```bash
make test
```
