# ESP32-WROOM-32 classic-Bluetooth add-on

The ESP32-C5 radio only scans **BLE (LE)** advertisements. Several skimmer modules
in the C5's fingerprint list are **classic Bluetooth (BR/EDR)** parts — HC-03/04/05/06/08,
CC41, SPP-CA, LINVOR, MLT-BT05 — and can **never** match a BLE scan.

This add-on bolts a classic-BT-capable **ESP32-WROOM-32** next to the C5. It runs a
classic-BT inquiry, matches discovered device names against the **same** skimmer
fingerprints the C5 uses, and forwards each hit to the C5 over a single UART wire.
The C5 then fires its **existing** alarm — LED, screen, and dashboard
counters/device list — exactly as it does for a BLE hit.

The C5 stays the brain. The WROOM-32 is just a second radio ("classic-BT nose")
beside it: two separate boards talking over a few wires.

---

## Board compatibility

| C5 board                                | Works? | Why |
| --------------------------------------- | ------ | --- |
| **ESP32-C5 Dev Board / Waveshare WIFI6-KIT** | ✅ **Yes — recommended** | Full GPIO header with free pins, 5V/3V3/GND broken out. Easy to solder. |
| **Seeed XIAO ESP32-C5**                 | ⚠️ Possible | Castellated `D0`–`D10` + `5V`/`3V3`/`GND` pads are exposed, but space is tight and `D0` is already the proximity-LED pin. |
| **LilyGO T-Dongle-C5**                  | ❌ No | Sealed USB-stick form factor — almost no exposed GPIO/power pads, and the display + APA102 LED already consume its pins. Nowhere to solder. |

The rest of this guide targets the **ESP32-C5 Dev Board**.

---

## What you need

- 1 × ESP32-WROOM-32 dev board (the common 30/38-pin "DevKitC" / "NodeMCU-32S"
  style — any board with the WROOM-32 module and classic Bluetooth).
- 2–4 jumper wires (or a soldering iron + thin wire for a permanent build).
- A USB cable for the WROOM-32 (to power and flash it).

> Both boards run at **3.3 V logic**, so the UART link needs **no level shifter**.

---

## Wiring / soldering guide (ports)

You only strictly need **two** connections — `TX` and a shared `GND` — if you
power the WROOM-32 from its own USB cable. Add the `5V` wire only if you want the
C5 to power the WROOM-32 from a single USB supply.

| Signal              | WROOM-32 pad        | →   | ESP32-C5 dev board pad | Required? |
| ------------------- | ------------------- | --- | ---------------------- | --------- |
| UART data (BT→C5)   | **GPIO17** (`TX2`)  | →   | **GPIO23** (`IO23`)    | ✅ yes |
| Ground              | **GND**             | →   | **GND**                | ✅ yes (shared ground is critical) |
| Power (optional)    | **5V** / **VIN**    | →   | **5V**                 | only if powering WROOM from the C5 |
| UART data (C5→BT)   | **GPIO16** (`RX2`)  | →   | *(leave unconnected)*  | ❌ no — the C5 only listens |

```
   ESP32-WROOM-32                         ESP32-C5 Dev Board
  ┌───────────────┐                      ┌───────────────────┐
  │  GPIO17 (TX2)●├──────────────────────▶● GPIO23 (IO23)    │
  │          GND ●├──────────────────────●  GND              │
  │       5V/VIN ●├ ─ ─ (  power  ) ─  ─ ●  5V               │
  │               │                      │                   │
  └───────────────┘                      └───────────────────┘
```

**Pins to avoid on the C5 dev board** (already in use): `GPIO27` (onboard RGB
LED), `GPIO6` (battery ADC), `GPIO13`/`GPIO14` (USB), and the strapping pins
`GPIO7`/`GPIO28`. The firmware default is **`GPIO23`**, a free header pin on the
Waveshare ESP32-C5-WIFI6-KIT (`GPIO20` is *not* broken out on that board). If you
wire to a different GPIO, pick another free, non-strapping pin and override
`CLASSIC_BT_UART_RX_PIN` to match (see below).

**Power options**
- *Two USB cables* (simplest): power and flash each board from its own USB. Wire
  only `TX → GPIO23` and `GND → GND`.
- *One USB supply*: power the C5 over USB, then feed the WROOM-32 from the C5's
  `5V` pin (`5V → 5V/VIN`) plus the shared `GND`.

---

## Flashing the WROOM-32

The sketch is [`wroom32-classic-bt.ino`](wroom32-classic-bt.ino). Flash the
WROOM-32 over **its own USB port** (not the C5's).

### Option A — Arduino IDE

1. **Add the ESP32 boards** (once): *File → Preferences →
   Additional boards manager URLs*, add
   `https://espressif.github.io/arduino-esp32/package_esp32_dev_index.json`.
   Then *Tools → Board → Boards Manager…*, install **esp32 by Espressif Systems**.
2. Open `wroom32-classic-bt/wroom32-classic-bt.ino`.
3. *Tools → Board* → **ESP32 Dev Module**.
4. *Tools → Port* → the WROOM-32's COM port.
5. Click **Upload**. If it hangs at "Connecting…", hold the **BOOT** button on the
   WROOM-32 until flashing starts.

### Option B — arduino-cli

```sh
# one-time: install the ESP32 core
arduino-cli config add board_manager.additional_urls \
  https://espressif.github.io/arduino-esp32/package_esp32_dev_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32

# from the repo root
arduino-cli compile --fqbn esp32:esp32:esp32 wroom32-classic-bt
arduino-cli upload  --fqbn esp32:esp32:esp32 -p COM5 wroom32-classic-bt   # set your port
```

> If compile fails with *"Classic Bluetooth is not enabled"*, the selected
> board/partition has BT disabled — use a standard **ESP32 Dev Module** profile.

### Verify it's running

Open the WROOM-32's USB serial monitor at **115200 baud**. You should see:

```
[VordBT] classic-BT scout up; inquiring...
```

and, when a classic-BT skimmer module is in range, a line per hit:

```
[VordBT] hit -> VBT1|SKIM|-55|aa:bb:cc:dd:ee:ff|HC-05
```

Once the C5 is wired and flashed, its dashboard shows a **"Scout"** status pill
(online/offline) driven by the per-round heartbeat, and classic-BT hits carry a
**`classic-BT`** badge in the device list — so you can confirm the link is live
even with no skimmer nearby. See [End-to-end test](#end-to-end-test) below.

---

## Enabling the receiver on the C5

The C5-side receiver is **enabled by default** in the `esp32c5-dev` build (and in
the C5 firmware on the web flasher), listening on **`GPIO23`**. The flags live in
[`../platformio.ini`](../platformio.ini) under `[env:esp32c5-dev]`:

```ini
build_flags =
    ...
    -DVORD_HAS_CLASSIC_BT_UART=1
    -DCLASSIC_BT_UART_RX_PIN=23
```

Leaving it on is harmless when no WROOM-32 is wired — the C5 just listens on an
idle pin. To wire to a different GPIO, change `CLASSIC_BT_UART_RX_PIN`; to turn
the receiver off, set `-DVORD_HAS_CLASSIC_BT_UART=0`. Then rebuild and flash the
C5 normally (`pio run -e esp32c5-dev`).
`CLASSIC_BT_UART_BAUD` defaults to **115200** and must match `UART_BAUD` in the
sketch. See the `VORD_HAS_CLASSIC_BT_UART` block in
[`../src/config.h`](../src/config.h) for all knobs.

### End-to-end test

With both boards wired and flashed, open the C5 dashboard (`http://192.168.4.1/`).
Within ~5 s the **"Scout"** pill should turn green ("Scout 3s ago"); if it stays
grey/"offline", recheck the `TX → IO23` wire and the shared `GND`. Then bring a
classic-BT module (e.g. an HC-05) into range: the C5 should light its proximity LED
(and flash the screen on boards that have one), and the device should appear in the
device list flagged as a skimmer with a **`classic-BT`** badge — the same alarm as a
BLE hit, just tagged by source.

---

## Line protocol (WROOM → C5, newline-terminated)

```
VBT1|<TYPE>|<RSSI>|<MAC>|<NAME>
```

- `TYPE` — `SKIM` (skimmer module), `PENT` (reserved for pentest gadgets), or
  `PING` (a liveness **heartbeat** sent once per inquiry round, carrying no device)
- `RSSI` — integer dBm, negative; best-effort from the classic inquiry (`0` for `PING`)
- `MAC`  — `aa:bb:cc:dd:ee:ff` (all-zero for `PING`)
- `NAME` — device name (sender strips `|`, CR and LF; empty for `PING`)

Any well-formed `VBT1|` line — hit **or** `PING` — refreshes the C5's scout-liveness
timer, which drives the **"Scout"** status pill on the dashboard (online when a line
arrived in the last ~15 s). Only `SKIM`/`PENT` register a device and fire the alert.
Lines that don't start with `VBT1|` are ignored, so USB-debug chatter or line noise
can't trip the alarm.

---

## Keeping the fingerprint list in sync

`SKIMMER_NAMES[]` in the sketch mirrors `SKIMMER_NAMES_DEFAULT[]` in
[`../src/config.h`](../src/config.h). When you add a classic-BT module name to one,
add it to the other.
