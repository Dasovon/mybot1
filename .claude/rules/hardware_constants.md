# Hardware Constants — mybot1

**Do not change GPIO pins, I2C addresses, or mechanical dimensions without explicit user instruction. Encoder CPR is pending re-validation — see encoder constants section.**

## ESP32-S3 GPIO Map

| GPIO | Function |
|---|---|
| 8 | I2C SDA — BNO055 (0x28), BME680 (0x76 planned) |
| 9 | I2C SCL |
| 10 | PWMA — Right side speed (LEDC ch 0, 1 kHz, 8-bit) → TB6612FNG PWMA |
| 11 | AIN1 — Right side direction 1 → TB6612FNG AIN1 |
| 12 | AIN2 — Right side direction 2 → TB6612FNG AIN2 |
| 13 | PWMB — Left side speed (LEDC ch 1, 1 kHz, 8-bit) → TB6612FNG PWMB |
| 14 | BIN1 — Left side direction 1 → TB6612FNG BIN1 |
| 15 | BIN2 — Left side direction 2 → TB6612FNG BIN2 |
| 17 | (free) |
| 18 | (free) |
| 19, 20 | Native USB D−/D+ → Pi `/dev/ttyACM0` — micro-ROS transport + flashing (921600 baud) |
| 43 | UART0 TX via Lonely Binary CH340 → Pi `/dev/ttyUSB0` — debug console (115200 baud) |
| 44 | UART0 RX via Lonely Binary CH340 ← Pi `/dev/ttyUSB0` |
| 39 | Right encoder B |
| 40 | Left encoder A ⚠️ EMI — 100 nF cap to GND required |
| 41 | Left encoder B ⚠️ EMI — 100 nF cap to GND required |
| 42 | Right encoder A |

**Right side = front_right + rear_right motors in parallel. Left side = front_left + rear_left motors in parallel. TB6612FNG is temporary — future upgrade to larger driver + 2 more wheels planned.**

GPIOs to avoid: 4,5,6,7 (not broken out), 25,26,27,32,33 (not broken out), 35/36/37 (flash), 38/48 (RGB LED), 43/44 (UART0/CH340 — debug console), 0/45/46 (strapping).

## I2C Addresses (ESP32 I2C bus, GPIO 8/9)

| Device | Address |
|---|---|
| BNO055 IMU | 0x28 |
| BME680 env sensor | 0x76 (not yet wired) |

## I2C Addresses (Pi I2C-1 bus, GPIO 2/3)

| Device | Address |
|---|---|
| INA219 battery monitor | 0x40 |

## Encoder constants

| Constant | Status | Notes |
|---|---|---|
| `ENC_CPR` | **Pending re-validation** | Firmware uses `990`; historical docs say `1010`. Protocol: 5 rev/trial × 3 trials/wheel, raw count only, CPR = |Δcount| / 5. Left Trial 1 = 987.0 CPR (9870 / 10 rev, valid and comparable). Do not update until all 6 trials reviewed. |
| `wheel_radius` | `0.03414 m` | Measured: 68.27 mm diameter |
| `wheel_separation` | `0.177 m` | Measured center-to-center |

## Motor specs (JGA25-371 / 25SG-370CA-45-EN)

| Parameter | Value |
|---|---|
| Voltage | 12V DC |
| Gear ratio | 45:1 |
| No-load speed | ~190 rpm |
| No-load current | < 150 mA |
| Rated speed | ~100 rpm |
| Rated current | < 750 mA |
| Rated torque | 5 kg·cm |
| Stall torque | 9 kg·cm |
| Encoder wire colors | Red/White = motor power, Blue/Black = encoder power, Yellow = Ch A, Green = Ch B |

## Timing requirements

| System | Rate |
|---|---|
| PID / encoder loop | 100 Hz |
| IMU + odom publish (micro-ROS) | 30 Hz |
| LiDAR scan (RPLidar A1) | ~5.5 Hz |
| RealSense depth | 6 Hz (424×240) |
| RealSense color | 15 Hz (424×240) |
| EKF output (`/odom`) | 20 Hz |
| Nav2 controller | 20 Hz |
| Battery publish | 1 Hz |
| Display daemon | 2 Hz |

## TF2 frame IDs (exact strings — never rename)

| Frame | Description |
|---|---|
| `map` | SLAM output |
| `odom` | EKF output |
| `base_footprint` | Robot base — EKF publishes `odom → base_footprint` |
| `base_link` | Robot body origin (fixed joint child of `base_footprint`) |
| `laser` | RPLidar A1 |
| `imu_link` | BNO055 |
| `camera_link` | RealSense D435 |
| `camera_depth_frame` | RealSense depth frame |
| `front_left_wheel` | Front left drive wheel |
| `front_right_wheel` | Front right drive wheel |
| `rear_left_wheel` | Rear left drive wheel |
| `rear_right_wheel` | Rear right drive wheel |

## Serial transport (ESP32 ↔ Pi)

| Role | ESP32 port | Pi device | Notes |
|---|---|---|---|
| micro-ROS + flashing | Native USB CDC, GPIO 19/20 | `/dev/ttyACM0` | Built-in USB-JTAG/Serial (303a:1001); 921600 baud; auto-reset via 1200bps touch |
| Debug console | UART0, GPIO 43/44 via CH340 | `/dev/ttyUSB0` | Firmware state, PID timing, micro-ROS transitions; 115200 baud; read-only from Pi side |

Required firmware build flags: `-DARDUINO_USB_CDC_ON_BOOT=1 -DARDUINO_USB_MODE=1 -DCORE_DEBUG_LEVEL=0`

Note: `-DCORE_DEBUG_LEVEL=0` suppresses Arduino framework chatter on UART0, keeping the CH340 debug stream limited to intentional `Serial0.printf()` output.
