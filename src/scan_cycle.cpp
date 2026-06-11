#include "scan_cycle.h"
#include "ble_scanner.h"
#include "config.h"

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

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
