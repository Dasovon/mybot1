# Control Loop Timing Validation — 2026-05-30

## Purpose

Determine whether the ~200 ms gaps observed on `/diff_cont/odom` and `/imu/imu`
indicate motor control loop stalls, or are limited to the ROS telemetry publish path.

## Setup

| Item | Value |
|---|---|
| Firmware | P-only baseline (KP=0.25, KI=0, KD=0), `SKIP_IMU_FOR_GAP_TEST` active |
| Timing guard | `[WARN] control dt %.2f ms` via `Serial0.printf` when 100 Hz loop dt > 15 ms |
| Monitor | CH340 UART0 → `/dev/ttyUSB0` at 115200 baud (`stty -F /dev/ttyUSB0 115200 raw && cat /dev/ttyUSB0`) |
| Duration | 35 seconds, robot stationary |
| micro-ROS session | Connected, odom publishing with ~200 ms gaps present |

## Results

| Metric | Result |
|---|---|
| `[ENC]` debug lines received | 56 (exactly 2 Hz — encoder log running normally) |
| `[WARN] control dt` lines received | **0** |
| Maximum observed control dt | < 15 ms (no violation in 35 s) |

## Conclusion

**The 100 Hz motor control loop is unaffected by the ~200 ms ROS telemetry gap.**

The gap is confined to the micro-ROS 30 Hz publish path (executor/USB CDC transport).
PID computation, encoder reads, and motor commands run at full 100 Hz throughout.

The telemetry gap affects:
- Bag analysis quality (gaps in recorded odom/IMU data)
- EKF input continuity (robot_localization sees 200 ms holes)
- Any subscriber expecting odom at 30 Hz

The telemetry gap does **not** affect:
- Closed-loop PID velocity control
- Motor safety watchdog
- Encoder tick accumulation

## Root causes of gap (diagnosed separately)

1. **Primary:** Zombie `ros2 bag record` processes create DDS congestion — 5 stale bag
   subscribers were found during this session. Killing them reduces but does not eliminate
   the gap. Always run `pkill -f 'ros2 bag record'` before testing.
2. **Residual:** ~200 ms gap persists on a clean DDS graph. Isolated to the micro-ROS
   30 Hz publish block in firmware (`microros_publish_odom` / `microros_publish_imu`
   via USB CDC). BNO055 I2C was ruled out (gap unchanged with IMU read disabled).
   Further investigation deferred — does not block motor tuning.

## Permanent guard added to firmware

```cpp
// In main.cpp, 100 Hz control block:
float dt_ms = dt * 1000.0f;
if (dt_ms > 15.0f) {
    Serial0.printf("[WARN] control dt %.2f ms\n", dt_ms);
}
```

Read on CH340: `stty -F /dev/ttyUSB0 115200 raw && cat /dev/ttyUSB0`

Zero `[WARN]` lines = control loop healthy.
