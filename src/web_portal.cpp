#include "web_portal.h"
#include "config.h"
#include "ble_scanner.h"
#include "scan_cycle.h"
#include "runtime_config.h"
#include "battery.h"

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <esp_netif.h>

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
    json += ",\"build\":";        appendJsonString(json, VORD_BUILD_ID);
    json += ",\"uptimeMs\":";     json += String(millis());
    json += ",\"scanning\":";     json += scan_cycle_is_running() ? "true" : "false";
    json += ",\"liveBle\":";      json += String(ble_scanner_count());
    json += ",\"liveSkimmer\":";  json += String(ble_scanner_skimmer_count());
    json += ",\"livePentool\":";  json += String(ble_scanner_pentool_count());
    json += ",\"sessionBle\":";   json += String(ble_scanner_session_count());
    json += ",\"sessionSkimmer\":"; json += String(ble_scanner_session_skimmer_count());
    json += ",\"sessionPentool\":"; json += String(ble_scanner_session_pentool_count());
    json += ",\"freeHeap\":";     json += String((uint32_t)ESP.getFreeHeap());
    const int vbatMv = battery_millivolts();
    json += ",\"vbatMv\":";       json += String(vbatMv);
    json += ",\"batPct\":";       json += String(vbatMv ? battery_percent(vbatMv) : 0);
    json += ",\"skimmerNames\":"; appendJsonString(json, getSkimmerNamesCsv());
    json += ",\"pentoolNames\":"; appendJsonString(json, getPentoolNamesCsv());
    json += "}";
    s_server.send(200, "application/json", json);
}

static void handleApiDevices() {
    std::vector<BleDeviceRecord> devices = ble_scanner_snapshot();

    // Stream the response as HTTP chunked transfer instead of building the whole
    // device list as one big String. On a board running with only ~30 KB free
    // heap, a 300-device list is a ~29 KB contiguous allocation that — on top of
    // the snapshot copy above — exceeds free heap, so the request fails and the
    // dashboard won't load. A small reused buffer flushed every few KB keeps the
    // peak allocation tiny and leaves headroom for the WiFi AP.
    s_server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    s_server.send(200, "application/json", "");

    String chunk;
    chunk.reserve(1280);
    chunk += "{\"nowMs\":";
    chunk += String(millis());
    chunk += ",\"count\":";
    chunk += String((int)devices.size());
    chunk += ",\"devices\":[";
    for (size_t i = 0; i < devices.size(); i++) {
        const BleDeviceRecord& d = devices[i];
        if (i) chunk += ',';
        chunk += "{\"mac\":";   appendJsonString(chunk, d.mac);
        chunk += ",\"name\":";  appendJsonString(chunk, d.name);
        chunk += ",\"rssi\":";  chunk += String(d.rssi);
        chunk += ",\"skimmer\":"; chunk += d.isSkimmer ? "true" : "false";
        chunk += ",\"pentool\":"; chunk += d.isPentool ? "true" : "false";
        chunk += ",\"seen\":";  chunk += String(d.seenCount);
        chunk += ",\"firstMs\":"; chunk += String(d.firstSeenMs);
        chunk += ",\"lastMs\":";  chunk += String(d.lastSeenMs);
        chunk += '}';
        if (chunk.length() >= 1024) {   // flush in small batches, reuse the buffer
            s_server.sendContent(chunk);
            chunk = "";
        }
    }
    chunk += "]}";
    s_server.sendContent(chunk);
    s_server.sendContent("");   // zero-length chunk terminates the response
}

// ---- Dashboard page -------------------------------------------------------

static const char INDEX_HTML[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang=en>
<head>
<meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1">
<title>Vord C5</title>
<link rel="icon" href='data:image/svg+xml,<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 32 32"><rect width="32" height="32" rx="7" fill="%23152018"/><text x="16" y="23" font-family="Sora,Segoe UI,system-ui,sans-serif" font-size="22" font-weight="700" text-anchor="middle" fill="%23f25f3a">V</text></svg>'>
<style>
:root{--bg:#f6f8f3;--bg-soft:#eaf0de;--ink:#152018;--muted:#4f6252;--accent:#f25f3a;--accent-2:#0da3b1;--panel:#fff;--line:#d8e1d0}
*{box-sizing:border-box}
html,body{margin:0;padding:0}
body{font-family:'Space Grotesk',system-ui,-apple-system,'Segoe UI',Roboto,sans-serif;color:var(--ink);min-height:100vh;
background:radial-gradient(circle at 10% 10%,rgba(13,163,177,.18),transparent 36%),radial-gradient(circle at 92% 18%,rgba(242,95,58,.18),transparent 30%),linear-gradient(135deg,var(--bg) 0%,#fff 50%,var(--bg-soft) 100%)}
.wrap{width:min(980px,94vw);margin:0 auto;padding:24px 0 40px}
.head{display:flex;align-items:flex-start;justify-content:space-between;gap:16px;flex-wrap:wrap}
.badge{display:inline-flex;align-items:center;gap:8px;padding:7px 12px;border-radius:999px;border:1px solid var(--line);color:var(--muted);font-size:12px;letter-spacing:.06em;text-transform:uppercase;background:var(--panel)}
h1{margin:10px 0 0;font-family:'Sora',system-ui,sans-serif;font-size:clamp(30px,6vw,52px);line-height:.95;letter-spacing:-.03em}
.accent{color:var(--accent)}
.meta{text-align:right;font-size:13px;color:var(--muted);padding-top:6px}
.dot{display:inline-flex;align-items:center;gap:8px;font-size:13px;color:var(--muted)}
.dot:before{content:"";width:9px;height:9px;border-radius:999px;background:#9aa}
.dot.on:before{background:var(--accent-2);box-shadow:0 0 0 4px rgba(13,163,177,.18)}
.dot.off:before{background:var(--accent)}
.stats{margin-top:20px;display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:12px}
.stat{background:var(--panel);border:1px solid var(--line);border-radius:16px;padding:14px 16px;box-shadow:0 12px 30px rgba(20,30,22,.06)}
.stat .n{font-family:'Sora',system-ui,sans-serif;font-size:28px;font-weight:800;line-height:1}
.stat .l{color:var(--muted);font-size:12px;letter-spacing:.04em;text-transform:uppercase;margin-top:6px}
.stat.skim .n{color:var(--accent)}
.stat.pent .n{color:#7c3aed}
.panel{margin-top:20px;background:var(--panel);border:1px solid var(--line);border-radius:22px;padding:20px;box-shadow:0 18px 44px rgba(20,30,22,.08)}
.bar-head{display:flex;align-items:center;justify-content:space-between;gap:12px;flex-wrap:wrap;margin-bottom:4px}
.panel h2{margin:0;font-family:'Sora',system-ui,sans-serif;font-size:20px}
.toggle{display:inline-flex;align-items:center;gap:8px;color:var(--muted);font-size:13px;cursor:pointer;user-select:none}
table{width:100%;border-collapse:collapse;margin-top:10px}
th,td{text-align:left;padding:10px 8px;border-bottom:1px solid var(--line);vertical-align:middle}
th{font-size:12px;letter-spacing:.04em;text-transform:uppercase;color:var(--muted)}
tr.skim td{background:rgba(242,95,58,.08)}
tr.skim td:first-child{box-shadow:inset 3px 0 0 var(--accent)}
tr.pent td{background:rgba(124,58,237,.07)}
tr.pent td:first-child{box-shadow:inset 3px 0 0 #7c3aed}
.mono{font-family:ui-monospace,Consolas,monospace;font-size:13px;color:var(--muted)}
.mac-sub{display:none;font-size:11px;margin-top:2px}
td:first-child{word-break:break-word}
.dim{color:var(--muted)}
.bar{display:inline-block;width:84px;height:7px;border-radius:999px;background:var(--bg-soft);overflow:hidden;vertical-align:middle;margin-right:8px;border:1px solid var(--line)}
.bar i{display:block;height:100%;background:linear-gradient(90deg,var(--accent-2),var(--accent))}
.tag{display:inline-block;padding:3px 9px;border-radius:999px;font-size:11px;font-weight:700;letter-spacing:.03em;background:rgba(242,95,58,.14);color:var(--accent)}
.tag.g{background:rgba(13,163,177,.14);color:var(--accent-2)}
.tag.p{background:rgba(124,58,237,.14);color:#7c3aed}
.foot{margin-top:16px;color:var(--muted);font-size:12px;text-align:center}
/* Full-page detection alert: two 1.5cm vertical bands on the left and right
   edges that blink 5x at 1Hz when a new flagged device appears. Square corners
   (no border-radius): each band is a horizontal gradient that's a solid color
   at the edge and fades in hue + transparency 1.5cm inward. pointer-events:none
   so it never blocks the table. skim = red->orange, pent = purple->yellow. */
#alert{position:fixed;inset:0;z-index:50;pointer-events:none;opacity:0}
#alert.run{animation:alertblink 1s ease-in-out 5}
@keyframes alertblink{0%,100%{opacity:0}50%{opacity:1}}
#alert.skim{background:linear-gradient(90deg,rgba(229,30,30,.95),rgba(255,140,30,.5) .4cm,transparent 1.5cm),linear-gradient(270deg,rgba(229,30,30,.95),rgba(255,140,30,.5) .4cm,transparent 1.5cm)}
#alert.pent{background:linear-gradient(90deg,rgba(150,40,220,.95),rgba(240,220,40,.5) .4cm,transparent 1.5cm),linear-gradient(270deg,rgba(150,40,220,.95),rgba(240,220,40,.5) .4cm,transparent 1.5cm)}
@media(max-width:520px){
.wrap{width:100%;padding:16px 12px 32px}
.meta{text-align:left;padding-top:0}
.panel{padding:14px;border-radius:16px}
.stat{padding:12px}
.stat .n{font-size:22px}
th,td{padding:8px 6px}
.bar{width:48px;margin-right:6px}
th:nth-child(2),td:nth-child(2){display:none}
.mac-sub{display:block}
.toggle input{width:18px;height:18px}
}
</style>
</head>
<body>
<div id=alert></div>
<main class=wrap>
<div class=head>
<div>
<div class=badge>Skim Detector</div>
<h1>Vord <span class=accent>C5</span></h1>
</div>
<div class=meta>
<span id=scan class="dot on">Scanning</span><br>
uptime <b id=up>-</b> &middot; heap <b id=heap>-</b><span id=batw style=display:none> &middot; batt <b id=bat>-</b></span>
</div>
</div>

<div class=stats>
<div class="stat skim"><div class=n id=lSkim>-</div><div class=l>Skimmer Views</div></div>
<div class="stat skim"><div class=n id=sSkim>-</div><div class=l>Skimmers</div></div>
<div class=stat><div class=n id=lBle>-</div><div class=l>BLE Views</div></div>
<div class=stat><div class=n id=sBle>-</div><div class=l>BLE Devices</div></div>
<div class="stat pent"><div class=n id=lPent>-</div><div class=l>Pentool Views</div></div>
<div class="stat pent"><div class=n id=sPent>-</div><div class=l>Pentools</div></div>
</div>

<section class=panel>
<div class=bar-head>
<h2>Devices <span class=dim id=count></span></h2>
<label class=toggle><input type=checkbox id=skimOnly> Flagged only</label>
</div>
<table><thead><tr><th>Name</th><th>MAC</th><th>RSSI</th><th>Type</th><th>Last seen</th></tr></thead>
<tbody id=rows></tbody></table>
</section>

<div class=foot>Vord C5 &middot; build <b id=build>-</b> &middot; refreshes every 2s</div>
</main>
<script>
const $=id=>document.getElementById(id);
function esc(s){return (s||'').replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]))}
function rssiPct(r){r=Math.max(-100,Math.min(-40,r));return Math.round((r+100)/60*100)}
function fmtUp(ms){let s=Math.floor(ms/1000),h=Math.floor(s/3600),m=Math.floor(s%3600/60);s=s%60;return (h?h+'h ':'')+((m||h)?m+'m ':'')+s+'s'}
function ago(ms){if(ms<0)ms=0;let s=Math.floor(ms/1000);if(s<60)return s+'s ago';let m=Math.floor(s/60);if(m<60)return m+'m ago';let h=Math.floor(m/60);if(h<24)return h+'h ago';return Math.floor(h/24)+'d ago'}
function cat(d){return d.skimmer?0:d.pentool?1:2}
// Border-blink alert on first sighting of a flagged MAC. A running blink isn't
// restarted (so a device staying in range blinks once, not forever), except a
// skimmer may override an in-progress pentool blink. The first tick only seeds
// the seen-sets (no blink) so devices already present at page load don't alert;
// only MACs that appear after the page is open trigger the border.
const seenSkim=new Set(),seenPent=new Set();
function fireAlert(kind){const a=$('alert');if(a.classList.contains('run')&&!(kind=='skim'&&a.classList.contains('pent')))return;a.classList.remove('run','skim','pent');void a.offsetWidth;a.classList.add(kind,'run')}
let skimOnly=false,primed=false;
async function tick(){
 try{
  const st=await fetch('/api/status').then(r=>r.json());
  const dv=await fetch('/api/devices').then(r=>r.json());
  let nS=false,nP=false;
  for(const d of dv.devices){if(d.skimmer){if(!seenSkim.has(d.mac)){seenSkim.add(d.mac);nS=true}}else if(d.pentool){if(!seenPent.has(d.mac)){seenPent.add(d.mac);nP=true}}}
  if(primed){if(nS)fireAlert('skim');else if(nP)fireAlert('pent')}
  primed=true;
  $('lBle').textContent=st.liveBle;$('lSkim').textContent=st.liveSkimmer;$('lPent').textContent=st.livePentool;
  $('sBle').textContent=st.sessionBle;$('sSkim').textContent=st.sessionSkimmer;$('sPent').textContent=st.sessionPentool;
  $('scan').textContent=st.scanning?'Scanning':'Paused';
  $('scan').className='dot '+(st.scanning?'on':'off');
  $('up').textContent=fmtUp(st.uptimeMs);
  $('heap').textContent=Math.round(st.freeHeap/1024)+'KB';
  $('build').textContent=st.build;
  if(st.vbatMv){$('batw').style.display='';$('bat').textContent=(st.vbatMv/1000).toFixed(2)+'V '+st.batPct+'%';$('bat').style.color=st.batPct<=15?'var(--accent)':''}
  const now=dv.nowMs;
  // Skimmers first, pentools next, everything else after; newest-seen on top within each group.
  let rows=dv.devices.slice().sort((a,b)=>cat(a)-cat(b)||b.lastMs-a.lastMs);
  if(skimOnly)rows=rows.filter(d=>d.skimmer||d.pentool);
  $('rows').innerHTML=rows.map(d=>{var p=rssiPct(d.rssi);return '<tr class="'+(d.skimmer?'skim':d.pentool?'pent':'')+'"><td>'+(esc(d.name)||'<span class=dim>(unnamed)</span>')+'<div class="mac-sub mono">'+esc(d.mac)+'</div></td><td class=mono>'+esc(d.mac)+'</td><td><span class=bar><i style="width:'+p+'%"></i></span><span class=dim>'+d.rssi+'</span></td><td>'+(d.skimmer?'<span class=tag>SKIMMER</span>':d.pentool?'<span class="tag p">PENTOOL</span>':'<span class="tag g">BLE</span>')+'</td><td class=dim>'+ago(now-d.lastMs)+'</td></tr>'}).join('')||'<tr><td colspan=5 class=dim>No devices yet&hellip;</td></tr>';
  $('count').textContent='('+rows.length+')';
 }catch(e){console.error(e)}
}
$('skimOnly').addEventListener('change',function(e){skimOnly=e.target.checked;tick()});
$('alert').addEventListener('animationend',function(){this.classList.remove('run','skim','pent')});
tick();setInterval(tick,2000);
</script>
</body>
</html>)HTML";


static void handleRoot() {
    s_server.send_P(200, "text/html", INDEX_HTML);
}

// Unknown paths must return a clean failure (404), NOT the dashboard. OS
// connectivity probes (Android generate_204, iOS captive.apple.com) hit an
// unknown host/path; failing them makes the phone mark this AP "no internet"
// and keep routing internet traffic over mobile data, while 192.168.4.1 stays
// reachable over WiFi. Serving the dashboard here would look like a captive
// portal and stall the phone's data connection.
static void handleNotFound() {
    s_server.send(404, "text/plain", "Vord C5 - dashboard at http://192.168.4.1/");
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

    // Don't advertise a default gateway in DHCP offers. The AP has no upstream,
    // so a network without a gateway is what we actually are: phones treat it
    // as local-only from the moment they join — 192.168.4.1 stays reachable
    // while internet traffic keeps flowing over mobile data. Relying only on
    // failed connectivity probes for that verdict breaks when the client has a
    // static IP/DNS configured (probes time out slowly instead of failing
    // fast, and the phone routes internet into us meanwhile).
    if (esp_netif_t* apNetif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF")) {
        uint8_t offerGw = 0;   // dhcps_offer_t: 0 = omit the router (option 3)
        esp_netif_dhcps_stop(apNetif);
        esp_netif_dhcps_option(apNetif, ESP_NETIF_OP_SET,
                               ESP_NETIF_ROUTER_SOLICITATION_ADDRESS,
                               &offerGw, sizeof(offerGw));
        esp_netif_dhcps_start(apNetif);
    }

    const IPAddress ip = WiFi.softAPIP();
    Serial.printf("[web] AP '%s' %s  band=%sGHz  ip=http://%s/\n",
                  AP_SSID, ok ? "up" : "FAILED TO START",
                  (WiFi.getBand() == WIFI_BAND_5G) ? "5" : "2.4",
                  ip.toString().c_str());
    // Resolve only "vord" to our IP; answer everything else NXDOMAIN, fast.
    // A wildcard hijack ("*") would make phones treat the AP as a captive
    // portal and pin their internet traffic to it instead of falling back to
    // mobile data. Fast NXDOMAIN = clean "no internet" verdict on the phone.
    s_dns.setErrorReplyCode(DNSReplyCode::NonExistentDomain);
    s_dns.start(DNS_PORT, "vord", ip);

    s_server.on("/", handleRoot);
    s_server.on("/api/status", handleApiStatus);
    s_server.on("/api/devices", handleApiDevices);
    s_server.onNotFound(handleNotFound);
    s_server.begin();

    // Run web server in its own task to avoid starvation by BLE scan on single-core C5.
    // 8 KB stack: WebServer request parsing + lwIP send path overflow a 4 KB stack.
    // Priority 2: above the display/LED/scan tasks so a busy alert flash can't
    // starve HTTP/DNS; the 2 ms sleep per loop keeps it from hogging in return.
    xTaskCreatePinnedToCore(web_portal_task, "web_portal", 8192, NULL, 2, NULL, 0);
}

void web_portal_loop() {
    // No-op; web server now runs in its own FreeRTOS task
}
