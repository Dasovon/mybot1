#!/usr/bin/env bash
# motor_test.sh — safe ROS velocity test with bag capture and inline analysis
#
# Usage (run on Pi):
#   ~/motor_test.sh [vx_m_s] [duration_s]
#   ~/motor_test.sh 0.10 8       # 0.10 m/s for 8 s (default)
#
# Sequence:
#   1. Start bag (sqlite3, detached from shell)
#   2. Drive foreground
#   3. Stop foreground
#   4. Stop bag, verify metadata + sanity gates
#   5. Analyze only if all gates pass
#
# SAFETY: no background cmd_vel publishers. Ever.

source /opt/ros/jazzy/setup.bash
source ~/bot_ws/install/setup.bash

VX=${1:-0.10}
DURATION=${2:-8}
RATE=20
TIMES=$(echo "$DURATION * $RATE" | bc)
STOP_TIMES=20

BAG_DIR=~/test_logs/test_$(date +%Y%m%d_%H%M%S)
mkdir -p ~/test_logs

echo "=== motor_test.sh ==="
echo "  vx=${VX} m/s  duration=${DURATION}s  (~$(echo "$VX * $DURATION" | bc) m travel)"
echo "  bag: ${BAG_DIR}"
echo ""
echo "┌─────────────────────────────────────────────────────┐"
echo "│  PRE-TEST CHECKLIST                                 │"
echo "│                                                     │"
echo "│  1. Place robot at start mark, pointed straight     │"
echo "│  2. Ensure $(echo "$VX * $DURATION" | bc) m of clear space ahead          │"
echo "│  3. Keep hands clear — robot will move on its own   │"
echo "│  4. Emergency stop: Ctrl+C  (watchdog fires in 0.5s)│"
echo "└─────────────────────────────────────────────────────┘"
echo ""
read -r -p "Robot in position and clear? Press Enter to start, Ctrl+C to abort: "
echo ""

# ── 1. Start bag ─────────────────────────────────────────────────────────────
nohup ros2 bag record \
  /diff_cont/cmd_vel_unstamped \
  /diff_cont/odom \
  /imu/imu \
  /battery_state \
  --storage sqlite3 \
  -o "${BAG_DIR}" > /tmp/bag_record.log 2>&1 &
BAG_PID=$!
disown $BAG_PID
echo "[1] Bag started (pid ${BAG_PID})"
sleep 3

# ── 2. Drive ─────────────────────────────────────────────────────────────────
echo "[2] Driving at ${VX} m/s for ${DURATION}s..."
ros2 topic pub --times "${TIMES}" --rate "${RATE}" \
  /diff_cont/cmd_vel_unstamped \
  geometry_msgs/msg/Twist \
  "{linear: {x: ${VX}}, angular: {z: 0.0}}"
echo "[2] Drive complete."

# ── 3. Stop ──────────────────────────────────────────────────────────────────
echo "[3] Sending explicit stop..."
for i in $(seq 1 ${STOP_TIMES}); do
  ros2 topic pub --once \
    /diff_cont/cmd_vel_unstamped \
    geometry_msgs/msg/Twist \
    "{linear: {x: 0.0}, angular: {z: 0.0}}" > /dev/null 2>&1
  sleep 0.05
done
echo "[3] Stop sent."
echo ""
read -r -p "Confirm robot is stationary, then press Enter to analyze: "
echo ""

# ── 4. Stop bag — use stored PID so rosbag can write metadata.yaml cleanly
echo "[4] Stopping bag..."
kill -INT "$BAG_PID" 2>/dev/null || true
wait "$BAG_PID" 2>/dev/null || true

BAG_DB=$(ls "${BAG_DIR}"/*.db3 2>/dev/null | head -1)

# Gate 1: db3 file exists and is non-empty
if [ -z "$BAG_DB" ] || [ ! -s "$BAG_DB" ]; then
  echo "ABORT: bag file missing or empty"
  echo "  recorder log:"
  cat /tmp/bag_record.log
  exit 1
fi
echo "  [gate 1] db3 exists: $(ls -lh $BAG_DB | awk '{print $5}')  — proceeding to data gates"
echo ""

# ── 5. Sanity gates + analysis ────────────────────────────────────────────────
python3 - "${BAG_DB}" "${VX}" "${DURATION}" <<'PYEOF'
import sys, sqlite3, statistics

bag_db   = sys.argv[1]
cmd_vx_t = float(sys.argv[2])
duration = float(sys.argv[3])

from rclpy.serialization import deserialize_message
from rosidl_runtime_py.utilities import get_message

Odometry     = get_message('nav_msgs/msg/Odometry')
Imu          = get_message('sensor_msgs/msg/Imu')
BatteryState = get_message('sensor_msgs/msg/BatteryState')
Twist        = get_message('geometry_msgs/msg/Twist')

conn = sqlite3.connect(bag_db)
topics = {row[1]: row[0] for row in conn.execute("SELECT id, name FROM topics")}

def tid(name): return topics.get(name)

odom_vx, pos_x, ts_odom = [], [], []
imu_wz, imu_ax, ts_imu  = [], [], []
bat_v, bat_i             = [], []
cmd_vx, ts_cmd           = [], []

for topic_id, timestamp, data in conn.execute(
        "SELECT topic_id, timestamp, data FROM messages ORDER BY timestamp"):
    data = bytes(data)
    if topic_id == tid('/diff_cont/odom'):
        m = deserialize_message(data, Odometry)
        odom_vx.append(m.twist.twist.linear.x)
        pos_x.append(m.pose.pose.position.x)
        ts_odom.append(timestamp)
    elif topic_id == tid('/imu/imu'):
        m = deserialize_message(data, Imu)
        imu_wz.append(m.angular_velocity.z)
        imu_ax.append(m.linear_acceleration.x)
        ts_imu.append(timestamp)
    elif topic_id == tid('/battery_state'):
        m = deserialize_message(data, BatteryState)
        bat_v.append(m.voltage)
        bat_i.append(m.current)
    elif topic_id == tid('/diff_cont/cmd_vel_unstamped'):
        m = deserialize_message(data, Twist)
        cmd_vx.append(m.linear.x)
        ts_cmd.append(timestamp)

sep = "─" * 54
print(sep)
print("  BAG SANITY GATES")
print(sep)

abort = False

# Gate 4: cmd_vel count
drive_cmds = [v for v in cmd_vx if v > 0.05]
expected_drive = int(duration * 20)
print(f"  [gate 4] cmd_vel: {len(cmd_vx)} total, {len(drive_cmds)} drive msgs (expected ~{expected_drive})")
if len(drive_cmds) < expected_drive * 0.8:
    print(f"  ABORT: drive cmd count too low — bag may have missed the run")
    abort = True

# Gate 5: duration sanity (odom)
if ts_odom:
    actual_dur = (ts_odom[-1] - ts_odom[0]) / 1e9
    print(f"  [gate 5] odom span: {actual_dur:.1f}s (expected ≥{duration:.0f}s)")
    if actual_dur < duration * 0.8:
        print(f"  ABORT: recorded duration too short")
        abort = True

# Gate 6: timestamps monotonic (odom)
if ts_odom:
    jumps = sum(1 for a, b in zip(ts_odom, ts_odom[1:]) if b <= a)
    print(f"  [gate 6] odom timestamp monotonic: {jumps} violations")
    if jumps > 0:
        print(f"  ABORT: non-monotonic timestamps — bag is corrupted")
        abort = True

# Gate 7: odom message count
expected_odom = int(duration * 30)
print(f"  [gate 7] odom msgs: {len(odom_vx)} (expected ~{expected_odom})")
if len(odom_vx) < expected_odom * 0.5:
    print(f"  ABORT: too few odom samples")
    abort = True

if abort:
    print(f"\n  One or more gates failed — do not tune from this data.")
    sys.exit(1)

print(f"  All sanity gates passed.\n")

# ── Analysis — cmd_vel-gated window ──────────────────────────────────────────
# Gate the analysis on actual drive timestamps, not hardcoded rate assumptions.
# drive_start = first cmd_vel with linear.x > 0
# drive_end   = last  cmd_vel with linear.x > 0
# Odom window = [drive_start + 2s ramp, drive_end]
# Distance    = delta pose over the same window (not pos_x[-1])

NS = 1e9  # nanoseconds per second
RAMP_S = 2.0  # skip first 2 s of drive for ramp-up

drive_ts = [ts for ts, v in zip(ts_cmd, cmd_vx) if v > 0.05]
if not drive_ts:
    print("  ABORT: no drive cmd_vel timestamps found")
    sys.exit(1)

t_drive_start = drive_ts[0] / NS
t_drive_end   = drive_ts[-1] / NS
t_ss_start    = t_drive_start + RAMP_S

# Odom samples inside steady-state window
ss_pairs = [(v, px, ts / NS) for v, px, ts in zip(odom_vx, pos_x, ts_odom)
            if t_ss_start <= ts / NS <= t_drive_end]
ss      = [v  for v, px, t in ss_pairs]
ss_pos  = [px for v, px, t in ss_pairs]

# IMU samples inside full drive window (including ramp — for jerk/yaw)
drive_imu_wz = [v for v, ts in zip(imu_wz, ts_imu)
                if t_drive_start <= ts / NS <= t_drive_end]
drive_imu_ax = [v for v, ts in zip(imu_ax, ts_imu)
                if t_drive_start <= ts / NS <= t_drive_end]

# Battery samples during drive
drive_bat_i = [v for v in bat_i]   # 1 Hz — use all; too few to gate precisely
drive_bat_v = [v for v in bat_v]

actual_drive_s = t_drive_end - t_drive_start
ss_span_s      = t_drive_end - t_ss_start

print(sep)
print("  RESULTS — P-only  KP=%.2f  target=%.2f m/s  duration=%.0fs" % (0.25, cmd_vx_t, duration))
print(f"  drive window: {actual_drive_s:.2f}s  |  steady-state window: {ss_span_s:.2f}s  |  n={len(ss)} odom samples")
print(sep)

# 1. Velocity tracking
if ss:
    m   = statistics.mean(ss)
    std = statistics.stdev(ss) if len(ss) > 1 else 0.0
    err = (m - cmd_vx_t) / cmd_vx_t * 100
    neg = sum(1 for v in ss if v < 0)
    print(f"\n  [1] Velocity tracking (steady-state: drive+2s → drive_end)")
    print(f"      mean={m:.4f} m/s  std={std:.4f}  error={err:+.1f}%")
    print(f"      negative samples: {neg}")
    print(f"      P-only baseline:  mean=0.0952  error=-4.8%")
    print(f"      {'PASS' if abs(err) < 10 and neg == 0 else 'FAIL'}")
else:
    print(f"\n  [1] Velocity tracking: NO SAMPLES in steady-state window")

# 2. Yaw drift
if drive_imu_wz:
    mwz  = statistics.mean(drive_imu_wz)
    pkwz = max(drive_imu_wz, key=abs)
    print(f"\n  [2] Yaw drift (full drive window, n={len(drive_imu_wz)})")
    print(f"      angular_velocity.z  mean={mwz:+.4f}  peak={pkwz:+.4f} rad/s")
    print(f"      {'PASS' if abs(mwz) < 0.05 else 'FAIL'}  (threshold ±0.05 rad/s mean)")

# 3. Jerk / oscillation
if drive_imu_ax:
    pp = max(drive_imu_ax) - min(drive_imu_ax)
    print(f"\n  [3] Jerk / oscillation (full drive window)")
    print(f"      linear_acceleration.x  peak-to-peak={pp:.3f} m/s²")
    print(f"      {'PASS' if pp < 1.0 else 'WARN'}  (threshold 1.0 m/s²)")

# 4. Current draw
if drive_bat_i:
    mi  = statistics.mean(drive_bat_i)
    pki = max(drive_bat_i)
    mv  = statistics.mean(drive_bat_v)
    print(f"\n  [4] Battery under load")
    print(f"      voltage={mv:.2f} V  current mean={mi:.3f} A  peak={pki:.3f} A")
    print(f"      {'PASS' if pki < 2*mi else 'WARN'}  (peak < 2× mean)")

# 5. Distance accuracy — delta pose over steady-state window
if len(ss_pos) >= 2:
    delta_x  = ss_pos[-1] - ss_pos[0]
    expected = cmd_vx_t * ss_span_s
    derr     = (delta_x - expected) / expected * 100 if expected else 0
    print(f"\n  [5] Distance accuracy (delta pose over steady-state window)")
    print(f"      window={ss_span_s:.2f}s  expected={expected:.3f} m  actual Δx={delta_x:.3f} m  error={derr:+.1f}%")
    print(f"      P-only baseline: ~-6.2%")
    print(f"      {'PASS' if abs(derr) < 10 else 'FAIL'}  (threshold ±10%)")

print(f"\n{sep}")
PYEOF
