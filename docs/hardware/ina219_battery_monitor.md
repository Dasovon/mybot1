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

## Breakout Pinout

**Breakout board:** Adafruit INA219 (product #904) — onboard 0.1 Ω precision shunt resistor, measures up to ±3.2A. No external shunt needed.

| Pin | Description |
|---|---|
| VCC | Logic power 3–5V |
| GND | Ground |
| SDA | I2C data |
| SCL | I2C clock |
| VIN+ | High-side + input (connect toward battery +) |
| VIN− | High-side − input (connect toward load) |
| A0, A1 | Address select — both open/GND = **0x40** |

Current flows from VIN+ through the onboard 0.1 Ω shunt to VIN−. Place the INA219 in series with the positive supply rail. I2C address: **0x40**. Shares bus with BNO055 (0x28).

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

## Inductive Kickback Warning

> ⚠️ **From the INA219 datasheet:** When switching inductive loads, instantaneous voltage levels may greatly exceed steady-state levels due to inductive kickback. Chip damage can occur without protection.
>
> In this build, the INA219 monitors the battery/supply rail. The TB6612 motor driver is also on this rail. The **TB6612 has built-in kickback diodes** on its motor outputs, which suppress most kickback. However, rapid direction changes or PWM at high duty cycle can still cause transient voltage spikes on the supply rail.
>
> Mitigation: place a 100 µF electrolytic + 100 nF ceramic capacitor in parallel across the INA219 VIN+/VIN− input (on the battery side) to absorb transients.

---

## Common Issues

| Symptom | Likely Cause |
|---|---|
| Voltage reads zero | VIN+ / VIN- swapped or not connected |
| Current reads negative | Shunt polarity reversed |
| I2C errors | Missing pull-ups or address conflict with other sensors |
| Incorrect current reading | Wrong shunt resistance configured in firmware |
