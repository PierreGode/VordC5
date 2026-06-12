#ifndef RUNTIME_CONFIG_H
#define RUNTIME_CONFIG_H

#include <Arduino.h>

// Runtime config currently holds the skimmer and pentest-tool name
// fingerprints only. Values are RAM-only and reset on boot.

bool   isSkimmerName(const String& name);
String getSkimmerNamesCsv();

bool   isPentoolName(const String& name);
String getPentoolNamesCsv();

void runtime_config_init();

#endif // RUNTIME_CONFIG_H
