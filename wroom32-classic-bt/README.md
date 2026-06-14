# WROOM-32 classic-Bluetooth scout

The ESP32-C5 main board only scans **BLE (LE)** advertisements. Several skimmer
modules in the C5's fingerprint list are **classic Bluetooth (BR/EDR)** parts —
HC-03/04/05/06/08, CC41, SPP-CA, LINVOR, MLT-BT05 — and can **never** match a BLE
scan.

This sketch runs on a classic-BT-capable **ESP32-WROOM-32** placed next to the C5.
It does a classic-BT inquiry, matches discovered device names against the same
skimmer fingerprints the C5 uses, and forwards each hit over UART. The C5 fires
its **existing** LED / screen / dashboard alert — the same alarm as a BLE hit.

The C5 stays the brain. The WROOM-32 is just a "classic-BT nose" beside it: two
separate boards talking over a few wires.

## Wiring (4 wires, no level shifter — both boards are 3.3 V logic)

| WROOM-32              |     | ESP32-C5                          |
| --------------------- | --- | --------------------------------- |
| TX2 (GPIO17)          | →   | `CLASSIC_BT_UART_RX_PIN`          |
| GND                   | →   | GND **(required — shared ground)**|
| 5V (VIN) or 3V3       | →   | same supply                       |
| RX2 (GPIO16)          | →   | *(optional; C5 only listens)*     |

Shared ground is the one critical connection. Both boards can run from the same
USB 5 V.

## Enabling the receiver on the C5

The C5 receiver is compiled out by default. Enable it and point it at the GPIO
you wired to the WROOM-32 TX (pick one that's free on **your** board — avoid the
LED, the battery ADC, and, on the T-Dongle-C5, the display pins):

```ini
build_flags =
    ...
    -DVORD_HAS_CLASSIC_BT_UART=1
    -DCLASSIC_BT_UART_RX_PIN=20
```

`CLASSIC_BT_UART_BAUD` defaults to 115200 and must match `UART_BAUD` in this
sketch. See the `VORD_HAS_CLASSIC_BT_UART` block in [`../src/config.h`](../src/config.h).

## Building the WROOM-32 sketch

- **Arduino IDE**: board **ESP32 Dev Module** (WROOM-32), open `wroom32-classic-bt.ino`, upload.
- **arduino-cli**:
  ```sh
  arduino-cli compile --fqbn esp32:esp32:esp32 wroom32-classic-bt
  arduino-cli upload  --fqbn esp32:esp32:esp32 -p <PORT> wroom32-classic-bt
  ```

Classic Bluetooth (Bluedroid) is enabled by default on the WROOM-32. If the
sketch fails to compile with a "Classic Bluetooth is not enabled" error, the
selected board/partition has BT disabled — choose a standard WROOM-32 profile.

## Line protocol (WROOM → C5, newline-terminated)

```
VBT1|<TYPE>|<RSSI>|<MAC>|<NAME>
```

- `TYPE` — `SKIM` (skimmer module) or `PENT` (reserved for pentest gadgets)
- `RSSI` — integer dBm, negative; best-effort from the classic inquiry
- `MAC`  — `aa:bb:cc:dd:ee:ff`
- `NAME` — device name (sender strips `|`, CR and LF)

Lines that don't start with `VBT1|` are ignored by the C5, so USB-debug chatter
or line noise can't trip the alarm.

## Keeping the fingerprint list in sync

`SKIMMER_NAMES[]` in the sketch mirrors `SKIMMER_NAMES_DEFAULT[]` in
[`../src/config.h`](../src/config.h). When you add a classic-BT module name to one,
add it to the other.
