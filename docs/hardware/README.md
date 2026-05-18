# Hardware Documentation Index

| Component | Role | Wiring Status | Doc |
|---|---|---|---|
| RPI5 PD Power Hat | Power distribution (9–24V in → 5V/8A Pi + VIN motor) | Confirmed | [rpi5_pd_power_hat.md](rpi5_pd_power_hat.md) |
| ESP32-S3-DevKitC-1 | Embedded motion controller | Confirmed | [esp32_s3.md](esp32_s3.md) |
| Raspberry Pi 5 | Sensor bridge / ROS interface | Confirmed | [raspberry_pi_5.md](raspberry_pi_5.md) |
| Development PC | SLAM / Nav2 / AI / RViz | Confirmed | [development_pc.md](development_pc.md) |
| Adafruit TB6612FNG | Dual motor driver | Confirmed | [tb6612fng.md](tb6612fng.md) |
| JGA25-371 Wheel Encoders | Odometry — 1010 CPR validated | Confirmed | [wheel_encoders.md](wheel_encoders.md) |
| RPLidar A1 M8 | 2D LiDAR — SLAM geometry | Confirmed | [rplidar.md](rplidar.md) |
| RealSense D435 | RGB-D depth camera | Confirmed | [realsense_d435.md](realsense_d435.md) |
| Adafruit BNO055 | IMU — orientation / heading | Confirmed | [bno055_imu.md](bno055_imu.md) |
| Adafruit INA219 | Battery voltage / current monitor | Confirmed | [ina219_battery_monitor.md](ina219_battery_monitor.md) |
| BME680 | Environmental sensor (temp / humidity / pressure / gas) | Planned — not yet wired | [bme680_environmental.md](bme680_environmental.md) |

## Quick Reference — ESP32-S3 GPIO Map

| GPIO | Function |
|---|---|
| 8 | I2C SDA (BNO055, INA219, BME680) |
| 9 | I2C SCL |
| 10 | PWMA — Right motor speed |
| 11 | AIN1 — Right motor direction A |
| 12 | AIN2 — Right motor direction B |
| 13 | PWMB — Left motor speed |
| 14 | BIN1 — Left motor direction A |
| 15 | BIN2 — Left motor direction B |
| 19, 20 | Native USB D−/D+ (micro-ROS to Pi) |
| 39 | Right encoder B |
| 40 | Left encoder A ⚠️ EMI from PWM |
| 41 | Left encoder B ⚠️ EMI from PWM |
| 42 | Right encoder A |
