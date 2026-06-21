#include "scan_cycle.h"
#include "ble_scanner.h"
#include "config.h"
#include "battery.h"
#include "skimmer_led.h"

static volatile bool s_running = true;

void scan_cycle_init() {
    s_running = true;
}

void scan_cycle_pause() {
    s_running = false;
}

void scan_cycle_resume() {
    s_running = true;
}

bool scan_cycle_is_running() {
    return s_running;
}

void scan_cycle_task(void* param) {
    (void)param;
    bool scanning = false;
    uint32_t lastRefresh = 0;
    uint32_t lastHeapLog = 0;
    uint32_t lastBattWarn = 0;

    for (;;) {
        if (!s_running) {
            if (scanning) {
                ble_scanner_stop();
                scanning = false;
            }
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        if (!scanning) {
            ble_scanner_start(BLE_MODE_SKIMMER);
            scanning = true;
            lastRefresh = millis();
        }

        // Periodically restart the scan so the BLE stack's internal advertiser
        // list can't grow without bound. Without this, a Flipper cycling random
        // MACs (BLE-spam) exhausts the heap within seconds and the scan — and the
        // proximity LED it feeds — goes dead. See ble_scanner_refresh().
        if (millis() - lastRefresh >= BLE_SCAN_REFRESH_MS) {
            ble_scanner_refresh();
            lastRefresh = millis();
        }

        // Heap health log. The scan must survive hours of unique MACs (a city, or
        // a Flipper spamming random addresses); free heap should settle and stay
        // flat. A steady downward trend here means something is still leaking.
        if (millis() - lastHeapLog >= 10000) {
            Serial.printf("[heap] free=%u  minFree=%u\n",
                          (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMinFreeHeap());
            lastHeapLog = millis();
        }

        // Low-battery warning: a short LED blink every BATT_WARN_INTERVAL_MS once
        // the pack drops to/below BATT_LOW_PCT, so a field user notices without the
        // dashboard. A reading below BATT_PRESENT_MV means monitoring is off or no
        // battery is fitted (the divider reads ~0 mV), so a USB-only unit stays
        // silent; skimmer_led_flash() is also a no-op when the LED is compiled out.
        if (millis() - lastBattWarn >= BATT_WARN_INTERVAL_MS) {
            const int mv = battery_millivolts();
            if (mv >= BATT_PRESENT_MV && battery_percent(mv) <= BATT_LOW_PCT) {
                skimmer_led_flash(40, 0, 40, 2);   // dim magenta x2 — distinct from proximity colors
            }
            lastBattWarn = millis();
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
