# ESP32-WROOM-32 classic-Bluetooth add-on

The ESP32-C5 radio only scans **BLE (LE)**. Several skimmer modules in Vord's
fingerprint list are **classic Bluetooth (BR/EDR)** parts — HC-03/04/05/06/08,
CC41, SPP-CA, LINVOR, MLT-BT05, FREE2MOVE — and can **never** appear in a BLE
scan. To cover them, a classic-BT-capable **ESP32-WROOM-32** rides alongside the
C5 as a "classic-BT nose": it runs a BR/EDR inquiry, matches discovered device
names against the **same** fingerprints the C5 uses, and forwards each hit to the
C5 over one UART wire. The C5 then fires its **existing** alarm — LED, screen,
dashboard counters/device list — exactly as for a BLE hit.

The C5 stays the brain; the WROOM-32 is just a second radio beside it. No C5 chip
(C5/C6/S3) can do classic Bluetooth, so this second radio is the *only* way to see
HC-05-class modules — there's no firmware-only substitute.

---

## The build — Seeed XIAO ESP32-C5 + WROOM-32 (+ LED)

This is the primary, intended build, and it's what the **Vord carrier PCB**
integrates: a Seeed XIAO ESP32-C5 (the brain), a WROOM-32 module (the classic-BT
nose), a WS2812/SK6812 proximity LED, and a 3.3 V regulator for the WROOM, all on
one board.

| Part | Role |
| ---- | ---- |
| **Seeed XIAO ESP32-C5** | BLE scan + WiFi dashboard + drives the LED + listens to the WROOM over UART |
| **ESP32-WROOM-32** (bare module / ESP-32S, or a dev board) | classic-BT (BR/EDR) inquiry → forwards hits to the XIAO |
| **WS2812B / SK6812 pixel** | proximity LED (white in range; blue→red by distance) |

> Everything runs at **3.3 V logic**, so the UART link needs **no level shifter**.

### Connections

Each row is one net — the listed pins all tie together:

| # | XIAO ESP32-C5 | WROOM-32 | LED pixel | Signal |
|---|---------------|----------|-----------|--------|
| 1 | **D0** (GPIO1) | — | **DIN** | LED data |
| 2 | **3V3** | — | **VCC** | LED power |
| 3 | **GND** | **GND** | **GND** | common ground (required) |
| 4 | **D2** (GPIO25) | **TX2** (GPIO17) | — | classic-BT data → C5 |

```
            WS2812 / SK6812 pixel
            DIN     VCC     GND
             │       │       │
        ┌────┴───────┴───────┴────┐
        │ D0        3V3      GND   │
        │            XIAO ESP32-C5 │   ← USB-C power/flash
        │ D2                  GND  │
        └────┬────────────────┬────┘
             │ UART           │ GND
        ┌────┴────────────────┴────┐
        │ TX2 (GPIO17)        GND   │
        │        ESP32-WROOM-32     │   ← own 3.3 V (see Power)
        └───────────────────────────┘
```

`D2`/GPIO25 is a free, non-strapping XIAO pad: it avoids the LED (`D0`/GPIO1), I²C
(`D4`/`D5`), SPI (`D8`–`D10`), the `D6`/`D7` UART0 debug pads, USB (GPIO13/14) and
the battery pins (GPIO6/26). The WROOM's `RX2` is left unconnected — the C5 only
listens.

### Power

A WROOM-32 is a *second radio*: ~80–160 mA average but **250–500 mA TX bursts**
during inquiry, landing on top of the XIAO's own WiFi/BLE peaks. Low, steady loads
(a GPS, an OLED) run straight off the XIAO's `3V3` — the WROOM just needs headroom
for its spikes. The carrier PCB powers it for the peaks:

```
USB-C (XIAO) ─VBUS 5V─┬─> XIAO 5V pad ──(XIAO LDO)──> XIAO 3V3 ──> LED VCC
                      └─> U2 reg (5V→3.3V, ≥1A) ─────> WROOM 3V3
```

`U2` = AMS1117-3.3 (simple) or a small buck (MP1584 / TPS62, runs cooler). This
keeps the WROOM's bursts off the XIAO's LDO so the XIAO's own WiFi stays rock-steady.

Powering options, best first:
1. **XIAO `5V` → dedicated 3.3 V regulator → WROOM** (what the PCB does).
2. **XIAO `5V` → a WROOM *dev board's* VIN/5V** — its onboard regulator absorbs the bursts.
3. **XIAO `3V3` → WROOM + a ≥470 µF (ideally 1000 µF) bulk cap** at the WROOM's `3V3` pin — cheapest; leans on the XIAO LDO's headroom.

Never feed 5 V to a bare module's `3V3` pin (it has no regulator). Always share **GND**.

### Module support parts (bare WROOM module)

A bare module has no regulator, no boot circuit — the carrier provides them:
- **3V3:** 10 µF ‖ 0.1 µF decoupling (plus the bulk cap if sharing the XIAO LDO).
- **EN:** 10 kΩ to 3V3 **plus** 0.1–1 µF to GND (clean power-on reset).
- **GPIO0:** 10 kΩ to 3V3.
- **Strapping — leave alone:** don't pull `GPIO12` (MTDI) high (must be low for 3.3 V flash), nor `GPIO2` / `GPIO15`. Only `GPIO17` (non-strapping) is used.
- **LED:** 0.1 µF at the pixel; optional 330 Ω series on `DIN`.

### RF layout (2.4 GHz — make-or-break)

Place **both** modules at board edges with their PCB antennas overhanging, and keep
**zero copper** (no pour, traces or ground) under or beside each antenna out to the
edge (~15 mm clearance to any metal). Put the two antennas on **opposite edges**
pointing outward — both are 2.4 GHz, so separation cuts desense. Solid ground pour
+ stitching vias elsewhere; route the UART and LED-data traces away from the antennas.

---

## Firmware

Two boards, two binaries.

### XIAO ESP32-C5 (the brain)

The classic-BT receiver is compiled in **by default** on **`D2`/GPIO25**:

```sh
bash scripts/build-xiao.sh      # then flash over the XIAO's USB-C
```

`scripts/build-xiao.sh` sets `-DVORD_HAS_CLASSIC_BT_UART=1 -DCLASSIC_BT_UART_RX_PIN=25`.
To use a different pad: `XIAO_CLASSIC_BT_RX_GPIO=<gpio> bash scripts/build-xiao.sh`
(or `=off` to drop the receiver). `CLASSIC_BT_UART_BAUD` defaults to **115200** and
must match `UART_BAUD` in the scout sketch. Leaving the receiver on with no WROOM
wired is harmless — the C5 just listens on an idle pin. All knobs live in the
`VORD_HAS_CLASSIC_BT_UART` block of [`../src/config.h`](../src/config.h).

### WROOM-32 scout (the classic-BT nose)

The sketch is [`wroom32-classic-bt.ino`](wroom32-classic-bt.ino):

```sh
bash scripts/build-wroom32.sh   # compile (FQBN esp32:esp32:esp32)
```

**Flashing a bare module on the PCB** (no USB): the carrier brings out the WROOM's
UART0 + boot pins. Use a 3.3 V USB-TTL adapter — adapter `TX → GPIO3` (U0RXD),
`RX → GPIO1` (U0TXD), shared GND — and enter download mode by holding **GPIO0** low
while tapping **EN**, then upload:

```sh
arduino-cli upload --fqbn esp32:esp32:esp32 -p <PORT> wroom32-classic-bt
```

A board with an onboard CH340/CP2102 + auto-reset flashes over USB with no buttons.
On a **WROOM-32 dev board**, just use its own USB port (Arduino IDE → *ESP32 Dev
Module*, or `arduino-cli upload`).

> If compile fails with *"Classic Bluetooth is not enabled"*, the selected
> board/partition has BT disabled — use a standard **ESP32 Dev Module** profile.

---

## Verify (end-to-end)

1. Power the WROOM and open its USB serial at **115200 baud**. You should see:
   ```
   [VordBT] classic-BT scout up; inquiring...
   ```
   and, on a hit:
   ```
   [VordBT] hit -> VBT1|SKIM|-55|aa:bb:cc:dd:ee:ff|HC-05
   ```
2. Open the C5 dashboard at `http://192.168.4.1/`. Within ~5 s the **"Scout"** pill
   turns green (driven by the per-round heartbeat — it confirms the link even with
   no skimmer nearby). If it stays grey/"offline", recheck the `TX2 → D2` wire and
   the shared `GND`.
3. Bring a classic-BT module (e.g. an HC-05) into range: the LED lights, and the
   device appears in the list flagged as a skimmer with a **`classic-BT`** badge —
   the same alarm as a BLE hit, tagged by source.

---

## Alternative — ESP32-C5 Dev Board (prototyping)

For breadboarding without the PCB, the ESP32-C5 Dev Board / Waveshare WIFI6-KIT
works too (full GPIO header, 5V/3V3/GND broken out). On it the receiver listens on
**`GPIO23`** by default (set in [`../platformio.ini`](../platformio.ini), built
with `pio run -e esp32c5-dev`):

| Signal | WROOM-32 | → | ESP32-C5 Dev Board | Required? |
| ------ | -------- | - | ------------------ | --------- |
| UART data | **GPIO17** (`TX2`) | → | **GPIO23** (`IO23`) | ✅ yes |
| Ground | **GND** | → | **GND** | ✅ yes (shared) |
| Power (opt.) | **5V** / **VIN** | → | **5V** | only to power WROOM from the C5 |

Avoid `GPIO27` (LED), `GPIO6` (battery ADC), `GPIO13`/`GPIO14` (USB), and strapping
pins `GPIO7`/`GPIO28`. `GPIO20` is *not* broken out on the Waveshare board (hence
`GPIO23`). Power and flash each board from its own USB, or feed the WROOM from the
C5's `5V` pin.

> The **LilyGO T-Dongle-C5 is not usable** for this add-on — sealed USB-stick with
> no exposed GPIO/power pads and its pins already taken by the display + APA102 LED.

---

## Line protocol (WROOM → C5, newline-terminated)

```
VBT1|<TYPE>|<RSSI>|<MAC>|<NAME>
```

- `TYPE` — `SKIM` (skimmer module), `PENT` (reserved for pentest gadgets), or
  `PING` (a liveness **heartbeat** sent once per inquiry round, no device).
- `RSSI` — integer dBm, negative; best-effort from the inquiry (`0` for `PING`).
- `MAC`  — `aa:bb:cc:dd:ee:ff` (all-zero for `PING`).
- `NAME` — device name (sender strips `|`, CR and LF; empty for `PING`).

Any well-formed `VBT1|` line — hit or `PING` — refreshes the scout-liveness timer
behind the dashboard "Scout" pill (online if a line arrived in the last ~15 s).
Only `SKIM`/`PENT` register a device and fire the alert. Lines not starting with
`VBT1|` are ignored, so USB-debug chatter or line noise can't trip the alarm.

---

## Keeping the fingerprint list in sync

`SKIMMER_NAMES[]` in [`wroom32-classic-bt.ino`](wroom32-classic-bt.ino) mirrors
`SKIMMER_NAMES_DEFAULT[]` in [`../src/config.h`](../src/config.h). When you add a
classic-BT module name to one, add it to the other.
