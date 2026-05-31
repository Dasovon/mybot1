#!/usr/bin/env bash
# mybot1 Phase 3 health check
#
# Validates the full boot stack: services, INA219 voltage, ROS topics,
# EKF output, TF transforms, and odom covariance.
#
# Topic rate checks use QoS-compatible echo-count method, NOT ros2 topic hz.
# (ros2 topic hz /odom showed false ~1.2 Hz on a healthy 20 Hz EKF due to
# QoS subscriber mismatch — documented 2026-05-30.)
#
# Usage:
#   On Pi, run directly or via mybot-health.service.
#   Exit 0 = all checks passed. Exit 1 = one or more failed.

set -eo pipefail

PASS=0
FAIL=0
WARN=0

log_pass() { echo "[PASS] $1"; (( PASS++ )) || true; }
log_fail() { echo "[FAIL] $1"; (( FAIL++ )) || true; }
log_warn() { echo "[WARN] $1"; (( WARN++ )) || true; }
log_fix()  { echo "[FIX ] $1"; }
log_info() { echo "[INFO] $1"; }

ensure_service() {
    local svc="$1"
    if systemctl is-active --quiet "$svc"; then
        log_pass "$svc active"
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

# Count messages on a topic using QoS-compatible echo (not ros2 topic hz).
# ros2 topic hz has a QoS subscriber mismatch that produces false low readings
# (e.g. /odom showed ~1.2 Hz on a healthy 20 Hz EKF — documented 2026-05-30).
#
# Args: topic  window_s  qos (best_effort|reliable)
# QoS compatibility: BEST_EFFORT subscriber works with RELIABLE or BEST_EFFORT
# publishers. RELIABLE subscriber fails against BEST_EFFORT publishers.
# Use best_effort for sensor streams, reliable for EKF/state outputs.
count_topic_msgs() {
    local topic="$1"
    local window_s="$2"
    local qos="${3:-best_effort}"
    timeout "$window_s" ros2 topic echo "$topic" \
        --field header.stamp.sec \
        --qos-reliability "$qos" 2>/dev/null \
        | grep -cE '^[[:space:]]*[0-9]+$' || true
}

check_topic_rate() {
    local topic="$1"
    local min_msgs="$2"
    local window_s="$3"
    local qos="$4"
    local label="$5"

    local count
    count=$(count_topic_msgs "$topic" "$window_s" "$qos")
    local hz=$(( count / window_s ))

    if [[ "$count" -ge "$min_msgs" ]]; then
        log_pass "$label: ${count} msgs/${window_s}s (~${hz} Hz) [qos:${qos}]"
    elif [[ "$count" -gt 0 ]]; then
        log_fail "$label: ${count} msgs/${window_s}s — need ≥${min_msgs} [qos:${qos}]"
    else
        log_fail "$label: no messages in ${window_s}s [qos:${qos}]"
    fi
}

check_topic_alive() {
    local topic="$1"
    local timeout_s="$2"
    local qos="$3"
    local label="$4"

    if timeout "$timeout_s" ros2 topic echo "$topic" --once \
            --qos-reliability "$qos" 2>/dev/null | grep -q '.'; then
        log_pass "$label: alive [qos:${qos}]"
    else
        log_fail "$label: no message in ${timeout_s}s [qos:${qos}]"
    fi
}

# ─────────────────────────────────────────────────────────────────────────────
# 1. Filesystem
# ─────────────────────────────────────────────────────────────────────────────
echo "=== [1] Filesystem ==="
if touch /tmp/.mybot_rw_check 2>/dev/null; then
    rm -f /tmp/.mybot_rw_check
    log_pass "Root filesystem read-write"
else
    log_fix "Root filesystem read-only — remounting rw"
    ROOT_DEV=$(findmnt -n -o SOURCE /)
    if sudo mount -o remount,rw "$ROOT_DEV" / 2>/dev/null; then
        log_pass "Root filesystem remounted read-write"
    else
        log_fail "Could not remount root read-write ($ROOT_DEV)"
    fi
fi

# ─────────────────────────────────────────────────────────────────────────────
# 2. Services
# ─────────────────────────────────────────────────────────────────────────────
echo ""
echo "=== [2] Services ==="
ensure_service microros-agent.service
ensure_service mybot-battery.service
ensure_service mybot-display.service
ensure_service robot-launch.service

# ─────────────────────────────────────────────────────────────────────────────
# 3. INA219 battery voltage
# ─────────────────────────────────────────────────────────────────────────────
echo ""
echo "=== [3] INA219 battery voltage ==="
BAT_RESULT=$(python3 - <<'PYEOF'
try:
    from ina219 import INA219
    ina = INA219(0.1, busnum=1)
    # Must specify gain explicitly — default GAIN_AUTO uses GAIN_1_40MV (0.4A max)
    # which overflows immediately on a loaded logic rail, producing 32.76V / NaN.
    ina.configure(
        voltage_range=INA219.RANGE_32V,
        gain=INA219.GAIN_8_320MV,
    )
    v = ina.voltage()
    i = ina.current() / 1000.0
    print(f"voltage={v:.2f} current={i:.3f}")
except Exception as e:
    print(f"ERROR {e}")
PYEOF
)

if echo "$BAT_RESULT" | grep -q "^ERROR"; then
    log_fail "INA219 read failed: $BAT_RESULT"
elif echo "$BAT_RESULT" | grep -q "voltage="; then
    VOLTAGE=$(echo "$BAT_RESULT" | grep -oP 'voltage=\K[0-9.]+')
    CURRENT=$(echo "$BAT_RESULT" | grep -oP 'current=\K[-0-9.]+')
    if awk "BEGIN { exit !($VOLTAGE >= 9.0 && $VOLTAGE <= 16.0) }"; then
        log_pass "INA219: ${VOLTAGE} V / ${CURRENT} A"
    elif awk "BEGIN { exit !($VOLTAGE >= 1.0 && $VOLTAGE < 9.0) }"; then
        log_warn "INA219: ${VOLTAGE} V — low battery or no battery rail (Pi USB-C only?)"
    elif awk "BEGIN { exit !($VOLTAGE < 1.0) }"; then
        log_warn "INA219: ${VOLTAGE} V — no battery rail (expected if Pi powered via USB-C)"
    else
        log_fail "INA219: ${VOLTAGE} V — out of range (OVF? check INA219 wiring)"
    fi
else
    log_fail "INA219: unexpected output: $BAT_RESULT"
fi

# ─────────────────────────────────────────────────────────────────────────────
# 4. ROS environment
# ─────────────────────────────────────────────────────────────────────────────
echo ""
echo "=== [4] ROS topics ==="
source /opt/ros/jazzy/setup.bash
source /home/ubuntu/microros_ws/install/setup.bash
source /home/ubuntu/bot_ws/install/setup.bash

# Wait briefly for EKF to settle after services start
sleep 3

# ESP32 micro-ROS topics — RELIABLE publisher, 30 Hz target.
# Require ≥100 msgs in 5s (~20 Hz floor — allows some USB CDC jitter).
check_topic_rate /diff_cont/odom 100 5 best_effort "/diff_cont/odom (ESP32 odom ~30 Hz)"
check_topic_rate /imu/imu        100 5 best_effort "/imu/imu (ESP32 IMU ~30 Hz)"

# LiDAR — BEST_EFFORT publisher, ~6 Hz target. Require ≥15 msgs in 5s (~3 Hz floor).
check_topic_rate /scan 15 5 best_effort "/scan (LiDAR ~6 Hz)"

# EKF filtered odometry — RELIABLE publisher, 20 Hz target. Require ≥70 msgs in 5s.
# Use reliable QoS to match the EKF publisher. Use echo-count, NOT ros2 topic hz
# (ros2 topic hz /odom showed false ~1.2 Hz on a healthy 20 Hz EKF — QoS artifact).
check_topic_rate /odom 70 5 reliable "/odom (EKF filtered ~20 Hz)"

# Battery state — RELIABLE publisher, 1 Hz. Just check alive.
check_topic_alive /battery_state 5 reliable "/battery_state"

# ─────────────────────────────────────────────────────────────────────────────
# 5. TF transforms
# ─────────────────────────────────────────────────────────────────────────────
echo ""
echo "=== [5] TF transforms ==="
TF_RESULT=$(python3 - <<'PYEOF'
import sys, time
try:
    import rclpy
    from rclpy.node import Node
    from tf2_ros import Buffer, TransformListener

    rclpy.init()
    node = Node('health_tf_check')
    buf = Buffer()
    lst = TransformListener(buf, node)

    # Spin briefly to populate TF buffer
    deadline = time.time() + 3.0
    while time.time() < deadline:
        rclpy.spin_once(node, timeout_sec=0.1)

    checks = [
        ('odom',      'base_footprint', 'EKF publishing odom→base_footprint'),
        ('base_link', 'laser',          'URDF laser frame'),
        ('base_link', 'imu_link',       'URDF imu_link frame'),
    ]
    for src, tgt, label in checks:
        try:
            buf.lookup_transform(src, tgt, rclpy.time.Time())
            print(f"OK {label} ({src} → {tgt})")
        except Exception as e:
            print(f"FAIL {label} ({src} → {tgt}): {e}")

    node.destroy_node()
    rclpy.shutdown()
except Exception as e:
    print(f"FAIL TF check error: {e}")
    sys.exit(0)
PYEOF
)

while IFS= read -r line; do
    if [[ "$line" == OK* ]]; then
        log_pass "${line#OK }"
    elif [[ "$line" == FAIL* ]]; then
        log_fail "${line#FAIL }"
    else
        log_info "$line"
    fi
done <<< "$TF_RESULT"

# ─────────────────────────────────────────────────────────────────────────────
# 6. Odom covariance (firmware verification)
# ─────────────────────────────────────────────────────────────────────────────
echo ""
echo "=== [6] Odom covariance ==="
COV_RESULT=$(python3 - <<'PYEOF'
import sys, subprocess, yaml

try:
    out = subprocess.run(
        ['ros2', 'topic', 'echo', '/diff_cont/odom', '--once',
         '--qos-reliability', 'best_effort'],
        capture_output=True, text=True, timeout=6,
        env={**__import__('os').environ}
    )
    # Split on --- to get first document only
    doc = out.stdout.split('---')[0]
    msg = yaml.safe_load(doc)
    cov = msg['pose']['covariance']
    yaw_var = cov[35]
    x_var   = cov[0]
    vx_var  = msg['twist']['covariance'][0]
    vyaw_var = msg['twist']['covariance'][35]
    print(f"pose[0]={x_var:.4f} pose[35]={yaw_var:.4f} twist[0]={vx_var:.4f} twist[35]={vyaw_var:.4f}")
    if yaw_var == 0.0:
        print("ZERO_YAW")
    else:
        print("NONZERO")
except Exception as e:
    print(f"ERROR {e}")
PYEOF
)

if echo "$COV_RESULT" | grep -q "NONZERO"; then
    VALS=$(echo "$COV_RESULT" | grep "pose\[0\]")
    log_pass "Odom covariance nonzero: $VALS"
elif echo "$COV_RESULT" | grep -q "ZERO_YAW"; then
    VALS=$(echo "$COV_RESULT" | grep "pose\[0\]")
    log_fail "Odom covariance yaw (pose[35]) is zero — old firmware still running. Re-flash with flash_esp32.sh"
    log_info "  $VALS"
elif echo "$COV_RESULT" | grep -q "^ERROR"; then
    log_warn "Could not read odom covariance: $COV_RESULT"
else
    log_warn "Odom covariance check inconclusive: $COV_RESULT"
fi

# ─────────────────────────────────────────────────────────────────────────────
# Summary
# ─────────────────────────────────────────────────────────────────────────────
echo ""
echo "════════════════════════════════════════════════"
echo "  Health check: ${PASS} passed  ${FAIL} failed  ${WARN} warnings"
if [[ $FAIL -eq 0 && $WARN -eq 0 ]]; then
    echo "  STATUS: OK — Phase 3 stack healthy"
    exit 0
elif [[ $FAIL -eq 0 ]]; then
    echo "  STATUS: OK with warnings — review above"
    exit 0
else
    echo "  STATUS: DEGRADED — ${FAIL} check(s) failed (see above)"
    exit 1
fi
