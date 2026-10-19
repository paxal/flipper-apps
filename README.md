# flipper-apps

Collection of Flipper Zero apps.

## Falldown

Port of the classic TI calculator **Falldown** arcade game.

Steer a ball left and right through gaps in ever-rising floors. Survive as long as you can — if a floor pushes you into the top of the screen, the run ends. Score increases for each floor you pass; the best score is saved on the SD card.

### Controls

| Key | Action |
|-----|--------|
| Left / Right | Move the ball |
| OK | Start / retry |
| Back | Quit to title (from play or game over); exit from title |

### Build

Requires [uFBT](https://github.com/flipperdevices/flipperzero-ufbt):

```bash
pip install --upgrade ufbt
cd falldown
ufbt
```

Flash and launch on a connected Flipper:

```bash
ufbt launch
```

The `.fap` is written under `falldown/dist/`.

### Credit

Gameplay concept from the classic TI-83/84 Falldown / Fall Down calculator games (e.g. Jedraszczak, Rosenberg). This is an original Flipper remake, not a binary dump of calculator software.
