#ifndef WEB_PORTAL_H
#define WEB_PORTAL_H

// Self-hosted WiFi Access Point + web dashboard.
//
// At boot the device starts its own WiFi network (AP_SSID/AP_PASSWORD from
// config.h) and serves a live page listing every BLE device it has seen and
// which ones matched a skimmer fingerprint, at http://192.168.4.1/. The AP
// deliberately fails OS connectivity probes (404 + NXDOMAIN for unknown
// hosts) so phones classify it as "no internet" and keep using mobile data
// while connected. BLE scanning continues alongside WiFi via coexistence.

void web_portal_init();   // bring up the AP, DNS and HTTP server (call once in setup)
void web_portal_loop();   // service DNS + HTTP clients (call frequently from loop)

#endif // WEB_PORTAL_H
