# FAP CI workflow examples

## Build + lint (release + dev)

```yaml
name: "FAP: Build and lint"
on: [push, pull_request]
jobs:
  ufbt-build:
    runs-on: ubuntu-latest
    strategy:
      matrix:
        include:
          - name: release
            sdk-channel: release
          - name: dev
            sdk-channel: dev
    name: "ufbt: ${{ matrix.name }}"
    steps:
      - uses: actions/checkout@v4
      - name: Build
        uses: flipperdevices/flipperzero-ufbt-action@v0.1
        id: build-app
        with:
          sdk-channel: ${{ matrix.sdk-channel }}
          # app-dir: path/to/app   # multi-app repos
      - uses: actions/upload-artifact@v4
        with:
          name: ${{ github.event.repository.name }}-${{ steps.build-app.outputs.suffix }}
          path: ${{ steps.build-app.outputs.fap-artifacts }}
      - name: Lint
        uses: flipperdevices/flipperzero-ufbt-action@v0.1
        with:
          skip-setup: true
          task: lint
```

## Host tests then build

```yaml
name: "FAP: Test, lint, build"
on: [push, pull_request]
jobs:
  ufbt:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Setup ufbt
        uses: flipperdevices/flipperzero-ufbt-action@v0.1
        with:
          task: setup
          sdk-channel: release
      - name: Host unit tests
        run: make -C test
      - name: Lint
        run: ufbt lint
      - name: Build
        id: build-app
        uses: flipperdevices/flipperzero-ufbt-action@v0.1
        with:
          skip-setup: true
          sdk-channel: release
      - uses: actions/upload-artifact@v4
        with:
          name: ${{ github.event.repository.name }}-${{ steps.build-app.outputs.suffix }}
          path: ${{ steps.build-app.outputs.fap-artifacts }}
```

## Action inputs worth remembering

| Input | Notes |
| --- | --- |
| `task` | `build` (default), `lint`, `setup` |
| `app-dir` | Path containing `application.fam` |
| `sdk-channel` | `release`, `rc`, `dev` |
| `sdk-branch` / `sdk-url` / `sdk-index-url` | Alternate SDK sources |
| `skip-setup` | Reuse SDK from an earlier step |
| `ufbt-version` | `latest`, `prerelease`, or PyPI pin |

Useful outputs from `build`: `fap-artifacts`, `suffix`, `api-version`, `fap-dir`.
