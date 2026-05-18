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

## Wiring — Confirmed Pin Assignments

**Board:** ESP32-S3-DevKitC-1 on ESP32-S3 expansion base.
All GPIO is 3.3V logic. 3V3 pin powers TB6612 logic, BNO055, INA219, and BME680 directly — no level shifter needed.

### I2C Bus — GPIO 8 / 9

```
SDA → GPIO 8
SCL → GPIO 9

ESP32-S3 I2C Bus
├── BNO055  (addr 0x28 — ADR pin unconnected)
├── INA219  (addr 0x40 — A0/A1 unconnected)
└── BME680  (addr 0x76 — SDO to GND)  ← new sensor, not yet wired
```

The Adafruit BNO055 and INA219 breakouts both have onboard pull-ups — no external pull-ups needed on this bus.

### TB6612FNG Motor Driver — GPIO 10–15

Motor A (PWMA/AIN1/AIN2) = **RIGHT** | Motor B (PWMB/BIN1/BIN2) = **LEFT**

| ESP32 GPIO | TB6612 Pin | Function | LEDC |
|---|---|---|---|
| GPIO 10 | PWMA | Right motor speed (PWM) | ch 0 |
| GPIO 11 | AIN1 | Right motor direction A | — |
| GPIO 12 | AIN2 | Right motor direction B | — |
| GPIO 13 | PWMB | Left motor speed (PWM) | ch 1 |
| GPIO 14 | BIN1 | Left motor direction A | — |
| GPIO 15 | BIN2 | Left motor direction B | — |
| 3V3 | VCC | TB6612 logic supply | — |
| GND | GND | Common ground | — |

STBY → **not wired** — Adafruit breakout has onboard 10 kΩ pull-up (defaults HIGH = enabled).

PWM config: `ledcSetup(ch, 1000, 8)` — 1 kHz, 8-bit (0–255).

### Wheel Encoders — GPIO 39–42

Pins configured `INPUT_PULLUP`. Interrupt fires on CHANGE of the A channel only.

| ESP32 GPIO | Signal | ISR role |
|---|---|---|
| GPIO 40 | Left encoder A | `attachInterrupt` CHANGE |
| GPIO 41 | Left encoder B | Read in ISR |
| GPIO 42 | Right encoder A | `attachInterrupt` CHANGE |
| GPIO 39 | Right encoder B | Read in ISR |

ISR direction logic:
- Left: `A == B on CHANGE` → forward (+)
- Right: `A != B on CHANGE` → forward (+)

Constants: `ENC_CPR = 1010`, `wheel_radius = 0.034 m`, `wheel_separation = 0.179 m`

### micro-ROS Serial Transport — GPIO 19 / 20 (native USB)

ESP32-S3 native USB HWCDC → USB cable → Raspberry Pi 5 `/dev/ttyACM0`

Stable by-id path: `/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_58:E6:C5:5C:23:1C-if00`

Required build flag: `-DARDUINO_USB_CDC_ON_BOOT=1`

Wi-Fi used only for OTA flashing and TelnetStream monitoring — **not** for micro-ROS.

### Common Ground

ESP32 GND → Battery −, Pi GND, TB6612 GND, encoder GND, sensor GND — all on one rail.

### Pins to Avoid (ESP32-S3 expansion base)

| GPIO | Reason |
|---|---|
| 4, 5, 6, 7 | Not broken out on ESP32-S3 expansion board |
| 19, 20 | Native USB D−/D+ — HWCDC transport |
| 25, 26, 27, 32, 33 | Not broken out on ESP32-S3 expansion board |
| 35, 36, 37 | Internal SPI flash/PSRAM — do not use |
| 38 | Onboard RGB LED |
| 43, 44 | UART0 TX/RX — not broken out |
| 0, 45, 46 | Strapping pins — state matters at boot |

> **Known EMI issue:** GPIO 40/41 (left encoder) pick up TB6612 1 kHz PWM noise.
> `VEL_ALPHA = 0.2` EMA filter attenuates it in firmware.
> Permanent fix: solder 100 nF ceramic caps from GPIO 40 → GND and GPIO 41 → GND at ESP32 headers.

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
