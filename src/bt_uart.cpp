#include "bt_uart.h"

#if VORD_HAS_CLASSIC_BT_UART

#include "ble_scanner.h"
#include "skimmer_led.h"
#include "skimmer_display.h"

// Dedicated hardware UART (Serial1 by default; Serial0 is the USB-CDC console).
static HardwareSerial s_link(CLASSIC_BT_UART_NUM);

// millis() of the last well-formed VBT1 line (hit or heartbeat). 0 = never heard.
// Written from the UART task, read from the web task; a 32-bit aligned scalar so
// the read/write is atomic on this MCU — volatile is enough, no mutex needed.
static volatile uint32_t s_lastScoutMs = 0;

uint32_t bt_uart_scout_last_seen_ms() { return s_lastScoutMs; }

// Parse one received line "VBT1|TYPE|RSSI|MAC|NAME" and fire the alert. Lines
// that don't start with the version tag are ignored, so the WROOM-32's USB-debug
// chatter or line noise can't trip the alarm.
static void handleLine(const String& line) {
    if (!line.startsWith("VBT1|")) return;

    // NAME is the last field and may contain spaces but never '|' (the sender
    // sanitizes), so a fixed 4-delimiter split is safe.
    const int p1 = line.indexOf('|', 0);
    const int p2 = line.indexOf('|', p1 + 1);
    const int p3 = line.indexOf('|', p2 + 1);
    const int p4 = line.indexOf('|', p3 + 1);
    if (p1 < 0 || p2 < 0 || p3 < 0 || p4 < 0) return;

    // A well-formed VBT1 line — whether a hit or a bare heartbeat — proves the
    // scout link is alive. Stamp liveness before the type check. millis() can be
    // 0 only in the first ms after boot; bump to 1 so 0 always means "never".
    uint32_t nowMs = millis();
    s_lastScoutMs = nowMs ? nowMs : 1;

    const String type = line.substring(p1 + 1, p2);
    int          rssi = line.substring(p2 + 1, p3).toInt();
    const String mac  = line.substring(p3 + 1, p4);
    String       name = line.substring(p4 + 1);
    name.trim();

    // PING is a liveness-only heartbeat (already stamped above) — nothing to alert on.
    if (type == "PING") return;

    const bool skimmer = (type == "SKIM");
    const bool pentool = (type == "PENT");
    if (!skimmer && !pentool) return;
    if (mac.length() == 0) return;
    // Classic-inquiry RSSI is best-effort; fall back to a mid-range value so the
    // proximity alert still blinks at a sensible rate.
    if (rssi >= 0 || rssi < -120) rssi = -60;

    const SkimmerLedAlert alert = skimmer ? SKIMMER_LED_SKIMMER : SKIMMER_LED_PENTOOL;
    skimmer_led_notify(rssi, alert);
    skimmer_display_notify(rssi, alert);
    ble_scanner_register_external(mac, name, rssi, skimmer, pentool);

    Serial.printf("[bt_uart] %s %s rssi=%d \"%s\"\n",
                  type.c_str(), mac.c_str(), rssi, name.c_str());
}

static void btUartTask(void* param) {
    (void)param;
    String line;
    for (;;) {
        while (s_link.available()) {
            const char c = (char)s_link.read();
            if (c == '\n' || c == '\r') {
                if (line.length()) {
                    // Relay the raw scout line to the USB-CDC console, tagged
                    // [wroom], so the web serial terminal can show ALL WROOM-32
                    // traffic coming through the C5 — hits and heartbeats alike —
                    // and its source filter can separate it from the C5's own log.
                    Serial.print("[wroom] ");
                    Serial.println(line);
                    handleLine(line);
                    line = "";
                }
            } else if (line.length() < 160) {
                line += c;
            } else {
                line = "";   // overlong line: drop and resync on the next newline
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void bt_uart_init() {
    static bool started = false;
    if (started) return;
    started = true;

    s_link.begin(CLASSIC_BT_UART_BAUD, SERIAL_8N1,
                 CLASSIC_BT_UART_RX_PIN, CLASSIC_BT_UART_TX_PIN);
    xTaskCreatePinnedToCore(btUartTask, "bt_uart", BT_UART_TASK_STACK, NULL, 1, NULL, 0);
    Serial.printf("[bt_uart] classic-BT sidecar listening on UART%d RX=%d @ %d baud\n",
                  CLASSIC_BT_UART_NUM, CLASSIC_BT_UART_RX_PIN, CLASSIC_BT_UART_BAUD);
}

#endif // VORD_HAS_CLASSIC_BT_UART
