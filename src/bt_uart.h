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
// Line protocol (newline-terminated), one per hit:
//     VBT1|<TYPE>|<RSSI>|<MAC>|<NAME>
//   TYPE  = SKIM | PENT
//   RSSI  = integer dBm (negative), best-effort from the classic inquiry
//   MAC   = aa:bb:cc:dd:ee:ff
//   NAME  = device name (no '|' or newline; sender sanitizes)
//
// See config.h (VORD_HAS_CLASSIC_BT_UART block) for wiring and pins, and
// wroom32-classic-bt/ for the sender sketch.

#if VORD_HAS_CLASSIC_BT_UART

// Open the UART and start the listener task. Safe no-op if already started.
void bt_uart_init();

#else

// Compiled-out no-op so main.cpp can call it unconditionally.
static inline void bt_uart_init() {}

#endif // VORD_HAS_CLASSIC_BT_UART

#endif // BT_UART_H
