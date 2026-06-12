#ifndef CONFIG_H
#define CONFIG_H

// ----- Firmware version -----
#define VORD_FW_VERSION "2.0"

// ----- Task stack sizes -----
#define CYCLE_TASK_STACK   8192

// ----- WiFi Access Point + web dashboard -----
// At boot the device brings up its own WiFi network and serves a live
// dashboard of all observed BLE devices and skimmer detections at
// http://192.168.4.1/. The AP intentionally reports "no internet" to clients
// so phones keep using mobile data while connected to it.
#ifndef AP_SSID
#define AP_SSID       "Vord-C5"
#endif
#ifndef AP_PASSWORD
// WPA2 requires >= 8 characters. Set to "" for an open (passwordless) network.
#define AP_PASSWORD   "Vord2026"
#endif

// Host the AP on 5 GHz (1) or 2.4 GHz (0). The C5 is dual-band. 5 GHz shares no
// spectrum with the 2.4 GHz BLE scan, so it removes in-band interference between
// the AP and BLE (the single radio still time-shares — see BLE_SCAN_* below).
// NOTE: 2.4 GHz-only client devices cannot see a 5 GHz AP. If your phone/laptop
// can't find the network, set this to 0 and reflash.
#ifndef AP_USE_5GHZ
#define AP_USE_5GHZ   1
#endif

#ifndef AP_CHANNEL
#if AP_USE_5GHZ
#define AP_CHANNEL    36   // UNII-1, non-DFS (no radar-wait), widely client-supported
#else
#define AP_CHANNEL    6
#endif
#endif

// 2.4 GHz channel used as a fallback if the 5 GHz AP fails to start.
#ifndef AP_CHANNEL_FALLBACK_2G
#define AP_CHANNEL_FALLBACK_2G 6
#endif

// ----- Battery monitoring (LiPo voltage on the dashboard) -----
// Both supported boards route their battery divider to GPIO6, but with
// different ratios:
//   Waveshare C5 dev kit (WIFI6-KIT): BAT -> 200K/100K divider -> GPIO6,
//     always connected (no enable pin). Ratio 3.0.
//   Seeed XIAO ESP32-C5: BAT -> 100K/100K divider -> GPIO6, gated by a load
//     switch on GPIO26 (drive HIGH to sample). Ratio 2.0.
// Set -DVBAT_ADC_PIN=-1 to disable monitoring (dashboard hides the readout).
#ifndef VBAT_ADC_PIN
#define VBAT_ADC_PIN 6
#endif
#ifndef VBAT_EN_PIN
#  if VORD_BOARD_XIAO_C5
#    define VBAT_EN_PIN 26
#  else
#    define VBAT_EN_PIN -1   // dev kit divider has no enable gate
#  endif
#endif
#ifndef VBAT_DIVIDER
#  if VORD_BOARD_XIAO_C5
#    define VBAT_DIVIDER 2.0f
#  else
#    define VBAT_DIVIDER 3.0f
#  endif
#endif
// How often (ms) to re-sample the battery; readings are cached in between.
#ifndef VBAT_REFRESH_MS
#define VBAT_REFRESH_MS 5000
#endif

// ----- BLE scan timing (coexistence-critical) -----
// WiFi (AP + web) and BLE time-share the single radio. The coexistence
// arbiter can only give WiFi the gaps between scan windows, so the window
// must be well below the interval. Window >= ~60% of interval starves AP
// beacons: clients drop seconds after connecting and HTTP stalls.
#ifndef BLE_SCAN_INTERVAL_MS
#define BLE_SCAN_INTERVAL_MS 160
#endif
#ifndef BLE_SCAN_WINDOW_MS
#define BLE_SCAN_WINDOW_MS   50
#endif

// How often (ms) to restart the BLE scan. An indefinite NimBLE scan never frees
// its internal per-advertiser records, so a flood of unique MACs (e.g. a Flipper
// running BLE-spam) would exhaust the heap and silently kill the scan — taking
// the proximity LED with it. Restarting periodically flushes that list. Short
// enough that the worst-case MAC flood between restarts stays well within free
// heap; long enough that the brief gap at each restart costs negligible coverage.
#ifndef BLE_SCAN_REFRESH_MS
#define BLE_SCAN_REFRESH_MS 1000
#endif

// Max distinct devices kept in the detailed table that feeds the dashboard.
// Bounds RAM; the session unique-MAC counters are tracked separately and higher.
#ifndef BLE_DETAIL_CAP
#define BLE_DETAIL_CAP 300
#endif

// ----- Skimmer suspicious names (defaults; runtime list lives in runtime_config) -----
static const char* SKIMMER_NAMES_DEFAULT[] = {
    // HC-series SPP/BLE modules (all variants; normalization handles dash/case)
    "HC-03", "HC-04", "HC-05", "HC-06", "HC-08",
    "HC03",  "HC04",  "HC05",  "HC06",  "HC08",

    // BT-series modules
    "BT04", "BT05", "BT06", "BT08",

    // CC41 / CC254x clones
    "CC41A", "CC41",

    // JDY family — "JDY" prefix catches JDY-08/09/16/17/18/19/23/24/25/30/31/33 …
    "JDY",
    // Keep explicit entries so they show clearly in the runtime name list
    "JDY-08", "JDY-09", "JDY-16", "JDY-17", "JDY-18",
    "JDY-19", "JDY-23", "JDY-24", "JDY-25",
    "JDY-30", "JDY-31", "JDY-33",

    // CC2541-based BLE modules (TI chip, widely used in skimmer kits)
    "HM-10", "HM-11",
    "AT-09",

    // Other known skimmer-associated module names
    "SPP-CA",
    "LINVOR",
    "MLT-BT05",

    // Flipper Zero BLE attack modes
    "BadKB",      // Flipper HID keyboard spoofing
    "Flipper",    // Catches "xFlipper" and variations via substring match

    nullptr
};

// ----- Proximity alert LED (optional — enabled by -DVORD_HAS_SKIMMER_LED=1) -----
#if VORD_HAS_SKIMMER_LED
// The proximity LED is a single addressable WS2812B / SK6812 pixel, driven on
// one GPIO via rgbLedWrite() (Arduino-ESP32's RMT pixel driver). It is NOT a
// plain on/off LED — pointing this at an ordinary GPIO LED will not work.
//
// Wiring a user-supplied pixel (e.g. on the Seeed XIAO ESP32-C5, which has no
// onboard RGB LED):
//   DIN -> SKIMMER_LED_PIN   VCC -> 3V3   GND -> GND
// Power the pixel from 3V3 (not 5V): one pixel draws < 60 mA and a 3.3 V data
// line is in-spec when the pixel itself runs at 3.3 V, so no level shifter is
// needed. (SK6812 / WS2812B-V5 are the most tolerant if you do power it at 5 V.)
//
// Pin resolution (first match wins):
//   1. -DSKIMMER_LED_PIN=<gpio> on the build command line — always wins; use it
//      to point the firmware at whatever GPIO you wired the pixel to.
//   2. Seeed XIAO ESP32-C5: no onboard pixel → default to the "D0" header pin.
//   3. Boards with an onboard pixel (C5 dev kit / WIFI6-KIT): RGB_BUILTIN.
//   4. Generic fallback GPIO for any other board.
#ifndef SKIMMER_LED_PIN
#  if VORD_BOARD_XIAO_C5
#    define SKIMMER_LED_PIN     D0   // XIAO silkscreen "D0" (GPIO1); override with -DSKIMMER_LED_PIN
#  elif defined(RGB_BUILTIN)
#    define SKIMMER_LED_PIN     RGB_BUILTIN
#  else
#    define SKIMMER_LED_PIN     8    // generic free GPIO; override with -DSKIMMER_LED_PIN
#  endif
#endif
#ifndef SKIMMER_LED_BRIGHTNESS
#define SKIMMER_LED_BRIGHTNESS 40      // 0-255 white level when lit (these LEDs are bright)
#endif
// While a flagged device is in range the LED is steady white and fires short
// color pulses on top (never dark): the white gap between pulses shrinks as
// the device gets closer, and the pulse color sweeps blue → red (see below).
// RSSI is negative; closer ≈ nearer 0 (e.g. -45), farther ≈ more negative (-95).
#define SKIMMER_LED_RSSI_NEAR  -45     // at/above this → fastest pulsing
#define SKIMMER_LED_RSSI_FAR   -95     // at/below this → slowest pulsing
#define SKIMMER_LED_FAST_MS     70     // white gap between color pulses, closest range
#define SKIMMER_LED_SLOW_MS   1000     // white gap between color pulses, farthest range
#define SKIMMER_LED_PULSE_MS   150     // duration of each color pulse
#define SKIMMER_LED_HOLD_MS  10000     // keep blinking this long after the last sighting
#define SKIMMER_LED_TASK_STACK 2048

// Proximity COLOR for the non-white blink phase. The white phase always blinks;
// the alternating phase changes color with distance so range is readable at a
// glance — far → blue, then yellow → orange → red as you close in. Approximate
// ranges (RSSI vs. distance is environment-dependent — tune to taste):
//   ~30 m blue   ~20 m yellow   ~10 m orange   ~1 m red
// Below YELLOW the color is solid blue; from YELLOW up to RED it fades
// yellow → orange → red (full red, green channel shrinking as you approach).
#define SKIMMER_LED_RSSI_RED    -52    // at/above this → solid red (closest, ~1 m)
#define SKIMMER_LED_RSSI_YELLOW -80    // at this → yellow (~20 m); below → blue (~30 m+)

// LED backend. Most C5 boards use a single-wire WS2812B/SK6812 pixel driven via
// rgbLedWrite() (RMT). The LilyGO T-Dongle-C5 instead has an APA102 onboard LED
// (a 2-wire clock+data pixel), which speaks a different protocol — so select the
// APA102 bit-bang backend on that board. All the proximity behaviour (white
// phase, blue → red distance sweep, faster pulses up close) is identical; only
// the wire format differs.
#ifndef SKIMMER_LED_APA102
#  if VORD_BOARD_TDONGLE_C5
#    define SKIMMER_LED_APA102 1
#  else
#    define SKIMMER_LED_APA102 0
#  endif
#endif
#if SKIMMER_LED_APA102
#  ifndef SKIMMER_LED_APA102_DATA_PIN
#    define SKIMMER_LED_APA102_DATA_PIN 5    // T-Dongle-C5 LED_DI (GPIO5)
#  endif
#  ifndef SKIMMER_LED_APA102_CLK_PIN
#    define SKIMMER_LED_APA102_CLK_PIN  4    // T-Dongle-C5 LED_CI (GPIO4)
#  endif
// APA102 has its own 5-bit (0-31) global brightness register on top of the 8-bit
// RGB channels. Keep it modest so the single onboard pixel isn't blinding.
#  ifndef SKIMMER_LED_APA102_BRIGHTNESS
#    define SKIMMER_LED_APA102_BRIGHTNESS 8
#  endif
#endif

// Some boards wire the addressable LED with red and green swapped, so a value
// the firmware sends as "red" lights up green. The ESP32-C5 dev board's onboard
// RGB LED is one of them, so the swap defaults on there; a user-wired standard
// WS2812B/SK6812 (e.g. on the XIAO) and the T-Dongle-C5's APA102 need no swap.
// Override for your hardware: set to 1 if red shows as green, 0 if colors are
// already correct.
#ifndef SKIMMER_LED_SWAP_RG
#  if VORD_BOARD_XIAO_C5 || SKIMMER_LED_APA102
#    define SKIMMER_LED_SWAP_RG 0    // standard pixel order: no swap
#  else
#    define SKIMMER_LED_SWAP_RG 1    // C5 dev board onboard LED: R/G swapped
#  endif
#endif
#endif

// ----- T-Dongle-C5 ST7735 alert display (enabled by -DVORD_HAS_DISPLAY=1) -----
// The LilyGO T-Dongle-C5 carries a 0.96" ST7735 80x160 SPI panel. When a flagged
// device is in range the whole screen flashes RED, and the flash rate tracks
// distance: a stronger RSSI (closer source) flashes faster, a weaker one (farther)
// flashes slowly. Idle shows a calm dim-green "scanning" screen.
//
// Pins are the T-Dongle-C5 board wiring (see its readme pinout). NOTE: LCD_SCK is
// GPIO6 — the same pin the other C5 boards use for the battery ADC divider — so
// battery monitoring is disabled on this board (VBAT_ADC_PIN=-1 in platformio.ini).
#if VORD_HAS_DISPLAY
#ifndef DISPLAY_PIN_MOSI
#define DISPLAY_PIN_MOSI 2     // LCD_MOSI
#endif
#ifndef DISPLAY_PIN_SCK
#define DISPLAY_PIN_SCK  6     // LCD_SCK  (shared with battery ADC on other C5 boards)
#endif
#ifndef DISPLAY_PIN_CS
#define DISPLAY_PIN_CS   10    // LCD_CS
#endif
#ifndef DISPLAY_PIN_DC
#define DISPLAY_PIN_DC   3     // LCD_RS (data/command)
#endif
#ifndef DISPLAY_PIN_RST
#define DISPLAY_PIN_RST  1     // LCD_RST
#endif
#ifndef DISPLAY_PIN_BL
#define DISPLAY_PIN_BL   0     // LCD_BL (backlight)
#endif
#ifndef DISPLAY_BL_ON_LEVEL
#define DISPLAY_BL_ON_LEVEL 0  // backlight-on level (matches the vendor LCD example)
#endif
#ifndef DISPLAY_ROTATION
#define DISPLAY_ROTATION 3     // landscape, 160x80 (USB connector to the side)
#endif

// Flash-rate mapping. RSSI is negative; closer ≈ nearer 0 (e.g. -45), farther ≈
// more negative (-95). The half-period (one red or one black phase) shrinks from
// SLOW (far) to FAST (close), so up close the screen strobes red rapidly while
// far away it blinks slowly. Reused independently of the LED's own thresholds.
#ifndef DISPLAY_RSSI_NEAR
#define DISPLAY_RSSI_NEAR  -45   // at/above this → fastest flashing
#endif
#ifndef DISPLAY_RSSI_FAR
#define DISPLAY_RSSI_FAR   -95   // at/below this → slowest flashing
#endif
#ifndef DISPLAY_FLASH_FAST_MS
#define DISPLAY_FLASH_FAST_MS 60    // half-period at closest range
#endif
#ifndef DISPLAY_FLASH_SLOW_MS
#define DISPLAY_FLASH_SLOW_MS 700   // half-period at farthest range
#endif
#ifndef DISPLAY_HOLD_MS
#define DISPLAY_HOLD_MS 10000       // keep flashing this long after the last sighting
#endif
#ifndef DISPLAY_TASK_STACK
#define DISPLAY_TASK_STACK 4096
#endif
#endif // VORD_HAS_DISPLAY

#endif // CONFIG_H
