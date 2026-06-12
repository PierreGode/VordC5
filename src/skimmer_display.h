#ifndef SKIMMER_DISPLAY_H
#define SKIMMER_DISPLAY_H

#include <Arduino.h>
#include "config.h"
#include "skimmer_led.h"   // for the SkimmerLedAlert enum (defined unconditionally)

// Optional status + proximity alert screen on the LilyGO T-Dongle-C5's ST7735
// 80x160 display (enabled with -DVORD_HAS_DISPLAY=1). The screen is white with
// the session counts ("BLE n" / "SKIM n") centred on it. While a flagged
// device is in range a red border (3 px solid + 2 px fading inward) blinks
// around the edge, and the blink rate tracks distance — a stronger RSSI
// (closer source) blinks faster, a weaker one (farther) blinks slowly. When
// nothing has been seen for DISPLAY_HOLD_MS the border disappears and the
// counts remain. Same notify-driven model as skimmer_led.

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
