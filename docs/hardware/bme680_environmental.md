# BME680 — Environmental Sensor

## Role in This Project

The BME680 provides ambient environmental data published by the ESP32 as telemetry. This data enriches the robot's situational awareness and can be used for logging, environmental mapping, and detecting hazards (smoke, elevated VOCs).

---

## Key Specs

| Property | Value |
|---|---|
| Measurements | Temperature, humidity, barometric pressure, gas/VOC |
| Temperature range | -40°C to +85°C (±1.0°C accuracy) |
| Humidity range | 0–100% RH (±3% accuracy) |
| Pressure range | 300–1100 hPa (±0.6 hPa accuracy) |
| Gas sensor | Metal oxide (MOX) — measures air quality / VOC resistance |
| Interface | I2C or SPI |
| I2C address | 0x76 (default, SDO to GND) or 0x77 (SDO to VCC) |
| Supply voltage | 1.71V – 3.6V |
| Power consumption | ~2.1 µA in sleep mode |

---

## Breakout Pinout

**Breakout board:** Adafruit BME680 (product #3660) or equivalent. **Status: not yet wired — Phase 6.**

| Pin | Description |
|---|---|
| VIN | Power 3.3–5V |
| GND | Ground |
| SCK / SCL | I2C clock |
| SDI / SDA | I2C data |
| SDO | I2C address select (LOW = 0x76, HIGH = 0x77) |
| CS | SPI/I2C mode select — pull HIGH to force I2C |

Planned I2C address: **0x76** (SDO → GND). No conflict: BNO055=0x28, INA219=0x40, BME680=0x76. Will share the existing I2C bus.

> Mount away from the ESP32 and motor driver — self-heating skews temperature readings.

---

## Gas Sensor Notes

The BME680 gas sensor measures **resistance** of a heated metal oxide element. Higher resistance = cleaner air. Lower resistance = elevated VOCs / pollution.

- Requires a **heater profile** — the sensor heats an internal element to a target temperature (typically 320°C) for a set duration before sampling.
- The first reading after power-on may be inaccurate. Allow a **burn-in period** of ~5 minutes.
- Absolute VOC concentration (ppm) requires the BSEC library from Bosch for accurate IAQ (Indoor Air Quality) scoring.

---

## Firmware — Slow Loop (1–5 Hz)

The BME680 runs in the slow loop. Sample at 1 Hz to allow the heater profile to complete:

```cpp
// Forced mode: trigger one measurement, read result, sleep
bme.setTemperatureOversampling(BME680_OS_8X);
bme.setHumidityOversampling(BME680_OS_2X);
bme.setPressureOversampling(BME680_OS_4X);
bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
bme.setGasHeater(320, 150);  // 320°C for 150ms
```

---

## Telemetry Output

The ESP32 transmits at 1–5 Hz:

```
ENV <temp> <humidity> <pressure> <gas_resistance>
```

Example:

```
ENV 23.4 48.2 1008.6 114523
```

| Field | Unit |
|---|---|
| temp | °C |
| humidity | % RH |
| pressure | hPa |
| gas_resistance | Ω |

---

## ROS Topic

The `esp32_serial_bridge` publishes environmental data. Consider a custom `robot_msgs/EnvironmentalStatus` message or use `sensor_msgs/Temperature` and `sensor_msgs/RelativeHumidity` for standard fields.

---

## Common Issues

| Symptom | Likely Cause |
|---|---|
| Gas reads very low / zero | Heater not enabled or heater duration too short |
| Erratic temperature | Sensor near heat sources (motors, ESP32 itself) |
| I2C errors | Address conflict with BNO055 at 0x77 — check SDO wiring |
| Humidity always 100% | Condensation on sensor; allow ventilation |
