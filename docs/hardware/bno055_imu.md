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

## Connection to ESP32-S3

| BNO055 Pin | ESP32-S3 Pin | Notes |
|---|---|---|
| VIN | 3.3V | Do not use 5V |
| GND | GND | Shared common ground |
| SDA | I2C SDA | 4.7 kΩ pull-up to 3.3V |
| SCL | I2C SCL | 4.7 kΩ pull-up to 3.3V |
| ADR | GND | Sets I2C address to 0x28 |
| INT | Optional GPIO | Interrupt on data ready |

---

## I2C Address

| ADR Pin | I2C Address |
|---|---|
| GND (default) | 0x28 |
| 3.3V | 0x29 |

If two BNO055 units are needed, use both addresses.

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
