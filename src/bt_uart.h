#ifndef BT_UART_H
#define BT_UART_H

#include <Arduino.h>
#include "config.h"

// Receiver for the classic-Bluetooth (BR/EDR) UART sidecar. A WROOM-32 next to
// the C5 performs a classic inquiry, matches device names against the same
// skimmer fingerprints, and sends one line per hit over UART. This module reads
// those lines on a hardware UART and drives the C5's existing alert path
// (skimmer_led_notify / skimmer_display_notify) plus the dashboard device table
// (ble_scanner_register_external) — the exact same alarm as a BLE hit.
//
// Line protocol (newline-terminated), one per hit plus a per-round heartbeat:
//     VBT1|<TYPE>|<RSSI>|<MAC>|<NAME>
//   TYPE  = SKIM | PENT | PING   (PING = liveness heartbeat, no device)
//   RSSI  = integer dBm (negative), best-effort from the classic inquiry
//   MAC   = aa:bb:cc:dd:ee:ff
//   NAME  = device name (no '|' or newline; sender sanitizes)
// Any well-formed VBT1 line refreshes the scout-liveness timestamp
// (bt_uart_scout_last_seen_ms); only SKIM/PENT register a device + alert.
//
// See config.h (VORD_HAS_CLASSIC_BT_UART block) for wiring and pins, and
// wroom32-classic-bt/ for the sender sketch.

#if VORD_HAS_CLASSIC_BT_UART

// Open the UART and start the listener task. Safe no-op if already started.
void bt_uart_init();

// True when the classic-BT scout receiver is compiled in (it is, here).
static inline bool bt_uart_enabled() { return true; }

// millis() timestamp of the last well-formed VBT1 line received from the
// WROOM-32 scout — a hit OR a periodic heartbeat — so the dashboard can show
// whether the scout link is alive. Returns 0 if nothing has been heard yet.
uint32_t bt_uart_scout_last_seen_ms();

#else

// Compiled-out no-ops so callers (main.cpp, web_portal) need no #if guards.
static inline void bt_uart_init() {}
static inline bool bt_uart_enabled() { return false; }
static inline uint32_t bt_uart_scout_last_seen_ms() { return 0; }

#endif // VORD_HAS_CLASSIC_BT_UART

#endif // BT_UART_H
