---
name: flipper-fap-build
description: Build, lint, host-test, and CI Flipper Zero FAP apps with uFBT. Use when creating or changing Flipper apps, application.fam, ufbt workflows, or device launch/debug.
---

# Flipper Zero FAP — build & test

## When to use

- Scaffolding or editing a Flipper external app (`.fap`)
- Choosing SDK channel / fixing API mismatch
- Setting up host unit tests or GitHub Actions for FAPs
- Launching on device, reading logs, or flashing matching firmware

## Tooling choice

| Tool | Use when |
| --- | --- |
| **uFBT** (`pip install ufbt`) | Standalone apps in this repo (default) |
| **FBT** in `flipperzero-firmware` | App must ship inside firmware / needs full tree |

uFBT downloads a prebuilt SDK + toolchain; you do not clone firmware to compile a FAP.

Validated locally (2026-03): `ufbt` 0.2.6, release SDK **1.5.1**, target **f7**, API **88.2**.

## Prerequisites

- Python ≥ 3.8
- `python3 -m pip install --upgrade ufbt`
- Put user scripts on `PATH` if needed: `export PATH="$HOME/.local/bin:$PATH"`
- First real build downloads SDK + toolchain into `~/.ufbt` (override with `UFBT_HOME` or per-project `ufbt dotenv_create`)

## Create an app

```bash
mkdir my_app && cd my_app
ufbt create APPID=my_app          # APPID: [a-z0-9_]+ only
ufbt vscode_dist                  # optional IDE wiring
```

Typical tree:

```text
my_app/
├── application.fam      # App(... FlipperAppType.EXTERNAL ...)
├── my_app.c             # entry_point e.g. my_app_app
├── my_app.png           # 10×10 1-bit FAP icon
├── images/              # fap_icon_assets → generates <appid>_icons.h
├── test/                # optional host tests (recommended)
└── .github/workflows/   # ufbt create copies a template build.yml
```

Minimal manifest fields: `appid`, `apptype=FlipperAppType.EXTERNAL`, `entry_point`, `fap_icon`, usually `fap_category`, `stack_size`, `fap_icon_assets`.

## Build

From the directory that contains `application.fam`:

```bash
ufbt update --channel=release   # pin channel before CI/device work
ufbt                            # writes dist/<appid>.fap (+ dist/debug/*.elf)
```

SDK sources:

```bash
ufbt update --channel=release|rc|dev
ufbt update --branch=0.81.1
ufbt update --url=<sdk.zip-or-tgz-url>
ufbt update --local=<path>
ufbt update --index-url=<directory.json> --channel=dev   # 3rd-party firmwares
```

**API rule:** device firmware API must be ≥ the FAP’s required API. Mismatch → flash matching firmware with `ufbt flash_usb` (or SWD `ufbt flash`), or rebuild against an older SDK channel/branch.

## Lint / format

```bash
ufbt lint      # clang-format check (CI-friendly)
ufbt format    # rewrite sources
```

## Test strategy

Flipper has no standard on-device unit-test runner for FAPs. Split code:

1. **Host-testable pure C** — parsers, state machines, crypto, classifiers: no Flipper headers.
2. **Device glue** — GUI, `furi_record_*`, radio HAL, storage: exercise with `ufbt launch` + `ufbt cli` / `log`.

Recommended layout:

```text
app/
├── helpers/foo.c     # pure logic, shared by FAP + tests
├── app.c             # Flipper entry / scenes
└── test/
    ├── Makefile
    └── test_foo.c
```

```bash
make -C test          # or: make -C test check / make test-host
```

Host tests need only `gcc`/`clang`. Keep them out of `application.fam` `sources` (exclude with `!test` or keep tests outside gathered globs).

On-device checks (when a Flipper is attached):

```bash
ufbt launch           # build, upload, start
ufbt cli              # interactive CLI; then: log  or  log info
```

`furi_check` / `furi_assert` are **runtime crash helpers**, not a host test framework.

## CI (GitHub Actions)

Prefer official action [`flipperdevices/flipperzero-ufbt-action@v0.1`](https://github.com/flipperdevices/flipperzero-ufbt-action):

- `task: build` (default) → `.fap` + outputs `fap-artifacts`, `suffix`, `api-version`
- `task: lint`
- `task: setup` then custom steps (`make -C test`, etc.) with `skip-setup: true` on later steps

See [references/ci-workflows.md](references/ci-workflows.md) for copy-paste workflows.

For multi-app repos, set `app-dir` to each app path (matrix over directories).

## Device / debug cheat sheet

| Goal | Command |
| --- | --- |
| Build + run on USB Flipper | `ufbt launch` |
| CLI session | `ufbt cli` |
| Flash SDK-matching firmware (USB) | `ufbt flash_usb` |
| Flash via SWD probe | `ufbt flash` |
| VS Code configs | `ufbt vscode_dist` |
| Reset broken uFBT state | `ufbt clean` (or remove `~/.ufbt`) |
| Toolchain shell | `eval "$(ufbt -s env)"` |

Multiple devices: `FLIP_PORT=...`.

## Cloud / headless agents

Assume **no Flipper hardware**:

1. Install `ufbt`, `ufbt update --channel=release`
2. Run host tests if present
3. `ufbt lint` + `ufbt` and confirm `dist/*.fap`
4. Do not block on `ufbt launch` / `flash_*`

## Common failures

| Symptom | Fix |
| --- | --- |
| `ufbt: command not found` | Add `$HOME/.local/bin` to `PATH` |
| Missing `application.fam` | `cd` to app root or `ufbt create` |
| SDK / API mismatch on device | Align channel or `ufbt flash_usb` |
| Lint fails in CI | `ufbt format` locally and commit |
| Icon / assets errors | 10×10 1-bit PNG for `fap_icon`; put assets under `fap_icon_assets` |

## Upstream docs

- uFBT: https://github.com/flipperdevices/flipperzero-ufbt
- First app (C): https://developer.flipper.net/flipperzero/doxygen/app_dev_your_first_app_in_c.html
- App manifests: https://github.com/flipperdevices/flipperzero-firmware/blob/dev/documentation/AppManifests.md
- FAPs on SD: https://github.com/flipperdevices/flipperzero-firmware/blob/dev/documentation/AppsOnSDCard.md
- ufbt GitHub Action: https://github.com/flipperdevices/flipperzero-ufbt-action
