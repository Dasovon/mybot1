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

## Connection to ESP32-S3

| BME680 Pin | Connects To | Notes |
|---|---|---|
| VCC | 3.3V | Logic and sensor supply |
| GND | Common GND | Shared ground |
| SDA | ESP32 I2C SDA | 4.7 kΩ pull-up |
| SCL | ESP32 I2C SCL | 4.7 kΩ pull-up |
| SDO | GND | Sets I2C address to 0x76 |
| CS | 3.3V or NC | Pull HIGH to force I2C mode |

---

## I2C Address

| SDO Pin | Address |
|---|---|
| GND | 0x76 (default) |
| VCC | 0x77 |

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
