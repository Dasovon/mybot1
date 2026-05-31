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
    if systemctl list-unit-files | grep -q "^${svc}"; then
        sudo systemctl enable "$svc"
        echo "  enabled: ${svc}"
    else
        echo "  SKIP: ${svc} not found in systemd"
    fi
done

# ── Start services now ──────────────────────────────────────────────────────
echo ""
echo "[4] Starting services"
for svc in microros-agent.service mybot-battery.service mybot-display.service robot-launch.service; do
    if systemctl list-unit-files | grep -q "^${svc}"; then
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
