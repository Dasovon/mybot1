# IMU Vibration Contamination — Validation Log
**Date:** 2026-05-30
**Phase:** 3 (sensor characterization)
**Robot config:** 2-wheel diff-drive test chassis, BNO055 hard-mounted to chassis

---

## Test

Straight drive at 0.10 m/s for 8 seconds. Full bag captured with `motor_test.sh`.
Bag: `test_20260530_201721`

Compared `odom.twist.angular.z` (encoder-derived) vs `imu.angular_velocity.z` (BNO055 gyro) over the 7.95-second drive window.

---

## Results

| Signal | Mean | Stdev | Min | Max | Spikes > 1 rad/s |
|---|---|---|---|---|---|
| `odom.twist.angular.z` (encoders) | +0.00010 rad/s | 0.0734 | −0.223 | +0.245 | 0 |
| `imu.angular_velocity.z` (BNO055) | +0.1127 rad/s | 2.390 | −5.500 | +11.313 | 111 |

Velocity tracking: mean=0.1001 m/s, error=+0.1% — robot drove mechanically straight.
Distance accuracy: −0.4% over 5.95 s steady-state window.
IMU linear_acceleration.x jerk: 2.49 m/s² peak-to-peak (also contaminated).

---

## Verdict

**BNO055 angular_velocity.z is contaminated by gearbox and motor switching vibration.**

The encoder odometry shows essentially zero yaw rate (mean +0.0001 rad/s, zero spikes above 1 rad/s) — the drive base is mechanically straight. The IMU gyro simultaneously shows 111 spikes above 1 rad/s and a sustained +0.113 rad/s mean bias. This is vibration pickup through the chassis, not real robot rotation.

The drive base **passed**. Motors, PID, and encoder odometry are trustworthy.

---

## Actions Taken

1. **EKF config (`ekf.yaml`):** Disabled `imu0_config[11]` (angular_velocity.z / vyaw). EKF now uses encoder odometry exclusively for yaw. Linear acceleration (ax, ay) retained — real signal at start/stop events even if noisy.

2. **Do not tune motors or PID from IMU yaw or IMU jerk** until BNO055 is mechanically isolated.

---

## Pending

- Mechanical: mount BNO055 on foam/rubber isolation pad, away from motor driver and motor wiring. Keep wiring short/twisted.
- Re-run this same test after isolation and compare IMU gyro z mean and spike count.
- If IMU gyro z cleans up (mean < 0.05 rad/s, spikes < 10 in 8 s), re-enable `imu0_config[11]` and re-validate EKF yaw.
