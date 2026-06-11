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
// RSSI is negative; closer ≈ nearer 0 (e.g. -45), farther ≈ more negative (-95).
#define SKIMMER_LED_RSSI_NEAR  -45     // at/above this → fastest blink
#define SKIMMER_LED_RSSI_FAR   -95     // at/below this → slowest blink
#define SKIMMER_LED_FAST_MS     70     // blink half-period at closest range
#define SKIMMER_LED_SLOW_MS   1000     // blink half-period at farthest range
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

// Some boards wire the addressable LED with red and green swapped, so a value
// the firmware sends as "red" lights up green. The ESP32-C5 dev board's onboard
// RGB LED is one of them, so the swap defaults on there; a user-wired standard
// WS2812B/SK6812 (e.g. on the XIAO) needs no swap. Override for your hardware:
// set to 1 if red shows as green, 0 if colors are already correct.
#ifndef SKIMMER_LED_SWAP_RG
#  if VORD_BOARD_XIAO_C5
#    define SKIMMER_LED_SWAP_RG 0    // user-wired standard pixel: no swap
#  else
#    define SKIMMER_LED_SWAP_RG 1    // C5 dev board onboard LED: R/G swapped
#  endif
#endif
#endif

#endif // CONFIG_H
