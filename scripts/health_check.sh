#!/usr/bin/env bash
# mybot1 boot health check — runs after all robot services have started.
# Auto-fixes read-only filesystem and stopped services, then verifies
# ROS topics are live. Results go to journalctl (systemd) or stdout.

set -eo pipefail

PASS=0
FAIL=0

log_pass() { echo "[PASS] $1"; (( PASS++ )) || true; }
log_fail() { echo "[FAIL] $1"; (( FAIL++ )) || true; }
log_fix()  { echo "[FIX ] $1"; }

ensure_service() {
    local svc="$1"
    if systemctl is-active --quiet "$svc"; then
        log_pass "$svc is active"
    else
        log_fix "$svc not active — starting"
        sudo systemctl start "$svc"
        sleep 3
        if systemctl is-active --quiet "$svc"; then
            log_pass "$svc started successfully"
        else
            log_fail "$svc failed to start"
        fi
    fi
}

# ---------------------------------------------------------------------------
# 1. Filesystem must be read-write (lgpio, ROS logs, pip installs all need it)
# ---------------------------------------------------------------------------
if touch /tmp/.mybot_rw_check 2>/dev/null; then
    rm -f /tmp/.mybot_rw_check
    log_pass "Root filesystem is read-write"
else
    log_fix "Root filesystem is read-only — remounting rw"
    ROOT_DEV=$(findmnt -n -o SOURCE /)
    if sudo mount -o remount,rw "$ROOT_DEV" / 2>/dev/null; then
        log_pass "Root filesystem remounted read-write"
    else
        log_fail "Could not remount root read-write ($ROOT_DEV)"
    fi
fi

# ---------------------------------------------------------------------------
# 2. Required services must be running
# ---------------------------------------------------------------------------
ensure_service microros-agent.service

# Display: restart if it started on a read-only filesystem
if systemctl is-active --quiet mybot-display.service; then
    START_TIME=$(systemctl show mybot-display.service --property=ActiveEnterTimestamp \
        | cut -d= -f2)
    if journalctl -u mybot-display.service --since "$START_TIME" --no-pager -q \
            2>/dev/null | grep -q "Read-only file system"; then
        log_fix "mybot-display started on read-only fs — restarting"
        sudo systemctl restart mybot-display.service
        sleep 3
    fi
fi
ensure_service mybot-display.service
ensure_service mybot-battery.service

# ---------------------------------------------------------------------------
# 3. INA219 sanity check — read voltage directly via smbus2
# ---------------------------------------------------------------------------
BAT_VOLTAGE=$(python3 - <<'EOF'
try:
    from ina219 import INA219
    ina = INA219(0.1, busnum=1)
    ina.configure()
    print(f"{ina.voltage():.2f}")
except Exception as e:
    print(f"ERROR: {e}")
EOF
)

if echo "$BAT_VOLTAGE" | grep -qE '^[0-9]+\.[0-9]+$'; then
    # Sanity check: must be between 8 V and 16 V (covers flat to full 3S + margin)
    if awk "BEGIN { exit !($BAT_VOLTAGE >= 8.0 && $BAT_VOLTAGE <= 16.0) }"; then
        log_pass "INA219 battery voltage: ${BAT_VOLTAGE} V"
    else
        log_fail "INA219 battery voltage out of range: ${BAT_VOLTAGE} V (expected 8–16 V)"
    fi
else
    log_fail "INA219 read failed: $BAT_VOLTAGE"
fi

# ---------------------------------------------------------------------------
# 4. ROS topic health — ESP32 must be publishing via micro-ROS
# ---------------------------------------------------------------------------
source /opt/ros/jazzy/setup.bash
source /home/ubuntu/microros_ws/install/setup.bash
source /home/ubuntu/bot_ws/install/setup.bash

check_topic_hz() {
    local topic="$1"
    local min_hz="$2"
    local label="$3"
    local timeout_s=15

    local rate
    rate=$(timeout "$timeout_s" ros2 topic hz "$topic" 2>/dev/null \
        | grep 'average rate' | head -1 | awk '{print $3}' | tr -d ':' || true)

    if [[ -z "$rate" ]]; then
        log_fail "$label: no messages received within ${timeout_s}s"
        return
    fi

    if awk "BEGIN { exit !($rate >= $min_hz) }"; then
        log_pass "$label: ${rate} Hz (>= ${min_hz} Hz required)"
    else
        log_fail "$label: ${rate} Hz (< ${min_hz} Hz required)"
    fi
}

check_topic_hz /diff_cont/odom 25.0 "odom"
check_topic_hz /imu/imu        25.0 "IMU"

# /battery_state: published by Pi battery node at 1 Hz
BATT=$(timeout 5 ros2 topic echo /battery_state --once --qos-reliability reliable 2>/dev/null || true)
if echo "$BATT" | grep -q 'voltage:'; then
    VOLTAGE=$(echo "$BATT" | grep '^voltage:' | awk '{print $2}')
    if awk "BEGIN { exit !($VOLTAGE >= 8.0 && $VOLTAGE <= 16.0) }"; then
        log_pass "/battery_state: ${VOLTAGE} V"
    else
        log_fail "/battery_state: voltage out of range: ${VOLTAGE} V"
    fi
else
    log_fail "/battery_state: no message received from Pi battery publisher"
fi

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
echo "----------------------------------------"
echo "Health check complete: ${PASS} passed, ${FAIL} failed"
if [[ $FAIL -eq 0 ]]; then
    echo "STATUS: OK — robot ready"
    exit 0
else
    echo "STATUS: DEGRADED — ${FAIL} check(s) failed (see above)"
    exit 1
fi
