#!/usr/bin/env bash
# shutdown_pi.sh — clean Pi shutdown before cutting power
#
# Usage (run on dev PC):
#   ./scripts/shutdown_pi.sh
#
# Initiates a clean shutdown, waits until the Pi halts, then tells you
# it is safe to kill power. Run this every time before cutting battery.

PI=ubuntu@pi5bot

echo "Shutting down ${PI}..."
ssh "${PI}" "sudo shutdown now" 2>/dev/null || true

echo "Waiting for Pi to halt..."
until ! ssh -o ConnectTimeout=3 -o StrictHostKeyChecking=no \
      -o LogLevel=ERROR "${PI}" "echo up" 2>/dev/null; do
    sleep 2
done

echo ""
echo "✓ Pi has shut down cleanly. Safe to kill power."
