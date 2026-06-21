# ESP32-WROOM-32 classic-Bluetooth add-on

The ESP32-C5 radio only scans **BLE (LE)** advertisements. Several skimmer modules
in the C5's fingerprint list are **classic Bluetooth (BR/EDR)** parts — HC-03/04/05/06/08,
CC41, SPP-CA, LINVOR, MLT-BT05, FREE2MOVE — and can **never** match a BLE scan.

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
| **ESP32-C5 Dev Board / Waveshare WIFI6-KIT** | ✅ **Yes — recommended** | Full GPIO header with free pins, 5V/3V3/GND broken out. Easy to solder. → [Option A](#option-a--esp32-c5-dev-board--wroom-32-recommended) |
| **Seeed XIAO ESP32-C5**                 | ✅ Yes — tighter | Castellated `D0`–`D10` + `5V`/`3V3`/`GND` pads are exposed; space is tight and `D0` is the LED, so the link goes on `D2`. → [Option B](#option-b--seeed-xiao-esp32-c5--wroom-32) |
| **LilyGO T-Dongle-C5**                  | ❌ No | Sealed USB-stick form factor — almost no exposed GPIO/power pads, and the display + APA102 LED already consume its pins. Nowhere to solder. |

The WROOM-32 side is identical for both — only the C5 pin you wire to and the
build command differ. Pick your combo below.

---

## What you need

- 1 × **ESP32-WROOM-32** — a **dev board** (DevKitC / NodeMCU-32S) is easiest (USB,
  regulator and boot circuit are all on board), or a **bare module** (ESP-32S) if
  you're building a custom PCB.
- 1 × **ESP32-C5 board** — the Dev Board (Option A) or the Seeed XIAO ESP32-C5
  (Option B).
- 2–4 jumper wires, or a soldering iron + thin wire for a permanent build.
- Power for the WROOM-32 (its own USB on a dev board, or a 3.3 V supply for a bare
  module).

> Both boards run at **3.3 V logic**, so the UART link needs **no level shifter**.

For a permanent unit you can reflow a **bare WROOM-32 module** (e.g. the ESP-32S)
onto a custom carrier PCB instead of using a dev board — see
[Building on a custom PCB](#building-on-a-custom-pcb-bare-module) for the power,
flashing and RF-layout requirements.

---

## Option A — ESP32-C5 Dev Board + WROOM-32 (recommended)

Wire **two** signals — `TX` and a shared `GND`. Add the `5V` wire only if you want
the C5 to power the WROOM-32 from a single USB supply.

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

**Pins to avoid on the dev board** (already in use): `GPIO27` (onboard RGB LED),
`GPIO6` (battery ADC), `GPIO13`/`GPIO14` (USB), and the strapping pins
`GPIO7`/`GPIO28`. The firmware default is **`GPIO23`**, a free header pin on the
Waveshare ESP32-C5-WIFI6-KIT (`GPIO20` is *not* broken out on that board). If you
wire to a different GPIO, pick another free, non-strapping pin and override
`CLASSIC_BT_UART_RX_PIN` to match.

**Power options**
- *Two USB cables* (simplest): power and flash each board from its own USB. Wire
  only `TX → GPIO23` and `GND → GND`.
- *One USB supply*: power the C5 over USB, then feed the WROOM-32 from the C5's
  `5V` pin (`5V → 5V/VIN`) plus the shared `GND`.

**Receiver:** already **enabled by default** in the `esp32c5-dev` build (and in the
web-flasher C5 firmware), listening on **`GPIO23`**. The flags live in
[`../platformio.ini`](../platformio.ini) under `[env:esp32c5-dev]`:

```ini
build_flags =
    ...
    -DVORD_HAS_CLASSIC_BT_UART=1
    -DCLASSIC_BT_UART_RX_PIN=23
```

Rebuild/flash with `pio run -e esp32c5-dev`. To wire a different GPIO, change
`CLASSIC_BT_UART_RX_PIN`; to turn the receiver off, set
`-DVORD_HAS_CLASSIC_BT_UART=0`.

---

## Option B — Seeed XIAO ESP32-C5 + WROOM-32

Smaller and tighter, but fully supported. Wire **two** signals to the XIAO's pads:

| Signal              | WROOM-32 pad        | →   | XIAO ESP32-C5 pad      | Required? |
| ------------------- | ------------------- | --- | ---------------------- | --------- |
| UART data (BT→C5)   | **GPIO17** (`TX2`)  | →   | **`D2`** (GPIO25)      | ✅ yes |
| Ground              | **GND**             | →   | **GND**                | ✅ yes (shared ground is critical) |

```
   ESP32-WROOM-32                         Seeed XIAO ESP32-C5
  ┌───────────────┐                      ┌───────────────────┐
  │  GPIO17 (TX2)●├──────────────────────▶● D2 (GPIO25)      │
  │          GND ●├──────────────────────●  GND              │
  └───────────────┘                      └───────────────────┘
       ▲
       └── own 3.3 V supply (see power warning)
```

**Receiver:** the XIAO build compiles it in **by default** on **`D2`/GPIO25** —
`scripts/build-xiao.sh` sets `-DCLASSIC_BT_UART_RX_PIN=25`. `D2` is a free,
non-strapping pad that avoids the proximity LED (`D0`/GPIO1), I²C (`D4`/`D5`),
SPI (`D8`–`D10`), the `D6`/`D7` UART0 debug pads, USB (GPIO13/14) and the battery
pins (GPIO6/26). Build and flash with `bash scripts/build-xiao.sh`. To use a
different pad: `XIAO_CLASSIC_BT_RX_GPIO=<gpio> bash scripts/build-xiao.sh` (or
`=off` to compile the receiver out).

> ⚠️ **Power:** don't run a WROOM-32 doing continuous classic-BT inquiry off the
> XIAO's small 3V3 LDO — the BT TX current peaks can brown out the XIAO and drop
> its AP. Give the WROOM its own supply (its own USB, or a separate 3V3/5V source)
> and share **only GND** with the XIAO.

> **Both options:** `CLASSIC_BT_UART_BAUD` defaults to **115200** and must match
> `UART_BAUD` in the sketch. Leaving the receiver on with nothing wired is harmless
> (the C5 just listens on an idle pin). See the `VORD_HAS_CLASSIC_BT_UART` block in
> [`../src/config.h`](../src/config.h) for all knobs.

---

## Building on a custom PCB (bare module)

A dev board is easiest for a prototype, but for a permanent C5 + WROOM-32 + LED
unit a **bare WROOM-32 module** (e.g. the ESP-32S) reflowed onto a custom carrier
PCB is the right build — bare modules are *made* for this. The catch: a bare
module has **no USB, no voltage regulator and no boot circuit**, so the carrier
has to provide them.

**Power.** The module runs on **3.3 V only** and pulls ~250 mA with ~500 mA TX
peaks. Give it a dedicated regulator — do **not** feed 5 V to its `3V3` pin, and
do **not** run it off the C5/XIAO's small LDO (the peaks brown out the C5 and drop
its AP). Recommended single-connector topology for a XIAO build:

```
USB-C (XIAO) ─VBUS 5V─┬─> XIAO 5V pad ──(XIAO LDO)──> XIAO 3V3 ──> LED VCC
                      └─> U2 reg (5V→3.3V, ≥1A) ─────> WROOM 3V3
```

`U2` = AMS1117-3.3 (simple) or a small buck (MP1584 / TPS62, runs cooler). One
common ground ties the XIAO, WROOM, LED and regulator together.

**Carrier netlist**

| Net        | Connections |
| ---------- | ----------- |
| 5V / VBUS  | XIAO `5V` → `U2` VIN (+ 10 µF) |
| 3V3_WROOM  | `U2` VOUT → WROOM `3V3` (+ 10 µF ‖ 0.1 µF) + EN pull-up |
| 3V3_XIAO   | XIAO `3V3` → LED `VCC` (+ 0.1 µF at pixel) |
| GND        | XIAO, WROOM, LED, `U2`, all caps — one pour |
| LED data   | XIAO `D0` (GPIO1) → [opt 330 Ω] → LED `DIN` |
| BT UART    | WROOM `GPIO17` (TX2) → XIAO `D2` (GPIO25) |

**Module support parts (don't skip).** 10 µF ‖ 0.1 µF on WROOM `3V3`; `EN` →
10 kΩ to 3V3 **plus** 0.1–1 µF to GND (clean power-on reset); `GPIO0` → 10 kΩ to
3V3. Leave the boot-strapping pins alone — don't pull `GPIO12` (MTDI) high (must
be low for 3.3 V flash), nor `GPIO2` / `GPIO15`. You only use the non-strapping
`GPIO17`, so this is just "don't tie those pins to anything."

**Flashing a bare module (no USB).** Bring out a 6-pin header — `3V3`, `GND`,
`U0TXD` (GPIO1), `U0RXD` (GPIO3), `EN`, `GPIO0` — plus a **BOOT** button
(GPIO0→GND) and an **EN/RST** button (EN→GND). Flash with a 3.3 V USB-TTL adapter
(adapter `TX→GPIO3`, `RX→GPIO1`, shared GND): hold `GPIO0` low while tapping `EN`
to enter download mode, then run `bash scripts/build-wroom32.sh` and upload. For a
button-free board, add an onboard CH340/CP2102 plus the 2-transistor auto-reset
circuit on EN/GPIO0 and flash over USB.

**RF layout (make-or-break at 2.4 GHz).** Place **both** modules at board edges
with their PCB antennas overhanging, and keep **zero copper** — no pour, no
traces, no ground — under or beside each antenna out to the edge (~15 mm clearance
to any metal). Put the two antennas on **opposite edges** pointing outward (both
are 2.4 GHz, so separation cuts desense). Solid ground pour + stitching vias
everywhere else; route the UART and LED-data traces away from the antennas.

---

## Flashing the WROOM-32

The sketch is [`wroom32-classic-bt.ino`](wroom32-classic-bt.ino). On a **dev
board**, flash over its own USB port (not the C5's). For a **bare module**, see
[the flashing rig above](#building-on-a-custom-pcb-bare-module).

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

# from the repo root (or: bash scripts/build-wroom32.sh to just compile)
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

---

## End-to-end test

With both boards wired and flashed, open the C5 dashboard (`http://192.168.4.1/`).
Within ~5 s the **"Scout"** pill should turn green ("Scout 3s ago"); if it stays
grey/"offline", recheck the `TX` wire (**`IO23`** on the dev board, **`D2`** on the
XIAO) and the shared `GND`. Then bring a classic-BT module (e.g. an HC-05) into
range: the C5 should light its proximity LED (and flash the screen on boards that
have one), and the device should appear in the device list flagged as a skimmer
with a **`classic-BT`** badge — the same alarm as a BLE hit, just tagged by source.

The "Scout" pill is driven by the per-round heartbeat, so it confirms the link is
live even with no skimmer nearby.

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
