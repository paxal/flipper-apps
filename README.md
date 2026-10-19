# flipper-apps

Flipper Zero external apps (FAPs) and shared agent guidance for building them.

## Agent guidance

- Always-on: [`AGENTS.md`](AGENTS.md)
- Build/test skill: [`.cursor/skills/flipper-fap-build/SKILL.md`](.cursor/skills/flipper-fap-build/SKILL.md)

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
