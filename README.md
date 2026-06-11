# Vord C5

Vord C5 is a dedicated BLE skimmer detector firmware for ESP32-C5 hardware. Flash from here [VordC5](https://pierregode.github.io/VordC5/)

This rebuild removes wardriving and serial command streaming entirely. The firmware now runs continuous BLE scanning all the time and focuses only on suspicious skimmer-class BLE modules.

## Supported hardware

- ESP32-C5 Dev Board / Waveshare ESP32-C5-WIFI6-KIT (16 MB flash)
- Seeed XIAO ESP32-C5 (8 MB flash)

## Runtime behavior

- BLE scan starts at boot and runs continuously.
- A WiFi Access Point + web dashboard start at boot (see below).
- Detection is local on the device, no host protocol required.
- Onboard RGB LED is used as proximity feedback:
- Red/white blink pattern means skimmer signature detected.
- Faster blinking means stronger RSSI (closer source).
- No wardrive mode.
- No serial command parser loop.

## WiFi dashboard

At boot the device brings up its own WiFi Access Point and serves a live web
dashboard. BLE scanning keeps running the whole time (WiFi and BLE share the
single C5 radio via the chip's coexistence, so the BLE scan deliberately runs
at a reduced duty cycle to leave airtime for the AP — see
[`BLE_SCAN_*` in src/config.h](src/config.h)).

1. Connect to the WiFi network **`Vord-C5`** (default password
   **`Vord2026`**).
2. Most phones pop the captive-portal "sign in" sheet automatically. Otherwise
   open **http://192.168.4.1/** in a browser (this is the AP gateway address).

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

## Build

### PlatformIO (ESP32-C5 Dev Board)

```bash
pio run -e esp32c5-dev
```

### Arduino CLI (Seeed XIAO ESP32-C5)

```bash
bash scripts/build-xiao.sh
```

## Web flasher

Web flasher in docs now targets only the two supported C5 profiles:

- ESP32-C5 Dev Board profile
- Seeed XIAO ESP32-C5 profile

The GitHub Actions deploy workflow builds both binaries and publishes the flasher site with C5-only manifests.

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

This Code of Conduct is adapted from the Contributor Covenant, version 2.0

This code of conduct is adapted from the Contributor Covenant, version 2.0.

