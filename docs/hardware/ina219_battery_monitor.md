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

## Wiring — Confirmed Pin Assignments

**Breakout board:** Adafruit INA219 (product #904) — onboard 0.1 Ω precision shunt resistor, measures up to ±3.2A.

### Breakout Pinout

```
        Adafruit INA219 Breakout
   ┌─────────────────────────────┐
   │  VCC  │ Logic power 3–5V   │
   │  GND  │ Ground              │
   │  SDA  │ I2C data            │
   │  SCL  │ I2C clock           │
   │  VIN+ │ High-side + input   │──→ Battery +
   │  VIN− │ High-side − input   │──→ Load +
   └─────────────────────────────┘
```

Current flows **from VIN+ through the 0.1 Ω shunt to VIN−**. Place in series with the positive supply rail.

### Connection to ESP32-S3

| INA219 Pin | ESP32-S3 GPIO | Notes |
|---|---|---|
| VCC | 3V3 | |
| GND | GND | Shared common ground |
| SDA | GPIO 8 | Shared I2C bus with BNO055 |
| SCL | GPIO 9 | Shared I2C bus with BNO055 |
| A0, A1 | — | Not wired → address **0x40** |
| VIN+ | Battery positive rail | High-side current sense input |
| VIN− | Load positive (after shunt) | To TB6612 VM and Pi power |

I2C address: **0x40** (A0/A1 open = default).
Shares bus with: BNO055 (0x28) — confirmed simultaneously at 11.4V / ~50 mA on bench.

### Shunt Resistor

Onboard 0.1 Ω, 1% precision resistor. Max measurable current: **±3.2A** at default gain (0.8 mA resolution). No external shunt needed.

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
