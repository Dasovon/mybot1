# Hardware Constants — mybot1

**Do not change any value in this file without explicit user instruction. These are validated hardware values.**

## ESP32-S3 GPIO Map

| GPIO | Function |
|---|---|
| 8 | I2C SDA — BNO055 (0x28), INA219 (0x40), BME680 (0x76 planned) |
| 9 | I2C SCL |
| 10 | PWM_R — Right side speed (LEDC ch 0, 20 kHz, 8-bit) → Cytron MDD10A Ch1 |
| 11 | DIR_R — Right side direction → Cytron MDD10A Ch1 |
| 12 | PWM_L — Left side speed (LEDC ch 1, 20 kHz, 8-bit) → Cytron MDD10A Ch2 |
| 13 | DIR_L — Left side direction → Cytron MDD10A Ch2 |
| 14 | (free) |
| 15 | (free) |
| 17 | UART1 TX → USB-UART adapter → Pi `/dev/ttyUSB0` (micro-ROS Serial1) |
| 18 | UART1 RX ← USB-UART adapter ← Pi `/dev/ttyUSB0` (micro-ROS Serial1) |
| 19, 20 | Native USB D−/D+ — Serial0 display telemetry CDC to Pi `/dev/ttyACM0` |
| 39 | Right encoder B |
| 40 | Left encoder A ⚠️ EMI — 100 nF cap to GND required |
| 41 | Left encoder B ⚠️ EMI — 100 nF cap to GND required |
| 42 | Right encoder A |

**Right side (Ch1) = front_right + rear_right motors in parallel. Left side (Ch2) = front_left + rear_left motors in parallel.**

GPIOs to avoid: 4,5,6,7 (not broken out), 25,26,27,32,33 (not broken out), 35/36/37 (flash), 38 (RGB LED), 43/44 (UART0), 0/45/46 (strapping).

## I2C Addresses (ESP32 I2C bus, GPIO 8/9)

| Device | Address |
|---|---|
| BNO055 IMU | 0x28 |
| INA219 battery monitor | 0x40 |
| BME680 env sensor | 0x76 (not yet wired) |

## Encoder constants (validated on floor)

| Constant | Value |
|---|---|
| `ENC_CPR` | 1010 (2× quadrature, 45:1 gear ratio) |
| `wheel_radius` | 0.03414 m (measured: 68.27 mm dia) |
| `wheel_separation` | 0.177 m (measured center-to-center) |

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
| RealSense depth + color | 15 Hz |
| EKF output (`/odom`) | 20 Hz |
| Nav2 controller | 20 Hz |
| Battery publish | 1 Hz |
| Display daemon | 2 Hz |

## TF2 frame IDs (exact strings — never rename)

| Frame | Description |
|---|---|
| `map` | SLAM output |
| `odom` | EKF output |
| `base_link` | Robot body origin |
| `laser` | RPLidar A1 |
| `imu_link` | BNO055 |
| `camera_link` | RealSense D435 |
| `camera_depth_frame` | RealSense depth frame |
| `front_left_wheel` | Front left drive wheel |
| `front_right_wheel` | Front right drive wheel |
| `rear_left_wheel` | Rear left drive wheel |
| `rear_right_wheel` | Rear right drive wheel |

## micro-ROS stable device path

```
/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_58:E6:C5:5C:23:1C-if00
```

Required firmware build flag: `-DARDUINO_USB_CDC_ON_BOOT=1`
