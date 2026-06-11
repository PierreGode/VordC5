#ifndef BLE_SCANNER_H
#define BLE_SCANNER_H

#include <Arduino.h>
#include <vector>

// BLE scan modes
enum BleMode {
    BLE_MODE_OFF,
    BLE_MODE_ALL,
    BLE_MODE_FILTERED,
    BLE_MODE_SKIMMER
};

// One observed BLE device, kept for the web dashboard. Times are millis().
struct BleDeviceRecord {
    String   mac;
    String   name;
    int32_t  rssi;        // most recent RSSI
    bool     isSkimmer;   // matched a skimmer fingerprint at any point
    uint32_t firstSeenMs;
    uint32_t lastSeenMs;
    uint32_t seenCount;   // advertisements observed
};

void ble_scanner_init();
void ble_scanner_start(BleMode mode);
void ble_scanner_stop();
int  ble_scanner_count();
int  ble_scanner_skimmer_count();

// Session totals — unique MACs observed since boot. RAM-only, reset on power cycle.
int  ble_scanner_session_count();
int  ble_scanner_session_skimmer_count();

// Snapshot of the detailed device table for the web UI. Returns a copy taken
// under the internal mutex so callers can iterate without holding the lock.
std::vector<BleDeviceRecord> ble_scanner_snapshot();

#endif // BLE_SCANNER_H
