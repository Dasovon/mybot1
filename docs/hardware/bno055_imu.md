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

## Breakout Pinout

**Breakout board:** Adafruit BNO055 (product #2472) — has onboard level shifter and 10 kΩ I2C pull-ups. No external pull-ups needed on this bus.

| BNO055 Pin | Description |
|---|---|
| VIN | Power 3.3–5V (onboard regulator) |
| 3VO | 3.3V output (~50 mA available) |
| GND | Ground |
| SDA | I2C data (onboard 10 kΩ pull-up) |
| SCL | I2C clock (onboard 10 kΩ pull-up) |
| RST | Hardware reset (active low) |
| INT | Interrupt output (3V logic) |
| ADR | Address select: float/GND = 0x28, HIGH = 0x29 |
| PS0, PS1 | Protocol mode — leave unconnected for I2C |

I2C address in this build: **0x28** (ADR → GND). Shares bus with INA219 (0x40).

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

The ESP32 publishes IMU data at 30 Hz:

```
IMU <roll> <pitch> <yaw>
```

The `esp32_serial_bridge` node converts this to `sensor_msgs/Imu` on the `/imu/imu` topic.

---

## robot_localization Fusion

The `/imu` topic feeds `robot_localization` (EKF node) alongside `/odom` from encoders.

Key parameters in `ekf.yaml`:

```yaml
imu0: /imu/imu
imu0_config: [false, false, false,
              true,  true,  true,
              false, false, false,
              true,  true,  true,
              true,  false, false]
imu0_differential: false
imu0_relative: false
```

---

## ESP32-S3 I2C Compatibility Warning

> ⚠️ **The BNO055 violates the I2C protocol in some circumstances and does not work reliably with all ESP32/ESP32-S3 firmware versions.**
>
> The underlying issue is in the ESP-IDF I2C driver. ESP-IDF **5.3.2 and later** handle the BNO055 quirks correctly. Older versions (ESP-IDF 4.x, used by arduino-esp32 2.x) are unreliable.
>
> **Required:** PlatformIO `platform = espressif32` version **6.x or later** (bundles ESP-IDF 5.x). If I2C hangs or returns garbage on startup, check your arduino-esp32 platform version first.
>
> Symptoms of a version mismatch: I2C timeouts on boot, sensor reads returning 0xFF, or the sensor appearing to initialize but returning junk orientation data.

---

## Common Issues

| Symptom | Likely Cause |
|---|---|
| Heading drift | Magnetometer not calibrated; motor magnetic interference |
| Incorrect orientation | Wrong axis mapping in URDF or node config |
| I2C errors / zeros on boot | ESP-IDF too old — upgrade arduino-esp32 platform to 6.x (IDF 5.x) |
| I2C errors | Missing pull-ups or address conflict |
| Stale data | I2C bus running too slow; check bus speed (400 kHz recommended) |
