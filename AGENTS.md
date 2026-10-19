# flipper-apps — agent notes

Repo for Flipper Zero external apps (FAPs). Prefer **uFBT** over full-firmware FBT unless an app must live inside `flipperzero-firmware`.

## Layout (target)

- One app per directory (or repo root for a single-app project)
- Each app has `application.fam` + C sources; optional `images/`, `test/`
- Built artifact: `dist/<appid>.fap`

## Always-on rules

1. Match the Flipper firmware/API the user targets (`ufbt update --channel=release|rc|dev`). Default to **release**.
2. Put testable logic in pure C (no `furi_*` / hardware calls) and cover it with **host** unit tests under `test/`.
3. Device launch (`ufbt launch`) needs a USB-connected Flipper with matching SDK; cloud agents usually cannot do this — build + host tests + lint are enough offline.
4. Do not commit `.ufbt/`, `dist/`, or local toolchain state unless the project deliberately pins via `ufbt dotenv_create`.

## Quick commands

```bash
python3 -m pip install --upgrade ufbt   # ensure ~/.local/bin on PATH
ufbt update --channel=release
ufbt                                    # → dist/*.fap
ufbt lint
ufbt format
make -C test                            # host tests, if present
ufbt launch                             # device: build + upload + run
ufbt cli                                # then: log / log info
```

## Deeper procedure

For create/build/CI/debug details, use the project skill:

`.cursor/skills/flipper-fap-build/SKILL.md`
