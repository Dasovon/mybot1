# INA219 — Battery Monitor

## Role in This Project

The INA219 continuously measures battery voltage, current draw, and power consumption. This data is critical for:
- Battery safety cutoff (stop motors below voltage threshold)
- Low-battery warnings published to ROS
- Power budget monitoring during development

The battery monitor operates **independently of ROS, Wi-Fi, and the development PC**. The ESP32 must always know battery state.

---

## Key Specs

| Property | Value |
|---|---|
| Measurement | Voltage (bus), current (shunt), power |
| Bus voltage range | 0–26V |
| Current resolution | ~0.8 mA (at default gain) |
| Interface | I2C |
| I2C addresses | 0x40–0x4F (4 address pins) |
| Supply voltage | 3.0V – 5.5V |
| Shunt resistor | External (typically 0.1Ω for ≤3.2A) |

---

## Connection to ESP32-S3

| INA219 Pin | Connects To | Notes |
|---|---|---|
| VCC | 3.3V | Logic supply |
| GND | Common GND | Shared ground |
| SDA | ESP32 I2C SDA | 4.7 kΩ pull-up |
| SCL | ESP32 I2C SCL | 4.7 kΩ pull-up |
| VIN+ | Battery positive (after shunt) | High-side current sense |
| VIN- | Load side of shunt | To motor driver and Pi supply |

---

## I2C Address Configuration

| A1 | A0 | Address |
|---|---|---|
| GND | GND | 0x40 (default) |
| GND | VCC | 0x41 |
| VCC | GND | 0x44 |
| VCC | VCC | 0x45 |

---

## Shunt Resistor Selection

The shunt resistor value determines current measurement range:

| Shunt Resistance | Max Current (at 320mV) |
|---|---|
| 0.1Ω | 3.2A |
| 0.05Ω | 6.4A |
| 0.01Ω | 32A |

Choose shunt value based on expected peak current draw. Use low-inductance current sense resistors.

---

## Battery Safety Logic (ESP32 Firmware)

```
Every 200ms (5 Hz):
    Read voltage, current, power from INA219

    if voltage < WARNING_THRESHOLD:
        Transmit: BAT <v> <i> <p>  (triggers ROS warning)

    if voltage < CUTOFF_THRESHOLD:
        Immediately stop all motors (STBY pin LOW or zero PWM)
        Transmit: ERR BATTERY_CUTOFF
```

Suggested thresholds (adjust for your battery chemistry):

| Battery Type | Warning | Cutoff |
|---|---|---|
| 2S LiPo (8.4V max) | 7.0V | 6.6V |
| 3S LiPo (12.6V max) | 10.5V | 9.9V |
| 4S LiPo (16.8V max) | 14.0V | 13.2V |

---

## ROS Telemetry

The ESP32 transmits at 1–5 Hz:

```
BAT <voltage> <current> <power>
```

The `esp32_serial_bridge` node publishes this as `sensor_msgs/BatteryState` on `/battery_state`.

---

## Common Issues

| Symptom | Likely Cause |
|---|---|
| Voltage reads zero | VIN+ / VIN- swapped or not connected |
| Current reads negative | Shunt polarity reversed |
| I2C errors | Missing pull-ups or address conflict with other sensors |
| Incorrect current reading | Wrong shunt resistance configured in firmware |
