# Hardware Documentation Index

Wiring for each component is determined at the corresponding build gate in `docs/testing/electronics_build_protocol_2026-05-18.md`. See `CLAUDE.md` for the GPIO function map.

| Component | Role | Doc |
|---|---|---|
| RPI5 PD Power Hat | Power distribution — 9–24V in → 5V/8A USB PD + VIN motor | [rpi5_pd_power_hat.md](rpi5_pd_power_hat.md) |
| DFR0205 | Alternative power board — not used in this build | [dfr0205.md](dfr0205.md) |
| ESP32-S3-DevKitC-1 | Embedded motion controller | [esp32_s3.md](esp32_s3.md) |
| Raspberry Pi 5 | Sensor bridge / ROS interface | [raspberry_pi_5.md](raspberry_pi_5.md) |
| Development PC | SLAM / Nav2 / AI / RViz | [development_pc.md](development_pc.md) |
| Adafruit TB6612FNG | Dual motor driver | [tb6612fng.md](tb6612fng.md) |
| JGA25-371 Wheel Encoders | Odometry — 1010 CPR validated | [wheel_encoders.md](wheel_encoders.md) |
| RPLidar A1 M8 | 2D LiDAR — SLAM geometry | [rplidar.md](rplidar.md) |
| RealSense D435 | RGB-D depth camera | [realsense_d435.md](realsense_d435.md) |
| Adafruit BNO055 | IMU — orientation / heading | [bno055_imu.md](bno055_imu.md) |
| Adafruit INA219 | Battery voltage / current monitor | [ina219_battery_monitor.md](ina219_battery_monitor.md) |
| BME680 | Environmental sensor — Phase 6 | [bme680_environmental.md](bme680_environmental.md) |
| Waveshare 2.42" OLED | Status display (SSD1309, 128×64, SPI0 on Pi) — Phase 6 | [oled_display.md](oled_display.md) |
