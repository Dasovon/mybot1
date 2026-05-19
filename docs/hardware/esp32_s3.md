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

## GPIO Function Map

All GPIO is 3.3V logic. 3V3 pin powers TB6612 logic, BNO055, INA219, and BME680 directly — no level shifter needed.

| GPIO | Function | Notes |
|---|---|---|
| 8 | I2C SDA | BNO055 (0x28), INA219 (0x40), BME680 (0x76 planned) |
| 9 | I2C SCL | Adafruit breakouts have onboard pull-ups — no external needed |
| 10 | PWMA — Right motor speed | LEDC ch 0, 1 kHz, 8-bit |
| 11 | AIN1 — Right motor direction A | Motor A = RIGHT |
| 12 | AIN2 — Right motor direction B | |
| 13 | PWMB — Left motor speed | LEDC ch 1, 1 kHz, 8-bit |
| 14 | BIN1 — Left motor direction A | Motor B = LEFT |
| 15 | BIN2 — Left motor direction B | |
| 19, 20 | Native USB D−/D+ | HWCDC micro-ROS transport to Pi |
| 39 | Right encoder B | Read in ISR |
| 40 | Left encoder A | `attachInterrupt` CHANGE ⚠️ EMI |
| 41 | Left encoder B | Read in ISR ⚠️ EMI |
| 42 | Right encoder A | `attachInterrupt` CHANGE |

ISR direction logic: Left `A == B on CHANGE` → forward (+) | Right `A != B on CHANGE` → forward (+)

STBY not wired — Adafruit TB6612 breakout has onboard 10 kΩ pull-up (always enabled).

⚠️ **GPIO 40/41 EMI:** Pick up 1 kHz PWM noise from the TB6612. EMA filter `VEL_ALPHA = 0.2` attenuates in firmware. Hardware fix: 100 nF ceramic caps from GPIO 40 → GND and GPIO 41 → GND, placed close to the ESP32 pins.

micro-ROS transport: native USB HWCDC → Pi `/dev/ttyACM0`. Build flag: `-DARDUINO_USB_CDC_ON_BOOT=1`.

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
> Hardware fix: route encoder wires through a breadboard and place 100 nF ceramic caps from GPIO 40 → GND and GPIO 41 → GND on the breadboard before connecting to the ESP32.

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
