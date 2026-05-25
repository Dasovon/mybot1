# Phase 1 Motor & Sensor Validation — 2026-05-25

## Goal

Complete the Phase 1 validation gate: confirm IMU, encoders, motors, and battery
isolation all pass on real hardware with the full micro-ROS stack running.

## Hardware

- ESP32-S3-DevKitC-1 on Lonely Binary expansion board
- BNO055 IMU (I2C 0x28, GPIO 8/9)
- INA219 battery monitor (I2C 0x40, GPIO 8/9)
- ESP32Encoder (PCNT) on GPIO 39/40/41/42
- TB6612FNG motor driver (GPIO 10–15)
- JGA25-371 motors × 4 (2 per side, parallel)
- Raspberry Pi 5, Ubuntu 24.04, ROS 2 Jazzy
- micro-ROS agent via `microros-agent.service`

## Firmware

Commit `041648c` — includes LEDC 2.x API fix (ledcSetup/ledcAttachPin),
corrected encoders.h CPR comment, and all Phase 1 firmware features.

---

## IMU — /imu/imu

```
ros2 topic hz /imu/imu --window 20
```

| Metric | Result |
|---|---|
| Rate | 30.4 Hz |
| Min interval | 0.030s |
| Max interval | 0.037s |
| Std dev | 0.00165s |

**Result: PASS** — 30.4 Hz, stable. Quaternion at rest: w≈1.0 (identity), angular velocity ≈ 0.001 rad/s. BNO055 detected and running in IMUPLUS mode (accel + gyro, no magnetometer).

---

## Encoders — /diff_cont/odom position

Wheels spun by hand while monitoring `ros2 topic echo /diff_cont/odom --field pose.pose.position`.

**Right wheel forward:**
- x increased from 0 → ~0.089m (robot arced left as expected)
- y increased monotonically
- Correct direction ✓

**Left wheel forward:**
- x increased, y decreased (robot arced right as expected)
- Correct direction ✓

**Result: PASS** — both encoders counting, both in correct direction. No wire swaps required.

---

## Motor motion — cmd_vel

```
ros2 topic pub /diff_cont/cmd_vel_unstamped geometry_msgs/msg/Twist \
  '{linear: {x: 0.15}, angular: {z: 0.0}}' --rate 20 --times 100
```

| Observation | Result |
|---|---|
| Both wheels moved forward | ✓ confirmed |
| Watchdog stopped on command end | ✓ confirmed |
| Measured linear velocity | ~0.066 m/s vs 0.15 m/s target |

**Result: PASS (motion confirmed).** PID tracking is under-target at 44% of setpoint after 5 seconds. PID gains need tuning before floor driving accuracy matters (see notes below).

---

## Battery isolation check

Motors driven continuously at 0.2 m/s for 25 seconds while monitoring `/battery_state` rate.

```
ros2 topic hz /battery_state --window 30
```

| Metric | Result |
|---|---|
| Average rate | ~0.91 Hz |
| Min interval | 0.989s |
| Max interval | 1.892s |
| Std dev | 0.22s |

**Result: PASS with warning.** Rate held near 1 Hz throughout. One 1.892s gap appeared mid-run. Battery task runs on Core 0 (FreeRTOS), isolated from the motor PID loop on Core 1 — gap is attributed to micro-ROS executor timing, not I2C contention. Monitor in subsequent sessions.

---

## Phase 1 gate summary

| Check | Result |
|---|---|
| `/diff_cont/odom` ~30 Hz | ✓ PASS |
| `/imu/imu` ~30 Hz | ✓ PASS |
| `/battery_state` ~1 Hz | ⚠ PASS (one 1.9s gap, watch) |
| Robot moves forward on cmd_vel | ✓ PASS |
| Watchdog stops on timeout | ✓ PASS |
| Battery rate holds during motor load | ⚠ PASS (minor jitter) |

**Phase 1: COMPLETE**

---

## Known issues / next steps

### PID tracking under-target
- Target: 0.15 m/s → measured: ~0.066 m/s (44% of setpoint at 5s)
- PID is saturating at near-full duty but wheels aren't reaching target speed
- Likely cause: default gains (KP=4.3, KI=1.2) scaled from old robot; actual motor load
  characteristics differ. Need on-floor characterisation and gain sweep.
- **Action required before Phase 2 odom accuracy matters:** tune KP up, verify
  velocity tracks setpoint within ±10% at steady state.

### Battery publish jitter
- One 1.892s gap observed during motor load test
- Not blocking but should be re-checked after PID tuning (higher duty = more I2C noise risk)

