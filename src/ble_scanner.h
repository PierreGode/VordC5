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
    bool     isPentool;   // matched a pentest-tool fingerprint (Flipper etc.)
    bool     external;    // sighting arrived via the classic-BT UART scout, not this chip's BLE scan
    uint32_t firstSeenMs;
    uint32_t lastSeenMs;
    uint32_t seenCount;   // advertisements observed
};

void ble_scanner_init();
void ble_scanner_start(BleMode mode);
void ble_scanner_stop();

// Restart the in-progress scan to flush the BLE stack's internal advertiser
// list (which an indefinite scan never frees). Required to survive a flood of
// unique MAC addresses, e.g. a Flipper running BLE-spam. Preserves all
// app-level counters and the device table; safe no-op if not scanning.
void ble_scanner_refresh();
int  ble_scanner_count();
int  ble_scanner_skimmer_count();
int  ble_scanner_pentool_count();

// Session totals — unique MACs observed since boot. RAM-only, reset on power cycle.
int  ble_scanner_session_count();
int  ble_scanner_session_skimmer_count();
int  ble_scanner_session_pentool_count();

// Register a sighting that did NOT come from this chip's BLE scan. Used by the
// classic-Bluetooth (BR/EDR) UART sidecar: a WROOM-32 alongside the C5 performs
// a classic inquiry, matches names, and forwards hits over UART. They land in
// the same session counters and device table as native BLE hits, so the
// dashboard shows classic-BT skimmer modules too. Thread-safe.
void ble_scanner_register_external(const String& mac, const String& name,
                                   int rssi, bool skimmer, bool pentool);

// Snapshot of the detailed device table for the web UI. Returns a copy taken
// under the internal mutex so callers can iterate without holding the lock.
std::vector<BleDeviceRecord> ble_scanner_snapshot();

#endif // BLE_SCANNER_H
