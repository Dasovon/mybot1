# Wiring Audit — Current State & Open Items

All confirmed data sourced from `docs/hardware/` component docs.

---

## Confirmed Connection Map

### Power

```
Battery (3S LiPo ~12V, or 9–24V DC)
    └── RPI5 PD Power Hat INPUT (DC barrel)
            ├── USB-C OUTPUT (5.15V / 5A, USB PD 3.0) → Raspberry Pi 5
            │       ├── USB-A → ESP32-S3          (power + micro-ROS serial)
            │       ├── USB-A (USB 2.0) → RPLidar A1 M8  (power + data)
            │       └── USB-A (USB 3.0, blue) → RealSense D435  (power + data)
            └── VIN screw terminal (raw battery voltage) → TB6612FNG VM pin

TB6612 logic VCC → ESP32 3V3 pin
Common ground: Battery −, hat GND, Pi GND, ESP32 GND, TB6612 GND — all one rail.
```

### ESP32-S3 GPIO

| GPIO | Signal | Connected to |
|---|---|---|
| 8 | I2C SDA | BNO055 (0x28), INA219 (0x40), BME680 (0x76 — planned) |
| 9 | I2C SCL | BNO055, INA219, BME680 |
| 10 | PWMA | TB6612 PWMA — right motor speed |
| 11 | AIN1 | TB6612 AIN1 — right motor direction A |
| 12 | AIN2 | TB6612 AIN2 — right motor direction B |
| 13 | PWMB | TB6612 PWMB — left motor speed |
| 14 | BIN1 | TB6612 BIN1 — left motor direction A |
| 15 | BIN2 | TB6612 BIN2 — left motor direction B |
| 19, 20 | USB D−/D+ | Native HWCDC → Pi `/dev/ttyACM0` (micro-ROS) |
| 39 | Right encoder B | JGA25-371 right wheel, read in ISR |
| 40 | Left encoder A | JGA25-371 left wheel, `attachInterrupt` CHANGE ⚠️ EMI |
| 41 | Left encoder B | JGA25-371 left wheel, read in ISR ⚠️ EMI |
| 42 | Right encoder A | JGA25-371 right wheel, `attachInterrupt` CHANGE |

**Motor A = RIGHT, Motor B = LEFT.**

### I2C Bus

```
ESP32 GPIO 8 (SDA) / GPIO 9 (SCL)
├── BNO055  addr 0x28  (Adafruit breakout — ADR pin unconnected)
├── INA219  addr 0x40  (Adafruit breakout — A0/A1 unconnected)
└── BME680  addr 0x76  (SDO → GND) — not yet wired
```

Adafruit BNO055 and INA219 breakouts have onboard pull-up resistors — no external pull-ups needed for these two devices. Confirm BME680 breakout has onboard pull-ups before wiring.

### Encoder Wiring (JGA25-371 6-wire harness)

| Wire | Function | Connected to |
|---|---|---|
| Red | Motor + | TB6612 AO1/BO1 |
| White | Motor − | TB6612 AO2/BO2 |
| Blue | Encoder VCC | ESP32 3V3 |
| Black | Encoder GND | Common GND |
| Yellow | Channel A | ESP32 GPIO 40 (left) / 42 (right) |
| Green | Channel B | ESP32 GPIO 41 (left) / 39 (right) |

Encoder output is 3.3V — no level shifter required.

### Common Ground

```
Battery −
├── RPI5 PD Power Hat GND
│       └── Pi GND (via USB PD cable)
├── ESP32 GND
├── TB6612 GND
├── Encoder GND (Blue/Black harness)
└── Sensor GND (BNO055, INA219, BME680)
```

---

## Resolved Items (from original audit)

| Item | Resolution |
|---|---|
| Power distribution plan | RPI5 PD Power Hat selected — 40W cap, 8W headroom over 32W peak load |
| GPIO pin assignments | Fully confirmed — motors 10–15, encoders 39–42, I2C 8/9 |
| I2C SDA/SCL pins | GPIO 8 / 9 — confirmed working on bench |
| Encoder voltage level | 3.3V supply and output — no level shifter needed |
| RPLIDAR model | RPLidar A1 M8, CP2102 adapter (VID:10c4 PID:ea60), baud 115200 |
| USB port assignments on Pi | RPLidar → USB 2.0, RealSense → USB 3.0 (blue), ESP32 → any USB-A |
| Battery chemistry | 3S LiPo (12.6V full, 9.9V cutoff) — within hat's 9–24V range |
| INA219 shunt resistor | Adafruit breakout has onboard 0.1 Ω shunt (3.2 A max range) |
| RealSense → USB 3.0 required | Confirmed — must use blue port on Pi 5 |
| I2C pull-up resistors | Adafruit BNO055 + INA219 breakouts have onboard pull-ups |
| Encoder type | Quadrature (A + B) — direction detection confirmed in firmware |

---

## Open Items & Improvement Suggestions

### 1. GPIO 40/41 EMI — Hardware Fix Pending (MEDIUM)

**Known issue:** Left encoder A (GPIO 40) and B (GPIO 41) pick up 1 kHz PWM switching noise from the TB6612. Causes spurious encoder counts, especially at low speed.

**Current mitigation:** EMA velocity filter (`VEL_ALPHA = 0.2`) in firmware attenuates the noise.

**Permanent fix:** Solder 100 nF ceramic capacitors from GPIO 40 → GND and GPIO 41 → GND at the ESP32 headers. Not yet installed.

---

### 2. TB6612FNG Decoupling Capacitors (MEDIUM)

**Issue:** Motor switching creates voltage spikes on the VM rail that can corrupt I2C and destabilize the ESP32.

**Suggestion:** Add close to the TB6612 VM pin:
- 100 µF electrolytic (bulk bypass)
- 100 nF ceramic (high-frequency bypass)

Also add 100 nF ceramic across each motor terminal pair (AO1/AO2 and BO1/BO2).

---

### 3. Inline Fuse on Battery Positive (MEDIUM)

**Issue:** No onboard fuse on the RPI5 PD Power Hat. A motor stall or wiring short can deliver full battery current before anything trips.

**Suggestion:** Add an inline automotive or blade fuse on the battery positive wire before the hat's DC barrel input. Size to ~5–10 A above expected peak draw (~8 A for hat + motor bursts).

---

### 4. BME680 Thermal Placement (MEDIUM)

**Issue:** BME680 measures ambient temperature. If mounted near the ESP32 or TB6612, readings will read above ambient.

**Suggestion:** Mount with airflow access, away from the ESP32 and motor driver. I2C wires can be extended a short distance. Wire once the sensor's physical position is finalized.

---

### 5. BME680 I2C Pull-ups (LOW — confirm before wiring)

**Issue:** Unknown whether the specific BME680 breakout board has onboard pull-up resistors on SDA/SCL.

**Action:** Before wiring the BME680, confirm the breakout has onboard pull-ups. If not, add 4.7 kΩ to 3.3V on SDA and SCL (one pair for the whole bus — do not duplicate per device).

---

### 6. Star Ground — Physical Layout (LOW)

**Issue:** Common ground is logically correct but daisy-chaining ground connections in physical wiring can introduce noise.

**Suggestion:** Run all ground connections to one physical point (a ground bus bar or thick ground plane) rather than device-to-device. Especially important once motors are running under load.

---

## Summary Table

| Item | Priority | Status |
|---|---|---|
| GPIO 40/41 EMI — 100 nF caps | MEDIUM | Fix known, not yet installed |
| TB6612 decoupling capacitors | MEDIUM | Not yet installed |
| Inline fuse on battery positive | MEDIUM | Not yet installed |
| BME680 thermal placement | MEDIUM | BME680 not yet wired |
| BME680 I2C pull-ups | LOW | Confirm before wiring |
| Star ground physical layout | LOW | Physical build concern |
