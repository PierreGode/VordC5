#include "ble_scanner.h"
#include "config.h"
#include "runtime_config.h"
#include "skimmer_led.h"
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <set>
#include <map>

static BleMode s_mode = BLE_MODE_OFF;
static int s_bleCount = 0;
static int s_skimmerCount = 0;
static BLEScan* s_pScan = nullptr;

// ---- Session totals — unique MACs observed since boot ----
// Touched by the BLE callback (BLE task) and by the display task on the
// other core, so all access goes through s_sessionMutex.
#define BLE_SESSION_TRACK_CAP 4096
static SemaphoreHandle_t s_sessionMutex = nullptr;
static std::set<String> s_sessionBleMacs;
static std::set<String> s_sessionSkimmerMacs;

// ---- Detailed device table for the web dashboard ----
// Keyed by MAC, capped to bound memory. Guarded by s_sessionMutex (same lock as
// the session sets, since both are updated together from the BLE callback).
static std::map<String, BleDeviceRecord> s_devices;

// ---- Scan callbacks ----
class ScanCallbacks : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) override {
        String mac   = String(advertisedDevice.getAddress().toString().c_str());
        String name  = String(advertisedDevice.getName().c_str());
        int rssi     = advertisedDevice.getRSSI();
        bool skimmer = isSkimmerName(name);

        // Session totals + detailed device table — all under one lock.
        if (s_sessionMutex && xSemaphoreTake(s_sessionMutex, portMAX_DELAY) == pdTRUE) {
            if (s_sessionBleMacs.size() < BLE_SESSION_TRACK_CAP) s_sessionBleMacs.insert(mac);
            if (skimmer) s_sessionSkimmerMacs.insert(mac);

            const uint32_t now = millis();
            auto it = s_devices.find(mac);
            if (it != s_devices.end()) {
                BleDeviceRecord& r = it->second;
                r.rssi = rssi;
                r.lastSeenMs = now;
                r.seenCount++;
                if (name.length()) r.name = name;     // keep the best name we've seen
                if (skimmer) r.isSkimmer = true;       // sticky once flagged
            } else if (s_devices.size() < BLE_DETAIL_CAP) {
                BleDeviceRecord r;
                r.mac = mac;
                r.name = name;
                r.rssi = rssi;
                r.isSkimmer = skimmer;
                r.firstSeenMs = now;
                r.lastSeenMs = now;
                r.seenCount = 1;
                s_devices.emplace(mac, r);
            }
            xSemaphoreGive(s_sessionMutex);
        }
        s_bleCount++;

        if (s_mode != BLE_MODE_SKIMMER || !skimmer) return;

        s_skimmerCount++;
        skimmer_led_notify(rssi, SKIMMER_LED_SKIMMER);
    }
};

static ScanCallbacks s_callbacks;

void ble_scanner_init() {
    if (!s_sessionMutex) s_sessionMutex = xSemaphoreCreateMutex();
    BLEDevice::init("Vord-C5");
    s_pScan = BLEDevice::getScan();
    s_pScan->setAdvertisedDeviceCallbacks(&s_callbacks, true);
    s_pScan->setActiveScan(true);
    // BLE and WiFi share the C5's single radio. The scan window must leave
    // airtime for AP beacons and HTTP, or clients disconnect and the
    // dashboard never loads. ~30% BLE duty still catches advertisers
    // (typical adv interval 20-100 ms) within a second or two.
    s_pScan->setInterval(BLE_SCAN_INTERVAL_MS);
    s_pScan->setWindow(BLE_SCAN_WINDOW_MS);
}

void ble_scanner_start(BleMode mode) {
    if (s_mode != BLE_MODE_OFF) ble_scanner_stop();
    s_mode = (mode == BLE_MODE_OFF) ? BLE_MODE_OFF : BLE_MODE_SKIMMER;
    s_bleCount = 0;
    s_skimmerCount = 0;
    s_pScan->clearResults();
    // Non-blocking: pass a no-op callback so start() returns immediately.
    // Duration 0 = indefinite; scan_cycle stops it explicitly via ble_scanner_stop().
    s_pScan->start(0, [](BLEScanResults){}, false);
}

void ble_scanner_stop() {
    if (s_pScan && s_mode != BLE_MODE_OFF) {
        s_pScan->stop();
    }
    s_mode = BLE_MODE_OFF;
}

void ble_scanner_refresh() {
    // The C5 uses NimBLE, which keeps one heap-allocated record per unique
    // advertiser MAC for the life of a single start() and never frees them
    // (its m_maxResults defaults to "store all"). A Flipper running BLE-spam
    // cycles through thousands of random MACs, so an indefinite scan exhausts
    // the heap within seconds — the scan then stops delivering callbacks and
    // the proximity LED goes quiet. Stop → clear → restart frees that vector
    // while leaving our own counters and device table intact (we deliberately
    // do NOT route through ble_scanner_start(), which would zero the counters).
    if (!s_pScan || s_mode == BLE_MODE_OFF) return;
    s_pScan->stop();
    s_pScan->clearResults();
    s_pScan->start(0, [](BLEScanResults){}, false);
}

int ble_scanner_count() {
    return s_bleCount;
}

int ble_scanner_skimmer_count() {
    return s_skimmerCount;
}

static int sessionSetSize(const std::set<String>& s) {
    if (!s_sessionMutex) return 0;
    int n = 0;
    if (xSemaphoreTake(s_sessionMutex, portMAX_DELAY) == pdTRUE) {
        n = (int)s.size();
        xSemaphoreGive(s_sessionMutex);
    }
    return n;
}

int ble_scanner_session_count()         { return sessionSetSize(s_sessionBleMacs); }
int ble_scanner_session_skimmer_count() { return sessionSetSize(s_sessionSkimmerMacs); }

std::vector<BleDeviceRecord> ble_scanner_snapshot() {
    std::vector<BleDeviceRecord> out;
    if (!s_sessionMutex) return out;
    if (xSemaphoreTake(s_sessionMutex, portMAX_DELAY) == pdTRUE) {
        out.reserve(s_devices.size());
        for (const auto& kv : s_devices) out.push_back(kv.second);
        xSemaphoreGive(s_sessionMutex);
    }
    return out;
}
