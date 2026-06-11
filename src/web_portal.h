#ifndef WEB_PORTAL_H
#define WEB_PORTAL_H

// Self-hosted WiFi Access Point + web dashboard.
//
// At boot the device starts its own WiFi network (AP_SSID/AP_PASSWORD from
// config.h) and serves a live page listing every BLE device it has seen and
// which ones matched a skimmer fingerprint. A captive-portal DNS responder
// redirects all hostnames to the dashboard so connecting clients pop straight
// into it. BLE scanning continues alongside WiFi via the chip's coexistence.

void web_portal_init();   // bring up the AP, DNS and HTTP server (call once in setup)
void web_portal_loop();   // service DNS + HTTP clients (call frequently from loop)

#endif // WEB_PORTAL_H
