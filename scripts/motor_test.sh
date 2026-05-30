#!/usr/bin/env bash
# motor_test.sh — safe ROS velocity test with mandatory bag capture
#
# Usage (run on Pi):
#   ./motor_test.sh [vx_m_s] [duration_s]
#   ./motor_test.sh 0.10 8       # 0.10 m/s for 8 s (default)
#
# What it does:
#   1. Starts bag recording all required topics
#   2. Publishes drive cmd_vel IN THE FOREGROUND (blocks until done)
#   3. Sends explicit stop for 1 second IN THE FOREGROUND
#   4. Stops bag cleanly
#
# SAFETY: no background cmd_vel publishers. Ever.

set -e

source /opt/ros/jazzy/setup.bash
source ~/bot_ws/install/setup.bash

VX=${1:-0.10}
DURATION=${2:-8}
RATE=20
TIMES=$(echo "$DURATION * $RATE" | bc)
STOP_TIMES=20   # 1 second of stop pulses at 20 Hz

BAG_DIR=~/test_logs/test_$(date +%Y%m%d_%H%M%S)

echo "=== motor_test.sh ==="
echo "  vx=${VX} m/s  duration=${DURATION}s  (${TIMES} publishes at ${RATE} Hz)"
echo "  bag: ${BAG_DIR}"
echo ""

# 1. Start bag in background (bag is not a cmd_vel publisher — this is safe)
ros2 bag record \
  /diff_cont/cmd_vel_unstamped \
  /diff_cont/odom \
  /imu/imu \
  /battery_state \
  -o "${BAG_DIR}" &
BAG_PID=$!
echo "[1] Bag started (pid ${BAG_PID})"
sleep 2   # let bag subscribe to all topics

# 2. Drive — FOREGROUND, blocks until --times completes
echo "[2] Driving at ${VX} m/s for ${DURATION}s..."
ros2 topic pub --times "${TIMES}" --rate "${RATE}" \
  /diff_cont/cmd_vel_unstamped \
  geometry_msgs/msg/Twist \
  "{linear: {x: ${VX}}, angular: {z: 0.0}}"

echo "[2] Drive complete."

# 3. Stop — FOREGROUND repeated --once for 1 second
echo "[3] Sending explicit stop..."
for i in $(seq 1 ${STOP_TIMES}); do
  ros2 topic pub --once \
    /diff_cont/cmd_vel_unstamped \
    geometry_msgs/msg/Twist \
    "{linear: {x: 0.0}, angular: {z: 0.0}}"
  sleep 0.05
done
echo "[3] Stop sent."

# 4. Stop bag
kill -SIGINT "${BAG_PID}" 2>/dev/null
wait "${BAG_PID}" 2>/dev/null
echo "[4] Bag saved to: ${BAG_DIR}"
echo ""
echo "=== CONFIRM ROBOT IS STATIONARY BEFORE PROCEEDING ==="
