# ESP32-S3 — Motion Controller

## Role in This Project

The ESP32-S3 is the embedded base controller. It runs independently of ROS and the development PC and is responsible for all real-time and safety-critical operations.

---

## Responsibilities

| Domain | Details |
|---|---|
| Motor control | PWM output via TB6612FNG |
| Encoder reading | Hardware interrupt-driven tick counting |
| PID velocity control | Closed-loop wheel velocity (rad/s) |
| IMU reading | BNO055 via I2C |
| Battery monitoring | INA219 via I2C |
| Environmental sensing | BME680 via I2C |
| Safety watchdog | Motor stop on comms timeout |
| Telemetry | ASCII serial output to Raspberry Pi |

---

## Key Specs

| Property | Value |
|---|---|
| MCU | Xtensa LX7 dual-core, up to 240 MHz |
| Flash | 8 MB (module dependent) |
| PSRAM | 2–8 MB (module dependent) |
| Wi-Fi | 802.11 b/g/n (not used in base config) |
| Bluetooth | BLE 5.0 (not used in base config) |
| I2C | Hardware I2C, hosts BNO055 / INA219 / BME680 |
| UART | USB CDC for serial bridge to Pi |
| GPIO | 45 usable pins |

---

## I2C Bus (ESP32-S3 hosted)

```
ESP32-S3 I2C
├── BNO055  — IMU          (addr: 0x28 or 0x29)
├── INA219  — Battery      (addr: 0x40–0x4F configurable)
└── BME680  — Environment  (addr: 0x76 or 0x77)
```

Pull-up resistors (4.7 kΩ) required on SDA and SCL lines.

---

## Motor Control Pins (TB6612FNG)

| Signal | Description |
|---|---|
| AIN1, AIN2 | Left motor direction |
| BIN1, BIN2 | Right motor direction |
| PWMA | Left motor speed |
| PWMB | Right motor speed |
| STBY | Standby (pull HIGH to enable) |

---

## Encoder Pins

Quadrature encoders connected to GPIO pins supporting hardware interrupts. Use `attachInterrupt()` on both A and B channels per wheel for direction-aware counting.

---

## Firmware Timing Loops

| Loop | Rate | Tasks |
|---|---|---|
| Fast | 100 Hz | Encoder update, PID calculation, PWM output |
| Medium | 50 Hz | IMU polling, velocity telemetry |
| Slow | 1–5 Hz | INA219 read, BME680 read, battery/env telemetry |
| Safety | Continuous | Watchdog timeout check, emergency stop |

---

## Serial Protocol

**Connection:** USB CDC (appears as `/dev/ttyUSBx` or `/dev/ttyACMx` on Pi)
**Baud rate:** 115200 (recommended)
**Format:** ASCII, newline-terminated

### Receive (from Pi)

```
V <left_rad_s> <right_rad_s>    # velocity command
STOP                             # immediate stop
PING                             # keepalive / health check
```

### Transmit (to Pi)

```
ENC <left_ticks> <right_ticks>
VEL <left_rad_s> <right_rad_s>
IMU <roll> <pitch> <yaw>
BAT <voltage> <current> <power>
ENV <temp> <humidity> <pressure> <gas>
ERR <code>
```

---

## Safety Rules

- The watchdog must stop motors if no `V` or `PING` command is received within the timeout window (e.g., 500 ms).
- Battery voltage below cutoff threshold must stop motors immediately, independent of ROS.
- The safety loop runs regardless of serial connectivity.

---

## Development Notes

- Use Arduino framework (ESP-IDF is an option for advanced use).
- Firmware lives in `firmware/esp32/`.
- Flash via USB; no special programmer required.
- Serial monitor at 115200 baud for debugging.
