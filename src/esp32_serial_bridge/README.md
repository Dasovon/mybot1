# esp32_serial_bridge

Pi-side battery publisher and low-voltage protection node. Reads the INA219 directly over Pi I2C-1 and publishes `/battery_state` at 1 Hz. Also enforces a low-voltage cutoff by publishing zero `cmd_vel` and initiating OS shutdown when voltage drops below 9.9V.

## Nodes

### battery_publisher

Reads the INA219 (Pi I2C-1, addr 0x40) and publishes battery telemetry.

**Must be configured with explicit gain/range — default `configure()` causes 32.76V / NaN:**
```python
INA219(SHUNT_OHMS, max_expected_amps=3.0)
configure(voltage_range=RANGE_32V, gain=GAIN_8_320MV)
```

**Published topics:**

| Topic | Type | Rate | Notes |
|---|---|---|---|
| `/battery_state` | `sensor_msgs/BatteryState` | 1 Hz | Voltage, current, power |

**Parameters (`config/battery.yaml`):**

| Parameter | Type | Default | Description |
|---|---|---|---|
| `shunt_ohms` | float | `0.1` | Shunt resistor value |
| `max_expected_amps` | float | `3.0` | Required for correct gain selection |
| `low_voltage_cutoff` | float | `9.9` | Voltage threshold for cutoff action |
| `cutoff_recovery` | float | `10.2` | Voltage to exit cutoff (hysteresis) |

**Launch:**
```bash
ros2 run esp32_serial_bridge battery_publisher
```

Or via systemd: `sudo systemctl start mybot-battery.service`

## Architecture note

Battery monitoring is Pi-side only. The ESP32 does not own an INA219 and does not publish `/battery_state`. The low-voltage cutoff is a ROS publisher race (provisional Phase 3 behavior); priority arbitration via `twist_mux` is required before Nav2.

## Planned expansion (Phase 6+)

If the micro-ROS transport is replaced with a raw serial protocol, this package may also add a serial parsing node for BME680 environmental data over UART.
