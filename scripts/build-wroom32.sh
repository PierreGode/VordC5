#!/usr/bin/env bash
# =====================================================================
#  build-wroom32.sh — compile the Vord classic-Bluetooth scout for the
#  ESP32-WROOM-32 (classic ESP32 Dev Module).
#
#  This is the standalone sidecar sketch in wroom32-classic-bt/: a
#  classic-BT-capable WROOM-32 that runs a BR/EDR inquiry, matches the
#  same skimmer fingerprints the C5 uses, and forwards each hit to the
#  C5 over UART. See wroom32-classic-bt/README.md.
#
#  The WROOM-32 board definition lives only in the official Espressif
#  esp32 Arduino core (FQBN esp32:esp32:esp32), so — like the XIAO C5
#  target — this is built with arduino-cli rather than the PlatformIO
#  pipeline used for the C5 boards. The sketch is already self-contained
#  (setup()/loop() in the .ino), so no throwaway wrapper is needed.
#
#  Usage:   bash scripts/build-wroom32.sh
#  Output:  wroom32-classic-bt/build/esp32.esp32.esp32/wroom32-classic-bt.ino.*
#
#  Prereqs: arduino-cli on PATH and the esp32 core installed:
#    arduino-cli config init
#    arduino-cli config add board_manager.additional_urls \
#      https://espressif.github.io/arduino-esp32/package_esp32_dev_index.json
#    arduino-cli core update-index
#    arduino-cli core install esp32:esp32
# =====================================================================
set -euo pipefail

# Repo root = parent of this script's dir, regardless of where it's invoked.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$ROOT"

SKETCH_DIR="wroom32-classic-bt"

# "ESP32 Dev Module" with the stock 4 MB "default" partition scheme (1.2 MB
# app / 1.5 MB SPIFFS). Classic Bluetooth (Bluedroid) is enabled in this
# profile — the sketch #errors out at compile time if it isn't.
FQBN="esp32:esp32:esp32:PartitionScheme=default"
ARDUINO_CLI_BIN="${ARDUINO_CLI:-arduino-cli}"

echo "==> Compiling $SKETCH_DIR for $FQBN"
"$ARDUINO_CLI_BIN" compile \
  --fqbn "$FQBN" \
  --export-binaries \
  "$SKETCH_DIR"

echo "==> Done. Binaries in $SKETCH_DIR/build/esp32.esp32.esp32/"
