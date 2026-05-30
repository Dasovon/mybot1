# INA219 — Battery Monitor

> **Architecture change:** The INA219 is wired to the **Raspberry Pi**, not the ESP32.
> See [`ina219_pi_battery_monitor.md`](ina219_pi_battery_monitor.md) for the current wiring, software, and ROS integration.

## Why it moved

Battery monitoring was originally planned for the ESP32 I2C bus (GPIO 8/9). That approach was abandoned because:

- The ESP32's job is motor control, encoders, IMU, and micro-ROS transport. Battery monitoring is a higher-level concern.
- Putting the INA219 on the Pi allows `/battery_state` to be published by a proper ROS 2 node with full lifecycle management, without adding complexity to the firmware.
- The Pi INA219 is available from boot, independent of the micro-ROS session state, so the display and health check always have battery data.

## Current architecture

| Unit | Host | Bus | Address | Publisher | Topic |
|---|---|---|---|---|---|
| INA219 | Raspberry Pi | I2C-1 (GPIO 2/3) | 0x40 | `battery_publisher` node (`esp32_serial_bridge`) | `/battery_state` at 1 Hz |

## ESP32 I2C bus (GPIO 8/9)

Only the BNO055 IMU remains on the ESP32 I2C bus:

| Address | Device |
|---|---|
| 0x28 | BNO055 IMU |
| 0x76 | BME680 environmental sensor (Phase 6, not yet wired) |
