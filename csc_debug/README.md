# CSC Debug

Flipper Zero BLE **central** tool for debugging [Cycling Speed and Cadence](https://www.bluetooth.com/specifications/specs/cycling-speed-and-cadence-service-1-0/) (CSC) sensors (GATT service `0x1816`).

## Why custom firmware?

Stock Flipper firmware loads ST’s **BLE Light** radio stack (`stm32wb5x_BLE_Stack_light_fw.bin`). That stack is **peripheral / broadcaster only**:

- no observer role → cannot scan
- no central role → cannot connect outbound
- no GATT client → cannot read or subscribe to sensor characteristics

Official confirmation:

- Flipper team ([issue #2906](https://github.com/flipperdevices/flipperzero-firmware/issues/2906)): full stack needs more RAM/flash than OFW budgets for the default image
- Central/GATT client support is still an **open** OFW PR ([#4436](https://github.com/flipperdevices/flipperzero-firmware/pull/4436)) and only works when the Full stack is installed
- Momentum / Unleashed / RogueMaster BLE tools (spam, FindMy, Bad KB) advertise as a **peripheral**; they do not add central GATT client to apps

This app therefore targets **[Moon Firmware](https://github.com/KaraZajac/Moon-Firmware)**, which ships the Full BLE stack and exports scan / connect / GATT client APIs to FAPs (`furi_hal_bt_start_scanning`, `furi_hal_bt_connect`, `ble_gatt_client_*`). [Blue++](https://dixon.cx/flipper-bluepp) is another Full-stack option with a different API (`libble`).

Most CSC sensors expose Measurement notifications without bonding; the worker uses `gap_set_no_pairing()`.

## Features

- Scan nearby BLE advertisers; mark devices that advertise CSC UUID `0x1816` with `*`
- Connect, discover CSC service, subscribe to **CSC Measurement** (`0x2A5B`)
- Live **speed** (km/h) and **cadence** (rpm) from wheel/crank deltas
- Raw notification hex, optional Feature / Sensor Location reads
- Configurable wheel circumference (1900–2300 mm) for speed

## Build (Moon Firmware)

```sh
git clone https://github.com/KaraZajac/Moon-Firmware.git
cd Moon-Firmware
# symlink or copy this app
ln -s /path/to/flipper-apps/csc_debug applications_user/csc_debug
./fbt COMPACT=1 DEBUG=0 fap_csc_debug
```

The `.fap` lands under `dist/`. Copy it to the Flipper SD card under `apps/Bluetooth/` on a device running Moon (Full BLE stack).

With uFBT, point the SDK at a Moon-built SDK directory (produced by the firmware build), then run `ufbt` inside `csc_debug/`.

## Host unit tests (parser only)

```sh
cc -I csc_debug/helpers -o /tmp/test_csc_parse \
  tests/test_csc_parse.c csc_debug/helpers/csc_parse.c -lm
/tmp/test_csc_parse
```

## CSC Measurement layout

| Field | Size | Notes |
|-------|------|--------|
| Flags | 1 | bit0 wheel, bit1 crank |
| Cumulative Wheel Revolutions | 4 | if wheel present |
| Last Wheel Event Time | 2 | 1/1024 s |
| Cumulative Crank Revolutions | 2 | if crank present |
| Last Crank Event Time | 2 | 1/1024 s |
