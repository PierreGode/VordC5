#include "battery.h"
#include "config.h"

#include <Arduino.h>

#if VBAT_ADC_PIN >= 0

static int      s_cachedMv   = -1;
static uint32_t s_lastReadMs = 0;

// Read the divider tap and scale back up to battery volts. The ESP32 core's
// analogReadMilliVolts() applies the factory ADC calibration, so no manual
// attenuation/reference math is needed.
static int readBatteryMv() {
#if VBAT_EN_PIN >= 0
    digitalWrite(VBAT_EN_PIN, HIGH);
    delay(20);  // let the divider's 100nF filter caps settle through ~50K
#endif
    uint32_t sum = 0;
    for (int i = 0; i < 8; i++) sum += analogReadMilliVolts(VBAT_ADC_PIN);
#if VBAT_EN_PIN >= 0
    digitalWrite(VBAT_EN_PIN, LOW);  // gate off to stop divider drain
#endif
    return (int)((sum / 8) * VBAT_DIVIDER + 0.5f);
}

void battery_init() {
#if VBAT_EN_PIN >= 0
    pinMode(VBAT_EN_PIN, OUTPUT);
    digitalWrite(VBAT_EN_PIN, LOW);
#endif
}

int battery_millivolts() {
    const uint32_t now = millis();
    if (s_cachedMv < 0 || now - s_lastReadMs >= VBAT_REFRESH_MS) {
        s_cachedMv = readBatteryMv();
        s_lastReadMs = now;
    }
    return s_cachedMv;
}

#else  // VBAT_ADC_PIN < 0: monitoring disabled

void battery_init() {}
int battery_millivolts() { return 0; }

#endif

// Resting-voltage discharge curve for a 1S LiPo, linearly interpolated.
// Voltage under load / while charging skews high; treat the % as approximate.
//
// 100% is anchored at 4.10 V, not the textbook 4.20 V, because the on-board
// charger tops the cell out at ~4.10 V (measured) — charging to 4.1 V instead
// of 4.2 V is gentler on the cell and is this board's actual "full". The lower
// points are rescaled to that ceiling so a fully-charged pack reads ~100%
// instead of ~90%. Any cell that does reach 4.2 V still clamps to 100%.
int battery_percent(int mv) {
    static const struct { int mv; int pct; } curve[] = {
        {4100, 100}, {4000, 87}, {3900, 69}, {3800, 50},
        {3700, 28},  {3600, 11}, {3500, 4},  {3300, 0},
    };
    const size_t n = sizeof(curve) / sizeof(curve[0]);
    if (mv >= curve[0].mv) return 100;
    for (size_t i = 1; i < n; i++) {
        if (mv >= curve[i].mv) {
            const int span = curve[i - 1].mv - curve[i].mv;
            return curve[i].pct + (curve[i - 1].pct - curve[i].pct) * (mv - curve[i].mv) / span;
        }
    }
    return 0;
}
