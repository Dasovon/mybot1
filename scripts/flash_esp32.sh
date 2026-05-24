#!/usr/bin/env bash
# Flash ESP32-S3 firmware from a Pi or dev PC.
# Uses 'python3 -m esptool' (esptool v4+/v5+) with modern hyphenated flags.
#
# Usage:
#   ./scripts/flash_esp32.sh [port]
#
# Default port: /dev/ttyACM0 (native USB CDC)
# For the USB-UART adapter: /dev/ttyUSB0
#
# PlatformIO build outputs (relative to firmware/esp32/):
#   .pio/build/esp32-s3-devkitc-1/bootloader.bin   → 0x0
#   .pio/build/esp32-s3-devkitc-1/partitions.bin   → 0x8000
#   .pio/build/esp32-s3-devkitc-1/firmware.bin     → 0x10000
#
# Put the ESP32-S3 into download mode before running:
#   Hold BOOT, press and release RESET, release BOOT.

set -euo pipefail

PORT="${1:-/dev/ttyACM0}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/../firmware/esp32/.pio/build/esp32-s3-devkitc-1"

for f in bootloader.bin partitions.bin firmware.bin; do
    if [ ! -f "${BUILD_DIR}/${f}" ]; then
        echo "ERROR: ${BUILD_DIR}/${f} not found — run 'pio run' first." >&2
        exit 1
    fi
done

echo "Flashing ESP32-S3 on ${PORT} ..."
python3 -m esptool \
    --chip esp32s3 \
    --port "${PORT}" \
    --baud 921600 \
    --before default-reset \
    --after hard-reset \
    write-flash \
    --flash-mode dio \
    --flash-freq 80m \
    --flash-size detect \
    0x0     "${BUILD_DIR}/bootloader.bin" \
    0x8000  "${BUILD_DIR}/partitions.bin" \
    0x10000 "${BUILD_DIR}/firmware.bin"

echo "Done."
