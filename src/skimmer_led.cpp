#include "skimmer_led.h"

#if VORD_HAS_SKIMMER_LED

// Shared between the BLE task (writer, via skimmer_led_notify) and the LED
// task (reader). All three are 32-bit aligned scalars, so plain volatile
// access is atomic on the ESP32 — no tearing, no mutex needed. A benign race
// where the reader pairs a fresh timestamp with a stale RSSI/type only nudges
// the blink for one cycle, which is harmless.
static volatile int32_t  s_lastRssi   = -100;
static volatile uint32_t s_lastSeenMs = 0;
static volatile int32_t  s_lastType   = SKIMMER_LED_SKIMMER;
static bool s_started = false;

// One-shot confirmation flash request (mode-switch feedback). The button task
// writes these; the LED task plays and clears the pending count.
static volatile uint8_t  s_flashR = 0, s_flashG = 0, s_flashB = 0;
static volatile int32_t  s_flashPending = 0;

// Map an RSSI to a blink half-period (ms): closer (stronger signal, i.e. RSSI
// nearer 0) blinks faster. RSSI is clamped to the configured NEAR..FAR window,
// then linearly interpolated across FAST..SLOW.
static uint32_t rssiToHalfPeriod(int rssi) {
    if (rssi > SKIMMER_LED_RSSI_NEAR) rssi = SKIMMER_LED_RSSI_NEAR;
    if (rssi < SKIMMER_LED_RSSI_FAR)  rssi = SKIMMER_LED_RSSI_FAR;

    const long span = (long)SKIMMER_LED_RSSI_NEAR - (long)SKIMMER_LED_RSSI_FAR; // > 0
    const long pos  = (long)rssi - (long)SKIMMER_LED_RSSI_FAR;                  // 0..span
    return (uint32_t)((long)SKIMMER_LED_SLOW_MS -
        (pos * ((long)SKIMMER_LED_SLOW_MS - (long)SKIMMER_LED_FAST_MS)) / span);
}

// Single write path so the optional red/green channel swap (for boards wired
// that way — see SKIMMER_LED_SWAP_RG) applies to every color we emit.
static inline void ledWrite(uint8_t r, uint8_t g, uint8_t b) {
#if SKIMMER_LED_SWAP_RG
    rgbLedWrite(SKIMMER_LED_PIN, g, r, b);
#else
    rgbLedWrite(SKIMMER_LED_PIN, r, g, b);
#endif
}

static inline void ledOff() {
    ledWrite(0, 0, 0);
}

// Proximity color for the non-white blink phase. Far away (weak RSSI) → blue;
// as RSSI rises (getting closer) the color fades yellow → orange → red. The
// thresholds live in config.h (SKIMMER_LED_RSSI_YELLOW / _RED).
static void proximityColor(int rssi, uint8_t& r, uint8_t& g, uint8_t& b) {
    const uint8_t B = SKIMMER_LED_BRIGHTNESS;
    if (rssi < SKIMMER_LED_RSSI_YELLOW) {            // far → solid blue
        r = 0; g = 0; b = B;
        return;
    }
    // From the yellow threshold up to the red threshold: full red, with the
    // green channel fading from full (yellow) down to none (red) as we approach.
    const int hi = SKIMMER_LED_RSSI_RED;             // at/above → pure red
    const int lo = SKIMMER_LED_RSSI_YELLOW;          // at → yellow
    if (rssi > hi) rssi = hi;
    long green = (long)B * (long)(hi - rssi) / (long)(hi - lo);
    if (green < 0) green = 0;
    if (green > B) green = B;
    r = B; g = (uint8_t)green; b = 0;
}

// Show one phase of the alternating blink: white on the white phase, otherwise
// the distance-coded proximity color (blue → yellow → orange → red).
static void ledPhase(bool whitePhase, int rssi) {
    if (whitePhase) {
        const uint8_t b = SKIMMER_LED_BRIGHTNESS;
        ledWrite(b, b, b);                           // white
    } else {
        uint8_t r, g, b;
        proximityColor(rssi, r, g, b);
        ledWrite(r, g, b);
    }
}

static void skimmer_led_task(void*) {
    bool whitePhase = false;
    ledOff();

    for (;;) {
        // Pending confirmation flash takes priority and plays synchronously.
        if (s_flashPending > 0) {
            const int     n = s_flashPending;
            const uint8_t r = s_flashR, g = s_flashG, b = s_flashB;
            s_flashPending = 0;
            for (int i = 0; i < n; i++) {
                ledWrite(r, g, b);
                vTaskDelay(pdMS_TO_TICKS(150));
                ledWrite(0, 0, 0);
                vTaskDelay(pdMS_TO_TICKS(150));
            }
            whitePhase = false;
            continue;
        }

        const uint32_t now = millis();
        const uint32_t age = now - (uint32_t)s_lastSeenMs;

        if (s_lastSeenMs != 0 && age <= SKIMMER_LED_HOLD_MS) {
            // A flagged device is (recently) in range — alternate the two
            // colors at the proximity rate.
            whitePhase = !whitePhase;
            ledPhase(whitePhase, (int)s_lastRssi);
            vTaskDelay(pdMS_TO_TICKS(rssiToHalfPeriod((int)s_lastRssi)));
        } else {
            // Nothing nearby — make sure the LED is off and idle-poll.
            ledOff();
            whitePhase = false;
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

void skimmer_led_init() {
    if (s_started) return;
    s_started = true;
    ledOff();
    // Pinned to core 0 so it coexists with the scan cycle on single-core C5.
    xTaskCreatePinnedToCore(skimmer_led_task, "skimmer_led",
                            SKIMMER_LED_TASK_STACK, NULL, 1, NULL, 0);
}

void skimmer_led_notify(int rssi, SkimmerLedAlert type) {
    s_lastRssi   = rssi;
    s_lastType   = (int32_t)type;
    s_lastSeenMs = millis();
}

void skimmer_led_flash(uint8_t r, uint8_t g, uint8_t b, int count) {
    s_flashR = r;
    s_flashG = g;
    s_flashB = b;
    s_flashPending = count;
}

#endif // VORD_HAS_SKIMMER_LED
