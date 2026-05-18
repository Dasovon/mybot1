# BNO055 — IMU (Inertial Measurement Unit)

## Role in This Project

The BNO055 provides absolute orientation, heading, and rotation rate. This data is fused with wheel encoder odometry by `robot_localization` to produce the `/odom` topic, improving localization accuracy especially during turns and on uneven surfaces.

---

## Key Specs

| Property | Value |
|---|---|
| Sensor type | 9-DOF (accelerometer + gyroscope + magnetometer) |
| Fusion mode | On-chip sensor fusion (Bosch BSX algorithm) |
| Interface | I2C (primary) or UART |
| I2C address | 0x28 (default) or 0x29 (ADR pin HIGH) |
| Supply voltage | 2.4V – 3.6V (logic); use 3.3V |
| Output | Quaternion, Euler angles, linear acceleration, gravity vector, angular velocity |
| Update rate | Up to 100 Hz |

---

## Wiring — Current Connections (from spec)

Connected to **ESP32-S3 via I2C** (ESP32 hosts the I2C bus).

```
ESP32-S3 I2C Bus
└── BNO055  — IMU
```

Shares common ground with ESP32, TB6612, Pi, and battery −.

> Specific SDA/SCL GPIO pins and I2C address are TBD — to be documented when firmware is written.

---

## Operating Mode

For this project, use **NDOF mode** (Nine Degrees Of Freedom):
- Full sensor fusion
- Absolute orientation (quaternion + Euler)
- Requires magnetometer calibration

Alternative: **IMU mode** (no magnetometer) — use if magnetic interference is a problem near motors.

---

## Calibration

The BNO055 requires calibration for accurate heading:

| Subsystem | Calibration Method |
|---|---|
| Accelerometer | Leave still in 6 orientations |
| Gyroscope | Leave still for a few seconds |
| Magnetometer | Figure-8 motion in the air |
| System | All three above complete |

Calibration status reported as 0–3 (3 = fully calibrated). Store calibration offsets in ESP32 flash to survive reboots.

---

## Telemetry Output

The ESP32 publishes IMU data at 50 Hz:

```
IMU <roll> <pitch> <yaw>
```

The `esp32_serial_bridge` node converts this to `sensor_msgs/Imu` on the `/imu` topic.

---

## robot_localization Fusion

The `/imu` topic feeds `robot_localization` (EKF node) alongside `/odom` from encoders.

Key parameters in `ekf.yaml`:

```yaml
imu0: /imu
imu0_config: [false, false, false,
              true,  true,  true,
              false, false, false,
              true,  true,  true,
              true,  false, false]
imu0_differential: false
imu0_relative: false
```

---

## Common Issues

| Symptom | Likely Cause |
|---|---|
| Heading drift | Magnetometer not calibrated; motor magnetic interference |
| Incorrect orientation | Wrong axis mapping in URDF or node config |
| I2C errors | Missing pull-ups or address conflict |
| Stale data | I2C bus running too slow; check bus speed (400 kHz recommended) |
