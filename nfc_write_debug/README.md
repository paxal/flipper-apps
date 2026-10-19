# NFC Write Debug

Emulate a saved `.nfc` card and log every memory write the reader performs against the
emulated tag. Logs are timestamped by the session start time for later debugging.

## Supported cards

Full write logging for:

- **MIFARE Classic** (block-level)
- **MIFARE Ultralight / NTAG** (page-level)

Other protocols are rejected at load time (UID-only emulate cannot accept memory writes).

## Usage

1. Put the `.nfc` under `/ext/nfc/` (same place as the stock NFC app).
2. Open **NFC Write Debug** from the Apps menu.
3. Pick the card → Flipper starts emulating.
4. Present it to the reader. Each detected write flashes the LED and is appended to the log.
5. Press **Back** to stop. A summary dialog shows the write count.

### Log files

```
/ext/apps_data/nfc_write_debug/logs/YYYYMMDD_HHMMSS_<card>.log
```

Example entry:

```text
+001250ms WRITE block 4
  old: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  new: DE AD BE EF 01 02 03 04 05 06 07 08 09 0A 0B 0C
```

If any writes occurred, a mutated copy is also saved as
`/ext/apps_data/nfc_write_debug/logs/<card>_mutated.nfc`.

## ACK and live re-emulate

**Do you need to ACK writes in the app?** No. The protocol `NfcListener` already
sends the correct ACK/NACK while applying the write to its in-memory card image.

**Can we write back and re-emulate immediately?** The listener *already* serves the
updated memory for the rest of the RF session — no stop/restart. Restarting the
listener mid-field usually breaks the reader session. This app:

1. Detects writes by diffing listener memory every 100 ms
2. Syncs `NfcDevice` to that live image (so later saves include changes)
3. Leaves the same listener running

## Build

```bash
cd nfc_write_debug
ufbt update --channel=release
ufbt
make -C test   # host unit tests for the diff/log helpers
ufbt launch    # USB Flipper
```

## Notes

- Detection is by **data mutation**, not a public “onWrite” callback (SDK 1.5.x).
- Polling can theoretically tear a mid-update unit once; the next poll corrects it.
- Classic/UL emulation quality still depends on keys, access bits, and reader timing
  (same limits as the stock NFC app).
