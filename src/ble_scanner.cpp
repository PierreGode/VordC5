#include "ble_scanner.h"
#include "config.h"
#include "runtime_config.h"
#include "skimmer_led.h"
#include "skimmer_display.h"
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <map>
#include <cstring>
#include <math.h>

static BleMode s_mode = BLE_MODE_OFF;
static int s_bleCount = 0;
static int s_skimmerCount = 0;
static int s_pentoolCount = 0;
static BLEScan* s_pScan = nullptr;

// ---- Session unique-device counters (constant memory) ----
// We count *unique* MACs seen since boot, but this device is meant to run for
// hours in a city and to survive a Flipper cycling through random MACs — both
// produce tens of thousands of distinct addresses. The previous
// std::set<String> grew ~70 bytes per new MAC and would exhaust the heap, after
// which NimBLE can no longer allocate advert buffers and the scan dies for good
// (it never recovers because the set is never freed). HyperLogLog estimates the
// unique count in a fixed ~1 KB no matter how many devices are seen, trading
// exactness (~3% standard error) for a hard memory bound.
class HyperLogLog {
public:
    static constexpr int      P = 10;          // 2^P registers
    static constexpr uint32_t M = 1u << P;     // 1024 registers, 1 byte each

    void add(const char* s) {
        const uint64_t h   = hash64(s);
        const uint32_t idx = (uint32_t)(h >> (64 - P));          // top P bits -> register
        const uint64_t w   = (h << P) | (1ull << (P - 1));       // remaining bits + sentinel
        const uint8_t  rho = (uint8_t)(__builtin_clzll(w) + 1);  // position of leftmost 1-bit
        if (rho > reg[idx]) reg[idx] = rho;
    }

    uint32_t estimate() const {
        double sum = 0.0;
        uint32_t zeros = 0;
        for (uint32_t i = 0; i < M; i++) {
            sum += 1.0 / (double)(1ull << reg[i]);
            if (reg[i] == 0) zeros++;
        }
        const double alpha = 0.7213 / (1.0 + 1.079 / (double)M);
        double e = alpha * (double)M * (double)M / sum;
        if (e <= 2.5 * M && zeros > 0)
            e = (double)M * log((double)M / (double)zeros);      // small-range linear counting
        return (uint32_t)(e + 0.5);
    }

private:
    uint8_t reg[M] = {0};
    static uint64_t hash64(const char* s) {
        uint64_t h = 1469598103934665603ull;                     // FNV-1a 64-bit
        while (*s) { h ^= (uint8_t)*s++; h *= 1099511628211ull; }
        h ^= h >> 33; h *= 0xff51afd7ed558ccdull;                // splitmix64 avalanche
        h ^= h >> 33; h *= 0xc4ceb9fe1a85ec53ull; h ^= h >> 33;  // (spread bits for HLL)
        return h;
    }
};

// Touched by the BLE callback and the web task, so all access goes through
// s_sessionMutex (shared with s_devices, updated in the same callback).
static SemaphoreHandle_t s_sessionMutex = nullptr;
static HyperLogLog s_hllBle;
static HyperLogLog s_hllSkimmer;
static HyperLogLog s_hllPentool;

// ---- Detailed device table for the web dashboard ----
// Keyed by MAC, capped to bound memory. Guarded by s_sessionMutex (same lock as
// the session sets, since both are updated together from the BLE callback).
static std::map<String, BleDeviceRecord> s_devices;

// Detail-table keep priority — higher survives longer when the table is full.
// Flagged discoveries are pinned regardless of age (skimmers above pentools),
// then named devices; anonymous background BLE (no name, not flagged) is shed
// first so the dashboard stays full of meaningful entries instead of noise.
static inline int detailKeepRank(bool skimmer, bool pentool, bool named) {
    if (skimmer) return 3;
    if (pentool) return 2;
    return named ? 1 : 0;
}

// ---- Scan callbacks ----
class ScanCallbacks : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) override {
        String mac   = String(advertisedDevice.getAddress().toString().c_str());
        String name  = String(advertisedDevice.getName().c_str());
        int rssi     = advertisedDevice.getRSSI();
        // Pentest tools (Flipper Zero & friends) are classified first so a name
        // matching both lists shows as PENTOOL, not SKIMMER.
        bool pentool = isPentoolName(name);
        bool skimmer = !pentool && isSkimmerName(name);

        // Session totals + detailed device table — all under one lock.
        if (s_sessionMutex && xSemaphoreTake(s_sessionMutex, portMAX_DELAY) == pdTRUE) {
            s_hllBle.add(mac.c_str());
            if (skimmer) s_hllSkimmer.add(mac.c_str());
            if (pentool) s_hllPentool.add(mac.c_str());

            const uint32_t now = millis();
            auto it = s_devices.find(mac);
            if (it != s_devices.end()) {
                BleDeviceRecord& r = it->second;
                r.rssi = rssi;
                r.lastSeenMs = now;
                r.seenCount++;
                if (name.length()) r.name = name;     // keep the best name we've seen
                if (skimmer) r.isSkimmer = true;       // sticky once flagged
                if (pentool) r.isPentool = true;       // sticky once flagged
            } else {
                // New device. When the table is full, shed the lowest-priority
                // entry (anonymous background BLE first, then least-recently
                // seen) so flagged discoveries and named devices stay on the
                // list regardless of age. If the newcomer itself is the
                // lowest-priority candidate, drop it rather than bump something
                // more useful off the list — this is how "several unnamed BLE"
                // get kept out to make room for the detections that matter.
                bool insert = true;
                if (s_devices.size() >= BLE_DETAIL_CAP) {
                    auto victim = s_devices.end();
                    int victimRank = 99;
                    uint32_t victimSeen = 0;
                    for (auto i = s_devices.begin(); i != s_devices.end(); ++i) {
                        const int ri = detailKeepRank(i->second.isSkimmer,
                                                      i->second.isPentool,
                                                      i->second.name.length() > 0);
                        if (victim == s_devices.end() || ri < victimRank ||
                            (ri == victimRank && i->second.lastSeenMs < victimSeen)) {
                            victim = i; victimRank = ri; victimSeen = i->second.lastSeenMs;
                        }
                    }
                    const int newRank = detailKeepRank(skimmer, pentool, name.length() > 0);
                    if (victimRank <= newRank) s_devices.erase(victim);
                    else                       insert = false;
                }
                if (insert) {
                    BleDeviceRecord r;
                    r.mac = mac;
                    r.name = name;
                    r.rssi = rssi;
                    r.isSkimmer = skimmer;
                    r.isPentool = pentool;
                    r.firstSeenMs = now;
                    r.lastSeenMs = now;
                    r.seenCount = 1;
                    s_devices.emplace(mac, r);
                }
            }
            xSemaphoreGive(s_sessionMutex);
        }
        s_bleCount++;

        if (s_mode != BLE_MODE_SKIMMER || !(skimmer || pentool)) return;

        // Both categories trigger the proximity alert (LED + display border) —
        // a Flipper nearby is still alert-worthy, it just isn't a skimmer.
        if (skimmer) s_skimmerCount++;
        if (pentool) s_pentoolCount++;
        const SkimmerLedAlert type = skimmer ? SKIMMER_LED_SKIMMER : SKIMMER_LED_PENTOOL;
        skimmer_led_notify(rssi, type);
        skimmer_display_notify(rssi, type);
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
    s_pentoolCount = 0;
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

int ble_scanner_pentool_count() {
    return s_pentoolCount;
}

// Snapshot the estimator under the lock (a cheap ~1 KB copy), then compute the
// cardinality estimate outside the lock so the BLE callback isn't blocked while
// we iterate the registers.
static uint32_t hllCount(const HyperLogLog& hll) {
    if (!s_sessionMutex) return 0;
    HyperLogLog snap;
    if (xSemaphoreTake(s_sessionMutex, portMAX_DELAY) == pdTRUE) {
        snap = hll;
        xSemaphoreGive(s_sessionMutex);
    }
    return snap.estimate();
}

int ble_scanner_session_count()         { return (int)hllCount(s_hllBle); }
int ble_scanner_session_skimmer_count() { return (int)hllCount(s_hllSkimmer); }
int ble_scanner_session_pentool_count() { return (int)hllCount(s_hllPentool); }

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
