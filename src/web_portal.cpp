#include "web_portal.h"
#include "config.h"
#include "ble_scanner.h"
#include "scan_cycle.h"
#include "runtime_config.h"

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>

static WebServer s_server(80);
static DNSServer s_dns;
static const byte DNS_PORT = 53;

// ---- JSON helpers ---------------------------------------------------------

// Append `s` to `out` as a quoted, escaped JSON string.
static void appendJsonString(String& out, const String& s) {
    out += '"';
    for (size_t i = 0; i < s.length(); i++) {
        char c = s[i];
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if ((uint8_t)c < 0x20) {
                    char buf[7];
                    snprintf(buf, sizeof(buf), "\\u%04x", (uint8_t)c);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    out += '"';
}

// ---- API endpoints --------------------------------------------------------

static void handleApiStatus() {
    String json;
    json.reserve(256);
    json += "{";
    json += "\"version\":";       appendJsonString(json, VORD_FW_VERSION);
    json += ",\"uptimeMs\":";     json += String(millis());
    json += ",\"scanning\":";     json += scan_cycle_is_running() ? "true" : "false";
    json += ",\"liveBle\":";      json += String(ble_scanner_count());
    json += ",\"liveSkimmer\":";  json += String(ble_scanner_skimmer_count());
    json += ",\"sessionBle\":";   json += String(ble_scanner_session_count());
    json += ",\"sessionSkimmer\":"; json += String(ble_scanner_session_skimmer_count());
    json += ",\"skimmerNames\":"; appendJsonString(json, getSkimmerNamesCsv());
    json += "}";
    s_server.send(200, "application/json", json);
}

static void handleApiDevices() {
    std::vector<BleDeviceRecord> devices = ble_scanner_snapshot();

    String json;
    json.reserve(128 + devices.size() * 96);
    json += "{\"nowMs\":";
    json += String(millis());
    json += ",\"count\":";
    json += String((int)devices.size());
    json += ",\"devices\":[";
    for (size_t i = 0; i < devices.size(); i++) {
        const BleDeviceRecord& d = devices[i];
        if (i) json += ',';
        json += "{\"mac\":";   appendJsonString(json, d.mac);
        json += ",\"name\":";  appendJsonString(json, d.name);
        json += ",\"rssi\":";  json += String(d.rssi);
        json += ",\"skimmer\":"; json += d.isSkimmer ? "true" : "false";
        json += ",\"seen\":";  json += String(d.seenCount);
        json += ",\"firstMs\":"; json += String(d.firstSeenMs);
        json += ",\"lastMs\":";  json += String(d.lastSeenMs);
        json += '}';
    }
    json += "]}";
    s_server.send(200, "application/json", json);
}

// ---- Dashboard page -------------------------------------------------------

static const char INDEX_HTML[] PROGMEM = R"HTML(<!DOCTYPE html>
<html>
<head>
<meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1">
<title>Vord C5</title>
<style>
body{margin:0;padding:20px;font-family:sans-serif;background:#0d1117;color:#e6edf3}
h1{margin:0 0 20px 0}
.stat{display:inline-block;padding:10px 15px;margin:5px;background:#161b22;border-radius:6px}
.stat .n{font-size:24px;font-weight:bold}
table{width:100%;border-collapse:collapse;margin-top:20px}
th,td{text-align:left;padding:8px;border-bottom:1px solid #30363d}
th{background:#161b22;font-weight:bold}
tr.skim{background:#5a1a1a33}
</style>
</head>
<body>
<h1>Vord C5</h1>
<div class=stat>Live BLE: <span class=n id=lBle>-</span></div>
<div class=stat>Skimmers: <span class=n id=lSkim>-</span></div>
<table><thead><tr><th>Name</th><th>MAC</th><th>RSSI</th><th>Type</th></tr></thead>
<tbody id=rows></tbody></table>
<script>
async function tick(){
  try{
    const st=await fetch('/api/status').then(r=>r.json());
    const dv=await fetch('/api/devices').then(r=>r.json());
    document.getElementById('lBle').textContent=st.liveBle;
    document.getElementById('lSkim').textContent=st.liveSkimmer;
    const html=dv.devices.map(d=>`<tr class="${d.skimmer?'skim':''}">
      <td>${d.name||'(unnamed)'}</td><td>${d.mac}</td><td>${d.rssi}</td>
      <td>${d.skimmer?'SKIMMER':'BLE'}</td></tr>`).join('');
    document.getElementById('rows').innerHTML=html||'<tr><td colspan=4>No devices</td></tr>';
  }catch(e){console.error(e)}
}
tick();setInterval(tick,2000);
</script>
</body>
</html>)HTML";


static void handleRoot() {
    s_server.send_P(200, "text/html", INDEX_HTML);
}

// Captive-portal: most OS connectivity checks hit an unknown path; serving the
// dashboard (200) prompts the "sign in to network" sheet to open it.
static void handleNotFound() {
    handleRoot();
}

// ---- Web server task (runs independently from main loop) ----

static void web_portal_task(void*) {
    for (;;) {
        s_dns.processNextRequest();
        s_server.handleClient();
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

// ---- Lifecycle ------------------------------------------------------------

void web_portal_init() {
    const char* pw = (sizeof(AP_PASSWORD) > 1) ? AP_PASSWORD : nullptr; // "" => open AP

    WiFi.mode(WIFI_AP);
    // Disable WiFi power save for stable AP/BLE coexistence on single-core C5
    WiFi.setSleep(false);

    // Band selection must happen after the AP interface is started (above) but
    // before softAP() configures the channel. setBandMode is a no-op on 2.4-only
    // builds; on the C5 it forces the requested band.
    bool ok;
#if AP_USE_5GHZ
    WiFi.setBandMode(WIFI_BAND_MODE_5G_ONLY);
    ok = WiFi.softAP(AP_SSID, pw, AP_CHANNEL);
    if (!ok) {
        // 5 GHz refused (regulatory/client) — fall back so the AP still comes up.
        WiFi.setBandMode(WIFI_BAND_MODE_2G_ONLY);
        ok = WiFi.softAP(AP_SSID, pw, AP_CHANNEL_FALLBACK_2G);
    }
#else
    WiFi.setBandMode(WIFI_BAND_MODE_2G_ONLY);
    ok = WiFi.softAP(AP_SSID, pw, AP_CHANNEL);
#endif

    const IPAddress ip = WiFi.softAPIP();
    Serial.printf("[web] AP '%s' %s  band=%sGHz  ip=http://%s/\n",
                  AP_SSID, ok ? "up" : "FAILED TO START",
                  (WiFi.getBand() == WIFI_BAND_5G) ? "5" : "2.4",
                  ip.toString().c_str());
    s_dns.setErrorReplyCode(DNSReplyCode::NoError);
    s_dns.start(DNS_PORT, "*", ip);

    s_server.on("/", handleRoot);
    s_server.on("/api/status", handleApiStatus);
    s_server.on("/api/devices", handleApiDevices);
    s_server.onNotFound(handleNotFound);
    s_server.begin();

    // Run web server in its own task to avoid starvation by BLE scan on single-core C5.
    // 8 KB stack: WebServer request parsing + lwIP send path overflow a 4 KB stack.
    xTaskCreatePinnedToCore(web_portal_task, "web_portal", 8192, NULL, 1, NULL, 0);
}

void web_portal_loop() {
    // No-op; web server now runs in its own FreeRTOS task
}
