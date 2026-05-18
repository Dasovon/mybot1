# Wiring Audit — Current State & Improvement Suggestions

Based strictly on `autonomous_robot_system_specification_v1.md`.

---

## What the Spec Defines

### Connection Map

```
Development PC
    ↕  Wi-Fi / Ethernet
Raspberry Pi 5
    ├── USB → ESP32-S3
    ├── USB → RPLIDAR
    └── USB → RealSense D435

ESP32-S3
    ├── I2C → BNO055 (IMU)
    ├── I2C → INA219 (battery)
    ├── I2C → BME680 (environment)
    ├── GPIO/PWM → TB6612FNG → Motors (left + right)
    └── GPIO (interrupts) → Wheel encoders (left + right)
```

### Common Ground (all share one ground rail)

```
Battery −
├── ESP32 GND
├── Pi GND
├── TB6612 GND
└── Sensor GND
```

### Motor Direction Logic (TB6612FNG)

| IN1 | IN2 | State |
|---|---|---|
| HIGH | LOW | Forward |
| LOW | HIGH | Reverse |
| HIGH | HIGH | Brake |
| LOW | LOW | Coast |

---

## What the Spec Does NOT Define

These are gaps that must be filled before wiring or writing firmware:

| Missing | Impact |
|---|---|
| Specific ESP32 GPIO pin assignments (motors, encoders) | Cannot write firmware |
| I2C SDA/SCL pin assignments | Cannot configure I2C |
| Encoder type (single-channel hall vs quadrature A/B) | Affects direction detection |
| Encoder supply voltage (3.3V or 5V) | Risk of damaging ESP32 if 5V encoders used without level shifter |
| RPLIDAR model (A1 / A2 / A3 / C1 / S1) | Affects driver config and baud rate |
| USB port assignment on Pi (which port for which device) | Affects udev rules |
| Battery chemistry and voltage (2S/3S/4S LiPo, Li-ion, etc.) | Safety cutoff thresholds depend on this |
| Power distribution (how battery → Pi 5V, ESP32 3.3V) | Fundamental — Pi 5 needs dedicated 5V/5A |
| Shunt resistor value for INA219 | Affects current measurement range |

---

## Audit Findings & Improvement Suggestions

### 1. Missing Power Distribution Plan (HIGH PRIORITY)

**Problem:** The spec shows no power architecture. A robot with this many components needs a clear plan.

**Suggested architecture:**

```
Battery (e.g., 3S LiPo, 11.1V)
├── VM pin → TB6612FNG (motor voltage, direct from battery)
├── → 5V/5A Buck Regulator → Raspberry Pi 5 (dedicated, do not share with motors)
├── → 3.3V or 5V regulator → ESP32-S3 (or USB from Pi if tethered during dev)
└── → 3.3V → I2C sensors (BNO055, INA219, BME680) and RPLIDAR (check model)
```

The Pi 5 draws up to 25W under load — it needs its own regulator, not a shared rail with motors.

---

### 2. Encoder Voltage Level Unspecified (HIGH PRIORITY)

**Problem:** Many DC motor encoder assemblies output 5V signals. The ESP32-S3 GPIO is 3.3V logic. Connecting 5V encoder signals directly can damage the ESP32.

**Suggestion:** Confirm encoder supply and output voltage. If 5V, add a level shifter (e.g., 74AHCT125 or simple resistor divider) on each encoder signal line before connecting to ESP32 GPIO.

---

### 3. No Pull-up Resistors Specified for I2C

**Problem:** The ESP32-S3 internal pull-ups are weak (~47 kΩ) and insufficient for a multi-device I2C bus at 400 kHz.

**Suggestion:** Add external pull-up resistors (4.7 kΩ to 3.3V) on SDA and SCL lines. One pair serves the entire bus — do not add per-device.

---

### 4. No Decoupling Capacitors Mentioned for TB6612FNG

**Problem:** Motor switching creates voltage spikes on the power rail that can corrupt I2C communication and ESP32 operation.

**Suggestion:** Add decoupling capacitors close to the TB6612FNG VM pin: 100 µF electrolytic (bulk) + 100 nF ceramic (bypass). Also add 100 nF across each motor terminal pair.

---

### 5. BME680 Thermal Proximity to Motors / ESP32

**Problem:** The BME680 measures ambient temperature, but if it is mounted close to the ESP32 (which self-heats) or near the motor driver, temperature readings will be inaccurate.

**Suggestion:** Mount the BME680 away from heat sources, ideally with airflow access. The I2C wires can be extended a short distance to accommodate this.

---

### 6. Single USB Connection ESP32 ↔ Pi (Acceptable but note the risk)

**Current:** ESP32 and Pi communicate via USB serial. This is correct for early development.

**Note:** USB CDC on ESP32-S3 can occasionally disconnect on Pi reboot or USB hub issues. The safety watchdog must handle this — if serial goes silent, the ESP32 stops motors. This is already called out in the spec and is correct.

**Future consideration:** If USB proves unreliable in a mobile vibration environment, UART via GPIO pins (with a fixed baud rate) is more robust than USB CDC.

---

### 7. RealSense D435 Requires USB 3.0

**Problem:** The spec says "USB" but does not specify USB 3.0. The D435 requires USB 3.0 bandwidth.

**Confirmed requirement:** Must use one of the Pi 5's USB 3.0 ports (blue ports). RPLIDAR can use USB 2.0.

---

### 8. No Fuse / Overcurrent Protection

**Problem:** If a motor stalls or wiring shorts, the battery can deliver very high current before any protection activates.

**Suggestion:** Add an inline fuse or polyfuse on the battery positive rail before the distribution point. Size to just above expected peak current draw.

---

### 9. Common Ground Path Needs Physical Layout Attention

**Problem:** The spec correctly calls for a common ground, but if ground runs are long or thin, ground loops and noise can still occur.

**Suggestion:** Use a star-ground topology — all grounds meet at one physical point (e.g., a ground plane or thick bus bar), not daisy-chained device to device.

---

## Summary Table

| Issue | Priority | Status |
|---|---|---|
| Power distribution plan | HIGH | Not in spec — needs design |
| Encoder voltage level | HIGH | TBD |
| I2C pull-up resistors | MEDIUM | Not in spec |
| TB6612 decoupling capacitors | MEDIUM | Not in spec |
| BME680 thermal placement | MEDIUM | Not in spec |
| RPLIDAR model selection | MEDIUM | TBD |
| RealSense D435 → USB 3.0 port | LOW (easy fix) | Confirm port assignment |
| Fuse / overcurrent protection | MEDIUM | Not in spec |
| Star ground topology | LOW | Physical layout concern |
