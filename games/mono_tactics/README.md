# Mono Tactics (Flipper Zero)

Original **tactical RPG prototype** for Flipper Zero: grid battles, turn-based combat, reduced 1-bit graphics, **no sound**.

Inspired by classic strategy-RPG *mechanics* only — **not** a port of Shining Force 2 or any copyrighted game.

**Status:** planning. See the feasibility and phased roadmap in [`PLAN.md`](PLAN.md).

## Build (once FS0 exists)

```bash
cd games/mono_tactics
ufbt
ufbt launch   # USB Flipper required
```

Host tests (from FS1+):

```bash
make -C tests test
```

## Credit

Original design for this repo. Genre inspiration: 16-bit tactical RPGs. No third-party ROM, assets, or scripts.
