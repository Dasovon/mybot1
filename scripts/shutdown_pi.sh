#!/usr/bin/env bash
# shutdown_pi.sh — clean Pi shutdown before cutting power
#
# Usage (run on dev PC):
#   ./scripts/shutdown_pi.sh
#
# Stops all robot services cleanly, then initiates OS shutdown.
# Run this every time before cutting battery power.

PI=ubuntu@pi5bot

echo "Stopping robot services on ${PI}..."

ssh "${PI}" bash -s <<'REMOTE'
# Kill any active bag recordings first
pkill -f "ros2 bag record" 2>/dev/null && echo "  killed zombie bag processes" || true

# Stop services in dependency order (display depends on ROS, agent holds the serial port)
for svc in mybot-display.service robot-launch.service microros-agent.service; do
    if systemctl is-active --quiet "$svc"; then
        sudo systemctl stop "$svc"
        echo "  stopped $svc"
    else
        echo "  $svc already stopped"
    fi
done
REMOTE

echo "All services stopped. Initiating shutdown..."
ssh "${PI}" "sudo shutdown now" 2>/dev/null || true

echo "Waiting for Pi to halt..."
until ! ssh -o ConnectTimeout=3 -o StrictHostKeyChecking=no \
      -o LogLevel=ERROR "${PI}" "echo up" 2>/dev/null; do
    sleep 2
done

echo ""
echo "Pi has shut down cleanly. Safe to kill power."
