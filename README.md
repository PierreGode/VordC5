<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/img/title-vordc5-dark.svg" />
  <img alt="Vord C5" src="docs/img/title-vordc5-light.svg" />
</picture>

Vord C5 is a dedicated BLE skimmer detector firmware for ESP32-C5 hardware. 

> ℹ️ **Detection only — nothing else.** Vord C5 *only* listens for and flags
> suspicious BLE advertisements. It does **not** connect to, interfere with,
> jam, block, modify, or attack any device, and it does not transmit anything to
> the devices it detects. It is a passive, **educational** tool that simply tells
> you a skimmer-class signature is nearby — what you do with that information is
> up to you (see [What to do if you find a skimmer](docs/found-a-skimmer.md)). A
> match is a *signal to be cautious*, not proof.

Flash from here [VordC5](https://pierregode.github.io/VordC5/)

The firmware now runs continuous BLE scanning all the time and focuses only on suspicious skimmer-class BLE modules.

<img width="700" height="320" alt="image" src="https://github.com/user-attachments/assets/c2ba3d3f-89db-4c16-9c44-b1ccde51580d" />


## Supported hardware

- ESP32-C5 Dev Board / Waveshare ESP32-C5-WIFI6-KIT (16 MB flash)
- Seeed XIAO ESP32-C5 (8 MB flash)
- LilyGO T-Dongle-C5 (16 MB flash, 0.96" ST7735 screen + onboard APA102 LED) —
  the **screen** flashes red and the **onboard RGB LED** gives proximity feedback,
  both at once (see below)

## Runtime behavior

- BLE scan starts at boot and runs continuously.
- A WiFi Access Point + web dashboard start at boot (see below).
- Detection is local on the device, no host protocol required.
- An addressable RGB LED (WS2812B/SK6812) is used as proximity feedback:
- Steady white means a skimmer signature is detected (LED never dark while in range).
- Short color pulses on top of the white code the distance: blue (far) → yellow → orange → red (close).
- Faster pulsing means stronger RSSI (closer source).
- No wardrive mode.
- No serial command parser loop.

## Proximity LED wiring

The proximity LED is a single addressable **WS2812B / SK6812** pixel (the same
type as an onboard "NeoPixel"), driven over one GPIO. It is *not* a plain on/off
LED.

- **ESP32-C5 Dev Board / WIFI6-KIT** — uses the onboard pixel automatically
  (`RGB_BUILTIN`). Nothing to wire.
- **Seeed XIAO ESP32-C5** — has **no** onboard RGB LED, so connect your own pixel:

  | Pixel pin | Connect to |
  |-----------|------------|
  | DIN       | `D0` (default; GPIO1) |
  | VCC       | `3V3` |
  | GND       | `GND` |

  Power the pixel from **3V3, not 5V**. A single pixel draws < 60 mA, and at a
  3.3 V supply the 3.3 V data line is in spec — no level shifter needed. (If you
  must run it at 5 V, an SK6812 or WS2812B-V5 tolerates 3.3 V data best.)

**Using a different GPIO.** Override the default pin at build time:

- XIAO build: `XIAO_SKIMMER_LED_GPIO=2 bash scripts/build-xiao.sh`
- PlatformIO: add `-DSKIMMER_LED_PIN=2` to `build_flags` in
  [`platformio.ini`](platformio.ini) (also works to drive an external pixel on
  the dev board).

See the LED notes in [`src/config.h`](src/config.h) for the full pin-resolution
order and brightness/timing knobs (`SKIMMER_LED_*`).

## T-Dongle-C5 alerts (screen + onboard LED)

The LilyGO T-Dongle-C5 has both a built-in 0.96" ST7735 (80×160) display and an
onboard RGB LED, and the firmware uses **both** at once:

- **Screen.** When a skimmer-class device is in range the whole screen flashes
  **RED**, and the flash rate codes the distance — a stronger signal (closer
  source) flashes faster, a weaker one (farther) flashes slowly. When nothing has
  been seen for a few seconds the screen returns to a calm dim-green
  "scanning / all clear" state.
- **Onboard LED.** The same proximity feedback as the other C5 boards: steady
  white while a flagged device is in range, with short color pulses on top that
  sweep blue → yellow → orange → red as you close in and pulse faster the nearer
  the source.

Nothing to wire — both are on-board. The screen pins, flash-rate window
(`DISPLAY_RSSI_NEAR`/`_FAR`) and speeds (`DISPLAY_FLASH_FAST_MS`/`_SLOW_MS`) live
in the `VORD_HAS_DISPLAY` block of [`src/config.h`](src/config.h); the LED knobs
are the `SKIMMER_LED_*` block. Notes on this board:

- The onboard LED is an **APA102** (2-wire clock+data on GPIO4/5), not a WS2812B,
  so `skimmer_led` drives it through its APA102 backend (`SKIMMER_LED_APA102=1`)
  instead of `rgbLedWrite()`. The proximity behaviour is identical; only the wire
  protocol differs. Brightness is `SKIMMER_LED_APA102_BRIGHTNESS` (0–31).
- Battery monitoring is disabled because the battery ADC pin (GPIO6) is the
  display's SPI clock on this board.

Build it with:

```bash
pio run -e tdongle-c5
```

## WiFi dashboard

At boot the device brings up its own WiFi Access Point and serves a live web
dashboard. BLE scanning keeps running the whole time (WiFi and BLE share the
single C5 radio via the chip's coexistence, so the BLE scan deliberately runs
at a reduced duty cycle to leave airtime for the AP — see
[`BLE_SCAN_*` in src/config.h](src/config.h)).

1. Connect to the WiFi network **`Vord-C5`** (default password
   **`Vord2026`**).
2. Open **http://192.168.4.1/** in a browser (this is the AP gateway address).
   Your phone will report "no internet" on this network — that is intentional:
   it makes the phone keep using mobile data for everything else while the
   dashboard stays reachable over WiFi. If Android asks, choose "stay
   connected".

By default the AP is hosted on **5 GHz** (`AP_USE_5GHZ`), which avoids 2.4 GHz
in-band interference with the BLE scan. If a client device is 2.4 GHz-only and
can't see the network, set `AP_USE_5GHZ` to `0` in
[`src/config.h`](src/config.h) and reflash; the firmware also falls back to
2.4 GHz automatically if the 5 GHz AP fails to start. The actual AP address and
band are printed over USB serial at boot.

The dashboard shows:

- Session totals: unique BLE devices and unique skimmers seen since boot.
- Live hit counters for the current scan.
- A searchable, sortable table of every observed device — name, MAC, latest
  RSSI (with signal bar), advertisement count, and time since last seen.
  Skimmer-flagged devices are highlighted; a "Skimmers only" filter is included.

The page auto-refreshes every 2 seconds via small JSON endpoints
(`/api/status`, `/api/devices`).

The AP SSID, password, band, and channel are configurable in
[`src/config.h`](src/config.h) (`AP_SSID`, `AP_PASSWORD`, `AP_USE_5GHZ`,
`AP_CHANNEL`); set `AP_PASSWORD` to `""` for an open network. The dashboard
tracks up to `BLE_DETAIL_CAP` (default 300) distinct devices in detail.

## Detection strategy

Vord C5 classifies suspicious devices by BLE advertised name fingerprints.

**Default fingerprints include:**

**Classic Bluetooth SPP/BLE modules** (widely used in gas pump skimmers):
- HC-03, HC-04, HC-05, HC-06, HC-08
- BT-04, BT-05, BT-06, BT-08
- CC41, CC41A
- SPP-CA, LINVOR

**BLE-specific modules** (Bluetooth Low Energy skimmer hardware):
- JDY family (JDY-08/09/16/17/18/19/23/24/25/30/31/33 and variants)
- HM-10, HM-11 (TI CC2541 chipset, documented in skimmer kit teardowns)
- AT-09 (CC2541 clone)
- MLT-BT05

**Flipper Zero detection:**
- BadKB (Flipper HID keyboard spoofing mode)
- Flipper (catches "xFlipper" and device name variants)

Matching is normalized and case-insensitive, with tolerance for separators (dash, underscore, dot, space).
So `HC-05`, `HC05`, and `hc_05` all match. The JDY prefix rule catches all JDY-xx variants automatically.

## What to do if you find a skimmer

**Do not touch it, do not tamper with it — call the authorities.** A flagged
skimmer is evidence in a crime; handling it can destroy evidence, may be illegal,
and can be dangerous.

👉 **Read the full guide: [What to do if you find a skimmer](docs/found-a-skimmer.md)**

## Build

### PlatformIO (ESP32-C5 Dev Board)

```bash
pio run -e esp32c5-dev
```

### PlatformIO (LilyGO T-Dongle-C5)

```bash
pio run -e tdongle-c5
```

### Arduino CLI (Seeed XIAO ESP32-C5)

```bash
bash scripts/build-xiao.sh
```

## Web flasher

The web flasher in docs targets the three supported C5 profiles, each its own
button:

- ESP32-C5 Dev Board profile
- Seeed XIAO ESP32-C5 profile
- LilyGO T-Dongle-C5 profile

The GitHub Actions deploy workflow builds all three binaries and publishes the
flasher site with C5-only manifests.

## Notes

- This project does not use eFuse operations.
- Flashing is regular firmware write only.
- **Recent improvements:** Extended fingerprint list with HC-03/04, BT-04/08, HM-10/11, AT-09, full JDY family, and Flipper Zero detection (BadKB, xFlipper). Fixed BLE callback duplicate filter to enable continuous RSSI tracking and LED proximity feedback.


(Educational Tool – Please Read Carefully)

Take note about this… Take note…

🤝 Our Commitment

This project is an educational tool, and we are committed to fostering an open and welcoming environment for all participants. Everyone who contributes to Ragnar is expected to follow the guidelines in this Code of Conduct.

🌟 Expected Behavior

Respect: Be respectful of differing viewpoints, cultures, and experiences.

Constructive Feedback: Give constructive feedback and remain open to receiving it.

Empathy & Kindness: Show understanding, patience, and kindness to others.

Respect for Maintainers: Acknowledge and respect decisions made by maintainers.

Positive Intent: Assume good intentions in discussions and collaborations.

🚫 Unacceptable Behavior

Harassment or Discrimination: Any form of harassment, discrimination, or hateful conduct.

Inappropriate Language or Imagery: Use of sexually explicit, violent, or offensive content.

Personal Attacks: Insults, threats, or targeted personal criticism.

Public or Private Harassment: Unwanted communication or intimidation in any form.

⚖️ Enforcement

Project maintainers are responsible for interpreting and enforcing this Code of Conduct. Violations may result in actions such as warnings, temporary restrictions, or permanent removal from the project or related communication channels.

🙏 Acknowledgments
#Pierre

This Code of Conduct is adapted from the Contributor Covenant, version 2.0

This code of conduct is adapted from the Contributor Covenant, version 2.0.

