#ifndef BATTERY_H
#define BATTERY_H

// LiPo battery voltage monitoring via the board's VBAT divider (see the
// "Battery monitoring" section in config.h for per-board wiring/defaults).

void battery_init();          // configure ADC + optional divider-enable pin (call once in setup)
int  battery_millivolts();    // battery voltage in mV (cached, refreshed every few seconds); 0 = disabled
int  battery_percent(int mv); // rough resting-voltage charge estimate, 0-100

#endif // BATTERY_H
