#!/usr/bin/env bash
# mybot1 boot health check — runs after microros-agent and mybot-display.
# Auto-fixes read-only filesystem and stopped services, then verifies
# ROS topics are live. Results go to journalctl (systemd) or stdout.

set -eo pipefail

PASS=0
FAIL=0

log_pass() { echo "[PASS] $1"; (( PASS++ )) || true; }
log_fail() { echo "[FAIL] $1"; (( FAIL++ )) || true; }
log_fix()  { echo "[FIX ] $1"; }

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
# 2. microros-agent must be running
# ---------------------------------------------------------------------------
if systemctl is-active --quiet microros-agent.service; then
    log_pass "microros-agent.service is active"
else
    log_fix "microros-agent.service not active — starting"
    sudo systemctl start microros-agent.service
    sleep 3
    if systemctl is-active --quiet microros-agent.service; then
        log_pass "microros-agent.service started successfully"
    else
        log_fail "microros-agent.service failed to start"
    fi
fi

# ---------------------------------------------------------------------------
# 3. mybot-display must be running (restart picks up writable /tmp for lgpio)
# ---------------------------------------------------------------------------
if systemctl is-active --quiet mybot-display.service; then
    # Check logs for the lgpio read-only error since last start
    START_TIME=$(systemctl show mybot-display.service --property=ActiveEnterTimestamp \
        | cut -d= -f2)
    if journalctl -u mybot-display.service --since "$START_TIME" --no-pager -q \
            2>/dev/null | grep -q "Read-only file system"; then
        log_fix "mybot-display started on read-only fs — restarting now that fs is rw"
        sudo systemctl restart mybot-display.service
        sleep 3
    fi
fi

if systemctl is-active --quiet mybot-display.service; then
    log_pass "mybot-display.service is active"
else
    log_fix "mybot-display.service not active — starting"
    sudo systemctl start mybot-display.service
    sleep 3
    if systemctl is-active --quiet mybot-display.service; then
        log_pass "mybot-display.service started successfully"
    else
        log_fail "mybot-display.service failed to start"
    fi
fi

# ---------------------------------------------------------------------------
# 4. ROS topic health — ESP32 must be publishing within 15 s of agent start
# ---------------------------------------------------------------------------
source /opt/ros/jazzy/setup.bash
source /home/ubuntu/microros_ws/install/setup.bash

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

    # Compare using awk (bash can't do float comparison)
    if awk "BEGIN { exit !($rate >= $min_hz) }"; then
        log_pass "$label: ${rate} Hz (>= ${min_hz} Hz required)"
    else
        log_fail "$label: ${rate} Hz (< ${min_hz} Hz required)"
    fi
}

check_topic_hz /diff_cont/odom  25.0 "odom"
check_topic_hz /imu/imu         25.0 "IMU"

# Battery only publishes at 1 Hz — just check it shows up
BATT=$(timeout 5 ros2 topic echo /battery_state --once 2>/dev/null)
if echo "$BATT" | grep -q 'voltage'; then
    VOLTAGE=$(echo "$BATT" | grep '^voltage:' | awk '{print $2}')
    log_pass "battery_state: ${VOLTAGE} V"
else
    log_fail "battery_state: no message received"
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
