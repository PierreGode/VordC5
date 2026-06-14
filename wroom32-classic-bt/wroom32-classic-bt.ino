// =====================================================================
//  Vord — WROOM-32 classic-Bluetooth scout
//
//  The ESP32-C5 main board can only scan BLE (LE) advertisements. Many
//  card-skimmer modules are CLASSIC Bluetooth (BR/EDR) — HC-03/04/05/06/08,
//  CC41, SPP-CA, LINVOR, MLT-BT05 ... — and never appear in a BLE scan.
//
//  This sketch runs on a classic-BT-capable ESP32-WROOM-32 sitting next to
//  the C5. It performs a classic-BT inquiry, matches discovered device names
//  against the same skimmer fingerprint list the C5 uses, and on a hit sends
//  one line over UART to the C5. The C5 fires its existing LED / screen /
//  dashboard alert — the exact same alarm as a BLE hit. The C5 stays the
//  brain; this board is just a "classic-BT nose" beside it.
//
//  Wiring (both boards are 3.3 V logic — NO level shifter):
//      WROOM-32 GPIO17 (TX2) ---> C5 CLASSIC_BT_UART_RX_PIN
//      WROOM-32 GND          ---> C5 GND     (REQUIRED — shared ground)
//      power both from the same 5 V / 3V3 source
//  The C5 only listens, so WROOM RX (GPIO16) can be left unconnected.
//
//  Build: Arduino IDE or arduino-cli, board "ESP32 Dev Module" (WROOM-32).
//  Classic Bluetooth (Bluedroid) is enabled by default on the WROOM-32.
//  Keep SKIMMER_NAMES below in sync with the C5's config.h.
// =====================================================================

#include "BluetoothSerial.h"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error "Classic Bluetooth is not enabled. Use an ESP32-WROOM-32 (Bluedroid) build."
#endif

// ---- UART link to the C5 -------------------------------------------------
// Serial2 on the WROOM-32: TX2 = GPIO17 -> C5 RX. The baud must match
// CLASSIC_BT_UART_BAUD on the C5 (default 115200).
static const int  UART_TX_PIN = 17;   // -> C5 CLASSIC_BT_UART_RX_PIN
static const int  UART_RX_PIN = 16;   // unused by the C5 link; harmless
static const long UART_BAUD   = 115200;
#define LINK Serial2

// ---- Inquiry timing ------------------------------------------------------
// Classic inquiry length per round. ~5 s catches discoverable modules within
// a round; we re-inquire continuously so a matched device keeps refreshing the
// C5's proximity alert (whose hold timer is several seconds).
static const int INQUIRY_SECONDS = 5;

// ---- Skimmer fingerprints (keep in sync with the C5's config.h) ----------
// Only the names that can actually be classic-BT modules need to live here;
// pentest tools (Flipper etc.) advertise on BLE and are handled by the C5.
static const char* SKIMMER_NAMES[] = {
    "HC-03", "HC-04", "HC-05", "HC-06", "HC-08",
    "HC03",  "HC04",  "HC05",  "HC06",  "HC08",
    "BT04", "BT05", "BT06", "BT08",
    "CC41A", "CC41",
    "JDY",
    "JDY-08", "JDY-09", "JDY-16", "JDY-17", "JDY-18",
    "JDY-19", "JDY-23", "JDY-24", "JDY-25",
    "JDY-30", "JDY-31", "JDY-33",
    "HM-10", "HM-11",
    "AT-09",
    "SPP-CA", "LINVOR", "MLT-BT05",
    nullptr
};

BluetoothSerial SerialBT;

// Normalize like the C5: drop space / - / _ / . and uppercase, so "HC-05"
// matches "HC05" and "hc 05".
static String normalizeName(const String& in) {
    String out;
    out.reserve(in.length());
    for (size_t i = 0; i < in.length(); i++) {
        char c = in[i];
        if (c == ' ' || c == '-' || c == '_' || c == '.') continue;
        out += (char)toupper((unsigned char)c);
    }
    return out;
}

// Same matching rule as the C5's runtime_config: exact, prefix, or substring
// on the normalized strings (so "JDY" catches "JDY-23", etc.).
static bool isSkimmerName(const String& name) {
    if (name.length() == 0) return false;
    const String nrm = normalizeName(name);
    for (int i = 0; SKIMMER_NAMES[i] != nullptr; i++) {
        const String cn = normalizeName(SKIMMER_NAMES[i]);
        if (cn.length() == 0) continue;
        if (nrm == cn || nrm.startsWith(cn) || nrm.indexOf(cn) >= 0) return true;
    }
    return false;
}

// Strip characters that would break the line protocol (the C5 splits on '|').
static String sanitize(const String& in) {
    String out;
    out.reserve(in.length());
    for (size_t i = 0; i < in.length(); i++) {
        char c = in[i];
        if (c == '|' || c == '\r' || c == '\n') c = ' ';
        out += c;
    }
    out.trim();
    return out;
}

void setup() {
    Serial.begin(115200);   // USB debug only
    LINK.begin(UART_BAUD, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
    SerialBT.begin("VordBT-Scout", true);   // true = master role (for inquiry)
    Serial.println("[VordBT] classic-BT scout up; inquiring...");
}

void loop() {
    // Blocking classic inquiry with name resolution; returns the devices found
    // this round.
    BTScanResults* results = SerialBT.discover(INQUIRY_SECONDS * 1000);
    if (results) {
        const int n = results->getCount();
        for (int i = 0; i < n; i++) {
            BTAdvertisedDevice* dev = results->getDevice(i);
            if (!dev) continue;

            String name = String(dev->getName().c_str());
            if (!isSkimmerName(name)) continue;

            String mac = String(dev->getAddress().toString().c_str());
            int rssi = dev->getRSSI();
            // Classic-inquiry RSSI is best-effort and often unreported; send a
            // mid-range default so the C5's alert blinks at a sensible rate.
            if (rssi >= 0 || rssi < -120) rssi = -55;

            // Protocol: VBT1|TYPE|RSSI|MAC|NAME
            String line = "VBT1|SKIM|" + String(rssi) + "|" + mac + "|" + sanitize(name);
            LINK.println(line);
            Serial.print("[VordBT] hit -> ");
            Serial.println(line);
        }
    }
    // Short gap, then inquire again. Re-sending matched hits each round keeps
    // the C5's proximity alert (and its hold timer) refreshed while a module is
    // in range.
    delay(200);
}
