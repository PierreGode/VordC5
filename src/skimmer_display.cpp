#include "skimmer_display.h"

#if VORD_HAS_DISPLAY

#include "st7735.h"

// Shared between the BLE task (writer, via skimmer_display_notify) and the
// display task (reader). Both are 32-bit aligned scalars, so plain volatile
// access is atomic on the ESP32 — no mutex needed, same as skimmer_led.
static volatile int32_t  s_lastRssi   = -100;
static volatile uint32_t s_lastSeenMs = 0;
static bool s_started = false;

static Adafruit_ST7735 s_tft(DISPLAY_PIN_CS, DISPLAY_PIN_DC, DISPLAY_PIN_RST,
                             DISPLAY_PIN_SCK, DISPLAY_PIN_MOSI);

// Convert a logical RGB colour to the 16-bit value this panel expects. The
// ST7735 here runs with display inversion (INVON) and BGR sub-pixel order, so a
// plain RGB565 word lights up the wrong colour. This bakes both transforms in:
// invert each channel, then swap red/blue. Verified against the vendor LCD demo
// (its 0x001F "green" fill maps back through this to logical blue, as expected).
static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    const uint8_t r5 = r >> 3, g6 = g >> 2, b5 = b >> 3;
    const uint8_t sr = 31 - b5, sg = 63 - g6, sb = 31 - r5;   // invert + R/B swap
    return (uint16_t)((sr << 11) | (sg << 5) | sb);
}

// Map an RSSI to the flash half-period (ms): closer (stronger signal, RSSI
// nearer 0) flashes faster. RSSI is clamped to the NEAR..FAR window then
// linearly interpolated across FAST..SLOW. Independent of the LED's mapping so
// the display works even when the LED feature is compiled out.
static uint32_t rssiToHalfPeriod(int rssi) {
    if (rssi > DISPLAY_RSSI_NEAR) rssi = DISPLAY_RSSI_NEAR;
    if (rssi < DISPLAY_RSSI_FAR)  rssi = DISPLAY_RSSI_FAR;

    const long span = (long)DISPLAY_RSSI_NEAR - (long)DISPLAY_RSSI_FAR;   // > 0
    const long pos  = (long)rssi - (long)DISPLAY_RSSI_FAR;                // 0..span
    return (uint32_t)((long)DISPLAY_FLASH_SLOW_MS -
        (pos * ((long)DISPLAY_FLASH_SLOW_MS - (long)DISPLAY_FLASH_FAST_MS)) / span);
}

static void skimmer_display_task(void*) {
    const uint16_t RED  = rgb565(255, 0, 0);
    const uint16_t OFF  = rgb565(0, 0, 0);     // black between red flashes
    const uint16_t IDLE = rgb565(0, 40, 0);    // dim green: scanning / all clear

    bool on = false;
    bool idleShown = false;
    s_tft.fillScreen(IDLE);

    for (;;) {
        const uint32_t now = millis();
        const uint32_t age = now - (uint32_t)s_lastSeenMs;

        if (s_lastSeenMs != 0 && age <= DISPLAY_HOLD_MS) {
            // A flagged device is (recently) in range — flash the whole screen
            // red, faster the closer it is.
            idleShown = false;
            on = !on;
            s_tft.fillScreen(on ? RED : OFF);
            vTaskDelay(pdMS_TO_TICKS(rssiToHalfPeriod((int)s_lastRssi)));
        } else {
            // Nothing nearby — show the calm idle screen once and idle-poll.
            if (!idleShown) {
                s_tft.fillScreen(IDLE);
                idleShown = true;
                on = false;
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

void skimmer_display_init() {
    if (s_started) return;
    s_started = true;

    s_tft.begin();
    s_tft.setRotation(DISPLAY_ROTATION);

    pinMode(DISPLAY_PIN_BL, OUTPUT);
    digitalWrite(DISPLAY_PIN_BL, DISPLAY_BL_ON_LEVEL);   // backlight on

    // Pinned to core 0 so it coexists with the scan cycle on single-core C5.
    xTaskCreatePinnedToCore(skimmer_display_task, "skimmer_display",
                            DISPLAY_TASK_STACK, NULL, 1, NULL, 0);
}

void skimmer_display_notify(int rssi, SkimmerLedAlert type) {
    (void)type;
    s_lastRssi   = rssi;
    s_lastSeenMs = millis();
}

#endif // VORD_HAS_DISPLAY
