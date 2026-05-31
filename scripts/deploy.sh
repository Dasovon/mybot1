#!/usr/bin/env bash
# deploy.sh — install and enable all mybot1 systemd services on the Pi.
#
# Run once after cloning the repo, or after updating any .service file.
# Must be run on the Pi as ubuntu (has sudo).
#
# Usage:
#   ~/bot_ws/scripts/deploy.sh

set -euo pipefail

SCRIPTS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

SERVICES=(
    microros-agent.service
    mybot-battery.service
    mybot-display.service
    mybot-health.service
    robot-launch.service
    mybot-lidar.service
    mybot-lidar-motor-off.service
)

echo "=== mybot1 service deployment ==="
echo ""

# ── Copy service files ──────────────────────────────────────────────────────
echo "[1] Installing service files to /etc/systemd/system/"
for svc in "${SERVICES[@]}"; do
    src="${SCRIPTS_DIR}/${svc}"
    if [ ! -f "$src" ]; then
        echo "  SKIP: ${svc} not found in scripts/ — skipping"
        continue
    fi
    sudo cp "$src" "/etc/systemd/system/${svc}"
    echo "  installed: ${svc}"
done

# ── Reload systemd ──────────────────────────────────────────────────────────
echo ""
echo "[2] Reloading systemd daemon"
sudo systemctl daemon-reload

# ── Enable services ─────────────────────────────────────────────────────────
echo ""
echo "[3] Enabling services (start on boot)"
for svc in "${SERVICES[@]}"; do
    if systemctl cat "${svc}" >/dev/null 2>&1; then
        if [ "$svc" = "mybot-lidar.service" ]; then
            # LiDAR ROS driver is NOT auto-enabled — start manually when scanning
            sudo systemctl disable "$svc" 2>/dev/null || true
            echo "  installed (not auto-enabled): ${svc}"
        else
            sudo systemctl enable "$svc"
            echo "  enabled: ${svc}"
        fi
    else
        echo "  SKIP: ${svc} not found in systemd"
    fi
done

# ── Start services now ──────────────────────────────────────────────────────
echo ""
echo "[4] Starting services"
echo "  Note: mybot-lidar.service is NOT auto-started — run 'sudo systemctl start mybot-lidar.service' when LiDAR is needed"
for svc in microros-agent.service mybot-battery.service mybot-display.service robot-launch.service mybot-lidar-motor-off.service; do
    if systemctl cat "${svc}" >/dev/null 2>&1; then
        sudo systemctl restart "$svc" && echo "  started: ${svc}" || echo "  WARN: ${svc} failed to start"
    fi
done

# mybot-health.service is oneshot — trigger it manually after other services settle
echo ""
echo "[5] Waiting 10s for services to settle, then running health check..."
sleep 10
sudo systemctl start mybot-health.service || true
journalctl -u mybot-health.service --no-pager -n 30

echo ""
echo "=== Deployment complete ==="
echo "To re-run health check: sudo systemctl start mybot-health.service"
echo "To view health log:     journalctl -u mybot-health.service --no-pager -n 50"
