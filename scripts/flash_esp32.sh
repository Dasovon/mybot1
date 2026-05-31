#!/usr/bin/env bash
# Flash ESP32-S3 firmware. Run on the Pi where /dev/ttyACM0 is local.
# Uses 'python3 -m esptool' (esptool v4+/v5+) with modern hyphenated flags.
#
# Production flash path: native USB CDC on /dev/ttyACM0 only.
# Do NOT flash through CH340 /dev/ttyUSB0 — that port is for debug output only.
#
# Usage (on Pi):
#   ~/bot_ws/scripts/flash_esp32.sh
#
# PlatformIO build outputs (relative to firmware/esp32/):
#   .pio/build/esp32-s3-devkitc-1/bootloader.bin   → 0x0
#   .pio/build/esp32-s3-devkitc-1/partitions.bin   → 0x8000
#   boot_app0.bin (from PlatformIO framework)       → 0xe000  ← resets OTA state
#   .pio/build/esp32-s3-devkitc-1/firmware.bin     → 0x10000
#
# 0xe000 (otadata) MUST be flashed. Without it, the bootloader may select
# app1 (the OTA partition) which still contains the old firmware, causing
# the new app0 flash to be silently ignored on next boot.
#
# Put the ESP32-S3 into download mode before running:
#   Hold BOOT, press and release RESET, release BOOT.
# Or just run — esptool triggers auto-reset via RTS/DTR on native USB.

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

# Warn if firmware.bin is more than 1 hour old — the Pi's build directory
# can contain a stale binary from a previous local build while the dev PC
# has a newer one. Always copy the dev-PC binary here before flashing:
#   scp firmware/esp32/.pio/build/esp32-s3-devkitc-1/firmware.bin ubuntu@pi5bot:~/bot_ws/firmware/esp32/.pio/build/esp32-s3-devkitc-1/firmware.bin
FW_AGE=$(( $(date +%s) - $(stat -c %Y "${BUILD_DIR}/firmware.bin") ))
if [[ $FW_AGE -gt 3600 ]]; then
    echo "WARNING: firmware.bin is $(( FW_AGE / 3600 ))h old — is this the latest build from the dev PC?" >&2
fi

# boot_app0.bin resets otadata so bootloader always picks app0 (slot 0).
# Located in the PlatformIO Arduino framework — find it dynamically.
BOOT_APP0=$(find ~/.platformio/packages/framework-arduinoespressif32 \
    -name "boot_app0.bin" 2>/dev/null | head -1)
if [ -z "$BOOT_APP0" ]; then
    echo "ERROR: boot_app0.bin not found in PlatformIO packages." >&2
    echo "       Install PlatformIO and run 'pio run' in firmware/esp32/ first." >&2
    exit 1
fi

# Stop micro-ROS agent so it cannot grab /dev/ttyACM0 between esptool's
# 1200bps reset touch and the write connection (reproduces mid-write failure).
# Trap ensures agent is always restored on exit, even if flash fails.
restore_agent() {
    echo "Restoring micro-ROS agent..."
    sudo systemctl unmask microros-agent.service 2>/dev/null || true
    sudo systemctl start microros-agent.service 2>/dev/null || true
}
trap restore_agent EXIT

echo "Stopping micro-ROS agent..."
sudo systemctl stop  microros-agent.service 2>/dev/null || true
sudo systemctl mask  microros-agent.service 2>/dev/null || true
sudo fuser -k "${PORT}" 2>/dev/null || true
# Verify port is free before proceeding
if sudo fuser "${PORT}" 2>/dev/null; then
    echo "ERROR: ${PORT} still held by another process. Aborting." >&2
    exit 1
fi

echo "Flashing ESP32-S3 on ${PORT} ..."
echo "  bootloader : 0x0000  ${BUILD_DIR}/bootloader.bin"
echo "  partitions : 0x8000  ${BUILD_DIR}/partitions.bin"
echo "  otadata    : 0xe000  ${BOOT_APP0}"
echo "  app0       : 0x10000 ${BUILD_DIR}/firmware.bin"

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
    0xe000  "${BOOT_APP0}" \
    0x10000 "${BUILD_DIR}/firmware.bin"

echo "Done. Verify firmware version via CH340 (/dev/ttyUSB0, 115200 baud):"
echo "  Expected: [uROS] init: PING_INTERVAL=1000ms CONNECTED_PING=2000ms ..."
