// =====================================================================
//  Vord C5 — BLE skimmer detector firmware
//
//  Runtime model:
//  - BLE scan runs continuously (100% duty) in skimmer-focused mode.
//  - A self-hosted WiFi AP serves a live dashboard of all BLE data and
//    skimmer detections (BLE/WiFi run together via chip coexistence).
//  - No wardriving and no serial command/protocol output.
//  - Onboard RGB LED indicates proximity to suspicious devices.
// =====================================================================

#include <Arduino.h>
#include "config.h"
#include "ble_scanner.h"
#include "scan_cycle.h"
#include "runtime_config.h"
#include "skimmer_led.h"
#include "skimmer_display.h"
#include "web_portal.h"
#include "battery.h"

void setup() {
    delay(250);

    Serial.begin(115200);   // USB CDC; prints the AP address/band for diagnostics

    runtime_config_init();
    battery_init();
    ble_scanner_init();

    // WiFi AP + dashboard. Brought up before scanning so the network is
    // available immediately at boot.
    web_portal_init();

    // Local-only alert paths on the device.
    skimmer_led_init();
    skimmer_display_init();   // T-Dongle-C5 screen alert (no-op on boards without a display)
    scan_cycle_init();

    // Continuous BLE scan task. On C5 this runs on the single available core.
    xTaskCreatePinnedToCore(scan_cycle_task, "scan_cycle", CYCLE_TASK_STACK, NULL, 1, NULL, 0);
}

void loop() {
    // Web server now runs in its own FreeRTOS task (web_portal_loop is a no-op).
    // Just yield here to let other tasks run.
    delay(10);
}
