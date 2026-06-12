#ifndef SKIMMER_DISPLAY_H
#define SKIMMER_DISPLAY_H

#include <Arduino.h>
#include "config.h"
#include "skimmer_led.h"   // for the SkimmerLedAlert enum (defined unconditionally)

// Optional full-screen proximity alert on the LilyGO T-Dongle-C5's ST7735
// 80x160 display (enabled with -DVORD_HAS_DISPLAY=1). It mirrors the proximity
// LED behaviour but on the screen: while a flagged device is in range the whole
// screen flashes RED, and the flash rate tracks distance — a stronger RSSI
// (closer source) flashes faster, a weaker one (farther) flashes slowly. When
// nothing has been seen for DISPLAY_HOLD_MS the screen returns to a calm dim
// "scanning / all clear" colour. Same notify-driven model as skimmer_led.

#if VORD_HAS_DISPLAY

// Bring up the panel and start the flashing task. Safe no-op if already started.
void skimmer_display_init();

// Report a fresh sighting and its RSSI (called from the BLE callback alongside
// skimmer_led_notify). Resets the hold timer; the flash rate tracks the most
// recent RSSI.
void skimmer_display_notify(int rssi, SkimmerLedAlert type);

#else

// Compiled-out no-ops so callers don't need their own #if guards.
static inline void skimmer_display_init() {}
static inline void skimmer_display_notify(int, SkimmerLedAlert) {}

#endif // VORD_HAS_DISPLAY

#endif // SKIMMER_DISPLAY_H
